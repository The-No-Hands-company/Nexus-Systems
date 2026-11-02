use chrono::{DateTime, Duration, Utc};
use sha2::{Digest, Sha256};
use sqlx::{PgPool, Postgres, Transaction};
use uuid::Uuid;

use crate::address::Address;
use crate::error::{MailStoreError, Result};
use crate::models::*;

/// Bodies at or below this size are stored inline in Postgres; larger ones go
/// to object storage. 256 KiB covers the overwhelming majority of messages
/// that carry no attachment, so the common read is a single query, while the
/// bytes that actually make mail large never enter the database.
pub const INLINE_BODY_LIMIT: usize = 256 * 1024;

/// A leased deletion of an object that held a message body or attachments.
///
/// The object key stays in the durable queue until the object-storage owner
/// confirms deletion. It is intentionally not logged by this crate.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ObjectDeletionJob {
    pub id: Uuid,
    pub generation_id: Uuid,
    pub object_key: String,
}

enum StoreReservationLock {
    Live { object_key: String },
    Finalized { message_id: Uuid },
}

pub fn content_hash(raw: &[u8]) -> String {
    hex::encode(Sha256::digest(raw))
}

/// Subject normalisation for thread grouping: strip any run of reply and
/// forward prefixes, collapse whitespace, casefold. `Re: Re: FW: Hello` and
/// `hello` belong to the same conversation.
pub fn normalise_subject(subject: &str) -> String {
    let mut s = subject.trim().to_lowercase();
    loop {
        let trimmed = s
            .strip_prefix("re:")
            .or_else(|| s.strip_prefix("fw:"))
            .or_else(|| s.strip_prefix("fwd:"))
            .map(str::trim_start);
        match trimmed {
            Some(t) => s = t.to_string(),
            None => break,
        }
    }
    s.split_whitespace().collect::<Vec<_>>().join(" ")
}

#[derive(Clone)]
pub struct MailStore {
    pool: PgPool,
}

impl MailStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn pool(&self) -> &PgPool {
        &self.pool
    }

    /// Create a mailbox owned by an ecosystem identity, with the standard
    /// special-use folders. A mailbox without an INBOX is not usable, so the
    /// folders are created in the same transaction rather than lazily.
    pub async fn create_identity_mailbox(
        &self,
        owner_subject: &str,
        display_name: &str,
    ) -> Result<Mailbox> {
        self.create_mailbox(OwnerKind::Identity, Some(owner_subject), display_name)
            .await
    }

    /// Create a node-owned mailbox, for role addresses like `info@`.
    pub async fn create_node_mailbox(&self, display_name: &str) -> Result<Mailbox> {
        self.create_mailbox(OwnerKind::Node, None, display_name)
            .await
    }

    async fn create_mailbox(
        &self,
        kind: OwnerKind,
        owner_subject: Option<&str>,
        display_name: &str,
    ) -> Result<Mailbox> {
        let mut tx = self.pool.begin().await?;
        let id = Uuid::now_v7();
        let created_at = Utc::now();

        sqlx::query(
            "INSERT INTO mailboxes (id, owner_subject, owner_kind, display_name, created_at)
             VALUES ($1, $2, $3, $4, $5)",
        )
        .bind(id)
        .bind(owner_subject)
        .bind(kind.as_str())
        .bind(display_name)
        .bind(created_at)
        .execute(&mut *tx)
        .await?;

        for (name, fk) in [
            ("INBOX", FolderKind::Inbox),
            ("Sent", FolderKind::Sent),
            ("Drafts", FolderKind::Drafts),
            ("Trash", FolderKind::Trash),
            ("Archive", FolderKind::Archive),
        ] {
            sqlx::query("INSERT INTO folders (id, mailbox_id, name, kind) VALUES ($1, $2, $3, $4)")
                .bind(Uuid::now_v7())
                .bind(id)
                .bind(name)
                .bind(fk.as_str())
                .execute(&mut *tx)
                .await?;
        }

        tx.commit().await?;
        Ok(Mailbox {
            id,
            owner_subject: owner_subject.map(str::to_string),
            owner_kind: kind,
            display_name: display_name.to_string(),
            created_at,
        })
    }

    /// Route an address to a mailbox. Many addresses may point at one mailbox,
    /// which is what makes aliases and role addresses ordinary.
    pub async fn add_address(
        &self,
        mailbox_id: Uuid,
        address: &Address,
        primary: bool,
    ) -> Result<()> {
        sqlx::query(
            "INSERT INTO addresses (id, localpart, domain, mailbox_id, is_primary)
             VALUES ($1, $2, $3, $4, $5)",
        )
        .bind(Uuid::now_v7())
        .bind(&address.localpart)
        .bind(&address.domain)
        .bind(mailbox_id)
        .bind(primary)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    /// Which mailbox, if any, receives mail for this address.
    pub async fn resolve(&self, address: &Address) -> Result<Uuid> {
        let row: Option<(Uuid,)> =
            sqlx::query_as("SELECT mailbox_id FROM addresses WHERE localpart = $1 AND domain = $2")
                .bind(&address.localpart)
                .bind(&address.domain)
                .fetch_optional(&self.pool)
                .await?;

        row.map(|r| r.0)
            .ok_or_else(|| MailStoreError::NoSuchAddress(address.as_string()))
    }

    pub async fn folder(&self, mailbox_id: Uuid, kind: FolderKind) -> Result<Uuid> {
        let row: (Uuid,) =
            sqlx::query_as("SELECT id FROM folders WHERE mailbox_id = $1 AND kind = $2")
                .bind(mailbox_id)
                .bind(kind.as_str())
                .fetch_one(&self.pool)
                .await?;
        Ok(row.0)
    }

    /// Find the thread a message belongs to, or start one.
    ///
    /// Header references win over the subject: `In-Reply-To` and `References`
    /// are what the sending client actually asserted, whereas subject matching
    /// is a heuristic that merges unrelated conversations sharing a common
    /// subject like "hello".
    pub async fn thread_for(
        &self,
        in_reply_to: Option<&str>,
        references: &[String],
        subject: Option<&str>,
    ) -> Result<Uuid> {
        let mut candidates: Vec<&str> = Vec::new();
        if let Some(irt) = in_reply_to {
            candidates.push(irt);
        }
        candidates.extend(references.iter().map(String::as_str));

        for msg_id in candidates {
            let found: Option<(Uuid,)> =
                sqlx::query_as("SELECT thread_id FROM messages WHERE rfc822_msg_id = $1 LIMIT 1")
                    .bind(msg_id)
                    .fetch_optional(&self.pool)
                    .await?;
            if let Some((thread_id,)) = found {
                return Ok(thread_id);
            }
        }

        let normalised = normalise_subject(subject.unwrap_or_default());
        let id = Uuid::now_v7();
        sqlx::query(
            "INSERT INTO threads (id, subject_normalised, last_activity_at) VALUES ($1, $2, $3)",
        )
        .bind(id)
        .bind(&normalised)
        .bind(Utc::now())
        .execute(&self.pool)
        .await?;
        Ok(id)
    }
}

impl MailStore {
    pub async fn reserve_external_body(
        &self,
        now: DateTime<Utc>,
    ) -> Result<ExternalBodyReservation> {
        let generation_id = Uuid::now_v7();
        let object_key = format!("mail-bodies/{generation_id}");
        let expires_at = now + Duration::minutes(15);
        sqlx::query(
            "INSERT INTO object_generations
             (id, object_key, lifecycle_state, created_at, reservation_expires_at)
             VALUES ($1, $2, 'reserved', $3, $4)",
        )
        .bind(generation_id)
        .bind(&object_key)
        .bind(now)
        .bind(expires_at)
        .execute(&self.pool)
        .await?;
        Ok(ExternalBodyReservation {
            generation_id,
            object_key,
            expires_at,
        })
    }

    /// Store a message once, content-addressed.
    ///
    /// Returns the existing id if this exact content is already stored: the
    /// same message delivered to five recipients is one row, and a retried
    /// delivery must not duplicate it.
    #[allow(clippy::too_many_arguments)]
    pub async fn store_message(
        &self,
        raw: &[u8],
        thread_id: Uuid,
        from: &Address,
        subject: Option<&str>,
        rfc822_msg_id: Option<&str>,
        in_reply_to: Option<&str>,
        references: &[String],
        transport: Transport,
        reservation: Option<&ExternalBodyReservation>,
    ) -> Result<Uuid> {
        if raw.len() > INLINE_BODY_LIMIT && reservation.is_none() {
            return Err(MailStoreError::ExternalBodyReservationRequired);
        }

        let hash = content_hash(raw);
        let size_bytes = raw.len() as i64;
        let from_address = from.as_string();
        let now = Utc::now();
        let mut tx = self.pool.begin().await?;

        let mut reserved_key: Option<String> = None;
        let mut generation_id: Option<Uuid> = None;
        if let Some(reservation) = reservation {
            match Self::lock_store_message_reservation(
                &mut tx,
                reservation,
                now,
                &hash,
                size_bytes,
                thread_id,
                &from_address,
                subject,
                rfc822_msg_id,
                in_reply_to,
                references,
                transport,
            )
            .await?
            {
                StoreReservationLock::Live { object_key } => {
                    reserved_key = Some(object_key);
                    generation_id = Some(reservation.generation_id);
                }
                StoreReservationLock::Finalized { message_id } => {
                    tx.commit().await?;
                    return Ok(message_id);
                }
            }
        }

        if let Some((existing,)) =
            sqlx::query_as::<_, (Uuid,)>("SELECT id FROM messages WHERE content_hash = $1")
                .bind(&hash)
                .fetch_optional(&mut *tx)
                .await?
        {
            if let (Some(generation_id), Some(object_key)) =
                (generation_id, reserved_key.as_deref())
            {
                Self::queue_reserved_generation_cleanup(&mut tx, generation_id, object_key, now)
                    .await?;
            }
            tx.commit().await?;
            return Ok(existing);
        }

        let id = Uuid::now_v7();
        let inline = if reserved_key.is_some() {
            None
        } else {
            Some(raw)
        };
        sqlx::query(
            "INSERT INTO messages (id, content_hash, rfc822_msg_id, thread_id, in_reply_to,
                                   references_ids, subject, from_address, transport,
                                   size_bytes, body_inline, body_object_key, body_object_generation_id)
             VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13)",
        )
        .bind(id)
        .bind(&hash)
        .bind(rfc822_msg_id)
        .bind(thread_id)
        .bind(in_reply_to)
        .bind(references)
        .bind(subject)
        .bind(&from_address)
        .bind(transport.as_str())
        .bind(size_bytes)
        .bind(inline)
        .bind(reserved_key.as_deref())
        .bind(generation_id)
        .execute(&mut *tx)
        .await?;

        if let (Some(generation_id), Some(object_key)) = (generation_id, reserved_key.as_deref()) {
            let updated = sqlx::query(
                "UPDATE object_generations
                 SET lifecycle_state = 'referenced',
                     reservation_expires_at = NULL,
                     finalized_message_id = $2
                 WHERE id = $1
                   AND object_key = $3
                   AND lifecycle_state = 'reserved'
                   AND finalized_message_id IS NULL",
            )
            .bind(generation_id)
            .bind(id)
            .bind(object_key)
            .execute(&mut *tx)
            .await?;
            if updated.rows_affected() != 1 {
                return Err(MailStoreError::ExternalBodyReservationFinalized);
            }
        }

        sqlx::query("UPDATE threads SET last_activity_at = $1 WHERE id = $2")
            .bind(Utc::now())
            .bind(thread_id)
            .execute(&mut *tx)
            .await?;

        tx.commit().await?;
        Ok(id)
    }

    pub async fn abandon_external_body_reservation(
        &self,
        reservation: &ExternalBodyReservation,
        now: DateTime<Utc>,
    ) -> Result<()> {
        let mut tx = self.pool.begin().await?;
        let object_key =
            Self::lock_live_external_body_reservation(&mut tx, reservation, now).await?;
        Self::queue_reserved_generation_cleanup(
            &mut tx,
            reservation.generation_id,
            &object_key,
            now,
        )
        .await?;
        tx.commit().await?;
        Ok(())
    }

    #[allow(clippy::too_many_arguments)]
    async fn lock_store_message_reservation(
        tx: &mut Transaction<'_, Postgres>,
        reservation: &ExternalBodyReservation,
        now: DateTime<Utc>,
        hash: &str,
        size_bytes: i64,
        thread_id: Uuid,
        from_address: &str,
        subject: Option<&str>,
        rfc822_msg_id: Option<&str>,
        in_reply_to: Option<&str>,
        references: &[String],
        transport: Transport,
    ) -> Result<StoreReservationLock> {
        let row: Option<(String, String, Option<DateTime<Utc>>, Option<Uuid>)> = sqlx::query_as(
            "SELECT object_key, lifecycle_state, reservation_expires_at, finalized_message_id
             FROM object_generations
             WHERE id = $1
             FOR UPDATE",
        )
        .bind(reservation.generation_id)
        .fetch_optional(&mut **tx)
        .await?;
        let Some((object_key, lifecycle_state, reservation_expires_at, finalized_message_id)) = row
        else {
            return Err(MailStoreError::ExternalBodyReservationMissing);
        };
        if object_key != reservation.object_key {
            return Err(MailStoreError::ExternalBodyReservationUnavailable);
        }

        if let Some(message_id) = finalized_message_id {
            let matches = Self::finalized_retry_matches_original_message(
                tx,
                reservation,
                message_id,
                hash,
                size_bytes,
                thread_id,
                from_address,
                subject,
                rfc822_msg_id,
                in_reply_to,
                references,
                transport,
            )
            .await?;
            if matches {
                return Ok(StoreReservationLock::Finalized { message_id });
            }
            return Err(MailStoreError::ExternalBodyReservationUnavailable);
        }

        if lifecycle_state != "reserved" {
            return Err(MailStoreError::ExternalBodyReservationUnavailable);
        }

        let Some(expires_at) = reservation_expires_at else {
            return Err(MailStoreError::ExternalBodyReservationUnavailable);
        };
        if expires_at <= now {
            return Err(MailStoreError::ExternalBodyReservationExpired);
        }

        Ok(StoreReservationLock::Live { object_key })
    }

    #[allow(clippy::too_many_arguments)]
    async fn finalized_retry_matches_original_message(
        tx: &mut Transaction<'_, Postgres>,
        reservation: &ExternalBodyReservation,
        message_id: Uuid,
        hash: &str,
        size_bytes: i64,
        thread_id: Uuid,
        from_address: &str,
        subject: Option<&str>,
        rfc822_msg_id: Option<&str>,
        in_reply_to: Option<&str>,
        references: &[String],
        transport: Transport,
    ) -> Result<bool> {
        let row: Option<(
            String,
            Uuid,
            String,
            Option<String>,
            Option<String>,
            Option<String>,
            Vec<String>,
            String,
            i64,
            Option<String>,
            Option<Uuid>,
        )> = sqlx::query_as(
            "SELECT content_hash, thread_id, from_address, subject, rfc822_msg_id, in_reply_to,
                    references_ids, transport, size_bytes, body_object_key, body_object_generation_id
             FROM messages
             WHERE id = $1",
        )
        .bind(message_id)
        .fetch_optional(&mut **tx)
        .await?;

        let Some((
            content_hash,
            persisted_thread_id,
            persisted_from_address,
            persisted_subject,
            persisted_rfc822_msg_id,
            persisted_in_reply_to,
            persisted_references,
            persisted_transport,
            persisted_size_bytes,
            persisted_object_key,
            persisted_generation_id,
        )) = row
        else {
            return Ok(false);
        };

        Ok(content_hash == hash
            && persisted_thread_id == thread_id
            && persisted_from_address == from_address
            && persisted_subject.as_deref() == subject
            && persisted_rfc822_msg_id.as_deref() == rfc822_msg_id
            && persisted_in_reply_to.as_deref() == in_reply_to
            && persisted_references == references
            && persisted_transport == transport.as_str()
            && persisted_size_bytes == size_bytes
            && persisted_object_key.as_deref() == Some(reservation.object_key.as_str())
            && persisted_generation_id == Some(reservation.generation_id))
    }

    async fn lock_live_external_body_reservation(
        tx: &mut Transaction<'_, Postgres>,
        reservation: &ExternalBodyReservation,
        now: DateTime<Utc>,
    ) -> Result<String> {
        let row: Option<(String, String, Option<DateTime<Utc>>, Option<Uuid>)> = sqlx::query_as(
            "SELECT object_key, lifecycle_state, reservation_expires_at, finalized_message_id
             FROM object_generations
             WHERE id = $1
             FOR UPDATE",
        )
        .bind(reservation.generation_id)
        .fetch_optional(&mut **tx)
        .await?;
        let Some((object_key, lifecycle_state, reservation_expires_at, finalized_message_id)) = row
        else {
            return Err(MailStoreError::ExternalBodyReservationMissing);
        };
        if object_key != reservation.object_key {
            return Err(MailStoreError::ExternalBodyReservationUnavailable);
        }
        if finalized_message_id.is_some() {
            return Err(MailStoreError::ExternalBodyReservationFinalized);
        }
        if lifecycle_state != "reserved" {
            return match lifecycle_state.as_str() {
                "referenced" => Err(MailStoreError::ExternalBodyReservationFinalized),
                _ => Err(MailStoreError::ExternalBodyReservationUnavailable),
            };
        }
        let Some(expires_at) = reservation_expires_at else {
            return Err(MailStoreError::ExternalBodyReservationUnavailable);
        };
        if expires_at <= now {
            return Err(MailStoreError::ExternalBodyReservationExpired);
        }
        Ok(object_key)
    }

    async fn queue_reserved_generation_cleanup(
        tx: &mut Transaction<'_, Postgres>,
        generation_id: Uuid,
        object_key: &str,
        now: DateTime<Utc>,
    ) -> Result<()> {
        let queued: Option<(Uuid,)> = sqlx::query_as(
            "UPDATE object_generations
             SET lifecycle_state = 'cleanup_queued',
                 reservation_expires_at = NULL
             WHERE id = $1
               AND object_key = $2
               AND lifecycle_state = 'reserved'
               AND finalized_message_id IS NULL
             RETURNING id",
        )
        .bind(generation_id)
        .bind(object_key)
        .fetch_optional(&mut **tx)
        .await?;

        if queued.is_none() {
            return Err(MailStoreError::ExternalBodyReservationUnavailable);
        }

        Self::enqueue_object_deletion_job(tx, generation_id, object_key, now).await
    }

    async fn enqueue_object_deletion_job(
        tx: &mut Transaction<'_, Postgres>,
        generation_id: Uuid,
        object_key: &str,
        now: DateTime<Utc>,
    ) -> Result<()> {
        sqlx::query(
            "INSERT INTO object_deletion_queue
             (id, generation_id, object_key, next_attempt_at, created_at)
             VALUES ($1, $2, $3, $4, $4)
             ON CONFLICT DO NOTHING",
        )
        .bind(Uuid::now_v7())
        .bind(generation_id)
        .bind(object_key)
        .bind(now)
        .execute(&mut **tx)
        .await?;
        Ok(())
    }

    /// Put an already-stored message into a mailbox's folder.
    ///
    /// This is delivery. The message itself is untouched and unduplicated;
    /// only the membership row is new, which is why one message can sit in
    /// many mailboxes with independent read state.
    pub async fn deliver(&self, mailbox_id: Uuid, message_id: Uuid, folder_id: Uuid) -> Result<()> {
        let mut tx = self.pool.begin().await?;

        // Claim the next UID under a row lock. Two concurrent deliveries to one
        // mailbox reading uid_next before either writes would hand out the same
        // UID twice — and a duplicate UID makes every IMAP client show the
        // wrong message, silently, from its cache.
        let (uid,): (i64,) = sqlx::query_as(
            "UPDATE mailboxes SET uid_next = uid_next + 1 WHERE id = $1 RETURNING uid_next - 1",
        )
        .bind(mailbox_id)
        .fetch_one(&mut *tx)
        .await?;

        sqlx::query(
            "INSERT INTO mailbox_messages (mailbox_id, message_id, folder_id, uid)
             VALUES ($1, $2, $3, $4)
             ON CONFLICT (mailbox_id, message_id) DO NOTHING",
        )
        .bind(mailbox_id)
        .bind(message_id)
        .bind(folder_id)
        .bind(uid)
        .execute(&mut *tx)
        .await?;

        // The UID is spent even if the insert was a no-op. Rolling it back
        // would let a later message reuse it, and IMAP forbids reuse outright.
        tx.commit().await?;
        Ok(())
    }

    /// What IMAP needs to open a folder.
    pub async fn mailbox_state(&self, mailbox_id: Uuid) -> Result<(i64, i64)> {
        let row: (i64, i64) =
            sqlx::query_as("SELECT uid_validity, uid_next FROM mailboxes WHERE id = $1")
                .bind(mailbox_id)
                .fetch_one(&self.pool)
                .await?;
        Ok(row)
    }

    /// Messages in a folder, by UID, for IMAP.
    pub async fn list_by_uid(&self, mailbox_id: Uuid, folder_id: Uuid) -> Result<Vec<ImapMessage>> {
        let rows: Vec<ImapRow> = sqlx::query_as(
            "SELECT mm.uid, m.id, m.size_bytes, mm.seen, mm.answered, mm.flagged,
                        mm.draft, mm.deleted, m.received_at
                 FROM mailbox_messages mm
                 JOIN messages m ON m.id = mm.message_id
                 WHERE mm.mailbox_id = $1 AND mm.folder_id = $2
                 ORDER BY mm.uid",
        )
        .bind(mailbox_id)
        .bind(folder_id)
        .fetch_all(&self.pool)
        .await?;

        Ok(rows
            .into_iter()
            .map(
                |(uid, message_id, size, seen, answered, flagged, draft, deleted, received_at)| {
                    ImapMessage {
                        uid,
                        message_id,
                        size,
                        received_at,
                        flags: Flags {
                            seen,
                            answered,
                            flagged,
                            draft,
                            deleted,
                            keywords: Vec::new(),
                        },
                    }
                },
            )
            .collect())
    }

    /// Set a system flag on a message by UID.
    pub async fn set_flag_by_uid(
        &self,
        mailbox_id: Uuid,
        uid: i64,
        flag: &str,
        value: bool,
    ) -> Result<()> {
        // The column name is chosen from a fixed set, never interpolated from
        // client input — an IMAP STORE command carries arbitrary flag names.
        let sql = match flag {
            "seen" => "UPDATE mailbox_messages SET seen = $1 WHERE mailbox_id = $2 AND uid = $3",
            "answered" => {
                "UPDATE mailbox_messages SET answered = $1 WHERE mailbox_id = $2 AND uid = $3"
            }
            "flagged" => {
                "UPDATE mailbox_messages SET flagged = $1 WHERE mailbox_id = $2 AND uid = $3"
            }
            "draft" => "UPDATE mailbox_messages SET draft = $1 WHERE mailbox_id = $2 AND uid = $3",
            "deleted" => {
                "UPDATE mailbox_messages SET deleted = $1 WHERE mailbox_id = $2 AND uid = $3"
            }
            _ => return Ok(()),
        };
        sqlx::query(sql)
            .bind(value)
            .bind(mailbox_id)
            .bind(uid)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    /// Raw bytes of a message identified by its UID in a mailbox.
    pub async fn raw_by_uid(&self, mailbox_id: Uuid, uid: i64) -> Result<Vec<u8>> {
        let row: Option<(Option<Vec<u8>>,)> = sqlx::query_as(
            "SELECT m.body_inline FROM mailbox_messages mm
             JOIN messages m ON m.id = mm.message_id
             WHERE mm.mailbox_id = $1 AND mm.uid = $2",
        )
        .bind(mailbox_id)
        .bind(uid)
        .fetch_optional(&self.pool)
        .await?;

        match row {
            Some((Some(bytes),)) => Ok(bytes),
            _ => Err(MailStoreError::NoSuchAddress(format!("uid {uid}"))),
        }
    }

    pub async fn set_seen(&self, mailbox_id: Uuid, message_id: Uuid, seen: bool) -> Result<()> {
        sqlx::query(
            "UPDATE mailbox_messages SET seen = $1 WHERE mailbox_id = $2 AND message_id = $3",
        )
        .bind(seen)
        .bind(mailbox_id)
        .bind(message_id)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    pub async fn unseen_count(&self, mailbox_id: Uuid) -> Result<i64> {
        let row: (i64,) = sqlx::query_as(
            "SELECT count(*) FROM mailbox_messages WHERE mailbox_id = $1 AND NOT seen",
        )
        .bind(mailbox_id)
        .fetch_one(&self.pool)
        .await?;
        Ok(row.0)
    }
}

/// One row of a summary query: the shape `list_folder` and `search` both
/// select. Named because the tuple is identical in both and unreadable inline.
type SummaryRow = (
    Uuid,
    Uuid,
    Option<String>,
    String,
    chrono::DateTime<Utc>,
    bool,
    bool,
    Option<String>,
);

fn to_summary(row: SummaryRow) -> MessageSummary {
    let (id, thread_id, subject, from_address, received_at, seen, flagged, snippet) = row;
    MessageSummary {
        id,
        thread_id,
        subject,
        from_address,
        received_at,
        seen,
        flagged,
        has_attachments: false,
        snippet,
    }
}

/// One row of the IMAP listing query. Named because the tuple is unreadable
/// inline and clippy is right to say so.
type ImapRow = (
    i64,
    Uuid,
    i64,
    bool,
    bool,
    bool,
    bool,
    bool,
    chrono::DateTime<Utc>,
);

/// A message as IMAP needs to see it.
#[derive(Debug, Clone)]
pub struct ImapMessage {
    pub uid: i64,
    pub message_id: Uuid,
    pub size: i64,
    pub received_at: chrono::DateTime<Utc>,
    pub flags: Flags,
}

/// A message as the mail UI needs to list it.
#[derive(Debug, Clone)]
pub struct MessageSummary {
    pub id: Uuid,
    pub thread_id: Uuid,
    pub subject: Option<String>,
    pub from_address: String,
    pub received_at: chrono::DateTime<Utc>,
    pub seen: bool,
    pub flagged: bool,
    pub has_attachments: bool,
    pub snippet: Option<String>,
}

impl MailStore {
    /// Record the displayable text of a message for search.
    ///
    /// Called by the delivery path, which has already parsed the MIME tree.
    /// The store deliberately does not parse messages itself — that would make
    /// storage depend on the message-format crate and give two places an
    /// opinion about what a message means.
    pub async fn set_search_text(&self, message_id: Uuid, text: &str) -> Result<()> {
        // Truncated: indexing an entire mail archive's worth of quoted replies
        // makes the index enormous and the results worse, since every message
        // in a thread then matches every term in it.
        let clipped: String = text.chars().take(16_000).collect();
        sqlx::query("UPDATE messages SET search_text = $1 WHERE id = $2")
            .bind(clipped)
            .bind(message_id)
            .execute(&self.pool)
            .await?;
        Ok(())
    }

    pub async fn folders(&self, mailbox_id: Uuid) -> Result<Vec<Folder>> {
        let rows: Vec<(Uuid, Uuid, String, String)> = sqlx::query_as(
            "SELECT id, mailbox_id, name, kind FROM folders WHERE mailbox_id = $1 ORDER BY name",
        )
        .bind(mailbox_id)
        .fetch_all(&self.pool)
        .await?;

        Ok(rows
            .into_iter()
            .map(|(id, mailbox_id, name, kind)| Folder {
                id,
                mailbox_id,
                name,
                kind: match kind.as_str() {
                    "inbox" => FolderKind::Inbox,
                    "sent" => FolderKind::Sent,
                    "drafts" => FolderKind::Drafts,
                    "trash" => FolderKind::Trash,
                    "archive" => FolderKind::Archive,
                    _ => FolderKind::Custom,
                },
            })
            .collect())
    }

    /// List a folder, newest first.
    pub async fn list_folder(
        &self,
        mailbox_id: Uuid,
        folder_id: Uuid,
        limit: i64,
        offset: i64,
    ) -> Result<Vec<MessageSummary>> {
        let rows: Vec<SummaryRow> = sqlx::query_as(
            "SELECT m.id, m.thread_id, m.subject, m.from_address, m.received_at,
                        mm.seen, mm.flagged, left(m.search_text, 200)
                 FROM mailbox_messages mm
                 JOIN messages m ON m.id = mm.message_id
                 WHERE mm.mailbox_id = $1 AND mm.folder_id = $2 AND NOT mm.deleted
                 ORDER BY m.received_at DESC
                 LIMIT $3 OFFSET $4",
        )
        .bind(mailbox_id)
        .bind(folder_id)
        .bind(limit.clamp(1, 200))
        .bind(offset.max(0))
        .fetch_all(&self.pool)
        .await?;

        Ok(rows.into_iter().map(to_summary).collect())
    }

    /// Search a mailbox.
    ///
    /// Scoped to one mailbox by joining membership, so a query can never
    /// surface a message the caller does not hold — the same message row is
    /// shared between mailboxes, and searching `messages` directly would leak
    /// across them.
    pub async fn search(
        &self,
        mailbox_id: Uuid,
        query: &str,
        folder_ids: &[Uuid],
        unread: Option<bool>,
        limit: i64,
    ) -> Result<Vec<MessageSummary>> {
        let rows: Vec<SummaryRow> = sqlx::query_as(
            "SELECT m.id, m.thread_id, m.subject, m.from_address, m.received_at,
                        mm.seen, mm.flagged, left(m.search_text, 200)
                 FROM mailbox_messages mm
                 JOIN messages m ON m.id = mm.message_id
                 WHERE mm.mailbox_id = $1
                   AND NOT mm.deleted
                   AND m.search_vector @@ plainto_tsquery('simple', $2)
                   AND mm.folder_id = ANY($3)
                   AND ($4::boolean IS NULL OR mm.seen = NOT $4::boolean)
                 ORDER BY ts_rank(m.search_vector, plainto_tsquery('simple', $2)) DESC,
                          m.received_at DESC
                 LIMIT $5",
        )
        .bind(mailbox_id)
        .bind(query)
        .bind(folder_ids)
        .bind(unread)
        .bind(limit.clamp(1, 100))
        .fetch_all(&self.pool)
        .await?;

        Ok(rows.into_iter().map(to_summary).collect())
    }

    /// The raw bytes of a stored message, for rendering or forwarding.
    pub async fn raw_message(&self, mailbox_id: Uuid, message_id: Uuid) -> Result<Vec<u8>> {
        // The membership join is the authorisation check: a message is only
        // readable through a mailbox that actually holds it.
        let row: Option<(Option<Vec<u8>>, Option<String>)> = sqlx::query_as(
            "SELECT m.body_inline, m.body_object_key
             FROM mailbox_messages mm
             JOIN messages m ON m.id = mm.message_id
             WHERE mm.mailbox_id = $1 AND mm.message_id = $2",
        )
        .bind(mailbox_id)
        .bind(message_id)
        .fetch_optional(&self.pool)
        .await?;

        match row {
            Some((Some(bytes), _)) => Ok(bytes),
            // Object-storage bodies are fetched by the caller, which owns the
            // storage client; the store deliberately does not open network
            // connections.
            Some((None, Some(key))) => Err(MailStoreError::NoSuchAddress(format!(
                "message body is in object storage at {key}"
            ))),
            _ => Err(MailStoreError::NoSuchAddress(message_id.to_string())),
        }
    }

    /// The folder currently containing a mailbox's message.
    ///
    /// The membership join scopes this lookup to the caller's mailbox, just as
    /// `raw_message` does, so exposing folder metadata cannot reveal another
    /// mailbox's placement.
    pub async fn message_folder(
        &self,
        mailbox_id: Uuid,
        message_id: Uuid,
    ) -> Result<Option<Folder>> {
        let row: Option<(Uuid, Uuid, String, String)> = sqlx::query_as(
            "SELECT f.id, f.mailbox_id, f.name, f.kind
             FROM mailbox_messages mm
             JOIN folders f ON f.id = mm.folder_id
             WHERE mm.mailbox_id = $1 AND mm.message_id = $2",
        )
        .bind(mailbox_id)
        .bind(message_id)
        .fetch_optional(&self.pool)
        .await?;

        Ok(row.map(|(id, mailbox_id, name, kind)| Folder {
            id,
            mailbox_id,
            name,
            kind: match kind.as_str() {
                "inbox" => FolderKind::Inbox,
                "sent" => FolderKind::Sent,
                "drafts" => FolderKind::Drafts,
                "trash" => FolderKind::Trash,
                "archive" => FolderKind::Archive,
                _ => FolderKind::Custom,
            },
        }))
    }

    /// The address a mailbox sends as.
    ///
    /// Falls back to any address it owns when none is marked primary, because
    /// a mailbox with addresses but no primary flag should still be able to
    /// send — and the alternative is composing From out of an internal user id,
    /// which is what this replaced.
    pub async fn primary_address(&self, mailbox_id: Uuid) -> Result<Option<Address>> {
        let row: Option<(String, String)> = sqlx::query_as(
            "SELECT localpart, domain FROM addresses
             WHERE mailbox_id = $1
             ORDER BY is_primary DESC, created_at
             LIMIT 1",
        )
        .bind(mailbox_id)
        .fetch_optional(&self.pool)
        .await?;
        Ok(row.map(|(localpart, domain)| Address { localpart, domain }))
    }

    /// Every message of a thread that this mailbox holds.
    ///
    /// Scoped by membership like every other read: a thread can contain
    /// messages delivered to several people, and only the ones in the caller's
    /// own mailbox are theirs to see. Querying by thread_id alone would leak
    /// the rest of the conversation.
    pub async fn thread_messages(
        &self,
        mailbox_id: Uuid,
        thread_id: Uuid,
    ) -> Result<Vec<MessageSummary>> {
        let rows: Vec<SummaryRow> = sqlx::query_as(
            "SELECT m.id, m.thread_id, m.subject, m.from_address, m.received_at,
                    mm.seen, mm.flagged, left(m.search_text, 200)
             FROM mailbox_messages mm
             JOIN messages m ON m.id = mm.message_id
             WHERE mm.mailbox_id = $1 AND m.thread_id = $2 AND NOT mm.deleted
             ORDER BY m.received_at",
        )
        .bind(mailbox_id)
        .bind(thread_id)
        .fetch_all(&self.pool)
        .await?;
        Ok(rows.into_iter().map(to_summary).collect())
    }

    /// A folder by name, created if it does not exist.
    ///
    /// Used for Junk, which is not one of the special-use folders every
    /// mailbox is born with — quarantine is rare enough that creating it on
    /// first use beats giving every mailbox a folder most will never see.
    pub async fn folder_named_or_create(
        &self,
        mailbox_id: Uuid,
        name: &str,
        kind: FolderKind,
    ) -> Result<Uuid> {
        if let Some((id,)) = sqlx::query_as::<_, (Uuid,)>(
            "SELECT id FROM folders WHERE mailbox_id = $1 AND name = $2",
        )
        .bind(mailbox_id)
        .bind(name)
        .fetch_optional(&self.pool)
        .await?
        {
            return Ok(id);
        }

        let id = Uuid::now_v7();
        sqlx::query(
            "INSERT INTO folders (id, mailbox_id, name, kind) VALUES ($1, $2, $3, $4)
             ON CONFLICT (mailbox_id, name) DO NOTHING",
        )
        .bind(id)
        .bind(mailbox_id)
        .bind(name)
        .bind(kind.as_str())
        .execute(&self.pool)
        .await?;

        // Re-read rather than trusting the insert: a concurrent delivery may
        // have created it first, in which case our id was never used.
        let (found,): (Uuid,) =
            sqlx::query_as("SELECT id FROM folders WHERE mailbox_id = $1 AND name = $2")
                .bind(mailbox_id)
                .bind(name)
                .fetch_one(&self.pool)
                .await?;
        Ok(found)
    }

    /// Move a message already in a mailbox to another of its folders.
    pub async fn move_to_folder(
        &self,
        mailbox_id: Uuid,
        message_id: Uuid,
        folder_id: Uuid,
    ) -> Result<()> {
        self.move_to_folder_at(mailbox_id, message_id, folder_id, Utc::now())
            .await
    }

    /// Move a message to a folder, recording a seven-day expiry only in Trash.
    pub async fn move_to_folder_at(
        &self,
        mailbox_id: Uuid,
        message_id: Uuid,
        folder_id: Uuid,
        moved_at: DateTime<Utc>,
    ) -> Result<()> {
        sqlx::query(
            "UPDATE mailbox_messages
             SET folder_id = $1,
                 trash_expires_at = CASE
                     WHEN (SELECT kind FROM folders WHERE id = $1 AND mailbox_id = $2) = 'trash'
                         THEN $4 + interval '7 days'
                     ELSE NULL
                 END
             WHERE mailbox_id = $2 AND message_id = $3",
        )
        .bind(folder_id)
        .bind(mailbox_id)
        .bind(message_id)
        .bind(moved_at)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    /// Erase one mailbox's membership and delete shared content only after the
    /// final membership is gone.
    pub async fn erase_membership(&self, mailbox_id: Uuid, message_id: Uuid) -> Result<()> {
        let mut tx = self.pool.begin().await?;

        let deleted: Option<(Uuid,)> = sqlx::query_as(
            "DELETE FROM mailbox_messages
             WHERE mailbox_id = $1 AND message_id = $2
             RETURNING message_id",
        )
        .bind(mailbox_id)
        .bind(message_id)
        .fetch_optional(&mut *tx)
        .await?;

        if deleted.is_some() {
            Self::erase_orphaned_message(&mut tx, message_id).await?;
        }

        tx.commit().await?;
        Ok(())
    }

    /// Delete one membership only if it is still in this mailbox's Trash.
    ///
    /// Empty Trash first obtains a display batch, but the folder predicate
    /// here is authoritative. A restore between the listing and deletion is
    /// therefore retained.
    pub async fn erase_trash_membership(&self, mailbox_id: Uuid, message_id: Uuid) -> Result<bool> {
        let mut tx = self.pool.begin().await?;
        let deleted: Option<(Uuid,)> = sqlx::query_as(
            "DELETE FROM mailbox_messages mm
             USING folders f
             WHERE mm.mailbox_id = $1
               AND mm.message_id = $2
               AND mm.folder_id = f.id
               AND f.mailbox_id = mm.mailbox_id
               AND f.kind = 'trash'
             RETURNING mm.message_id",
        )
        .bind(mailbox_id)
        .bind(message_id)
        .fetch_optional(&mut *tx)
        .await?;

        if deleted.is_some() {
            Self::erase_orphaned_message(&mut tx, message_id).await?;
        }

        tx.commit().await?;
        Ok(deleted.is_some())
    }

    /// Permanently erase expired Trash memberships in bounded batches.
    pub async fn purge_expired_trash(&self, now: DateTime<Utc>, limit: i64) -> Result<u64> {
        let rows: Vec<(Uuid, Uuid)> = sqlx::query_as(
            "SELECT mm.mailbox_id, mm.message_id
             FROM mailbox_messages mm
             JOIN folders f
               ON f.id = mm.folder_id
              AND f.mailbox_id = mm.mailbox_id
             WHERE f.kind = 'trash'
               AND mm.trash_expires_at IS NOT NULL
               AND mm.trash_expires_at <= $1
             ORDER BY mm.trash_expires_at
             LIMIT $2",
        )
        .bind(now)
        .bind(limit.clamp(1, 10_000))
        .fetch_all(&self.pool)
        .await?;

        let mut purged = 0_u64;
        for (mailbox_id, message_id) in rows {
            let mut tx = self.pool.begin().await?;
            let deleted: Option<(Uuid,)> = sqlx::query_as(
                "DELETE FROM mailbox_messages mm
                 USING folders f
                 WHERE mm.mailbox_id = $1
                   AND mm.message_id = $2
                   AND mm.folder_id = f.id
                   AND f.mailbox_id = mm.mailbox_id
                   AND f.kind = 'trash'
                   AND mm.trash_expires_at IS NOT NULL
                   AND mm.trash_expires_at <= $3
                 RETURNING mm.message_id",
            )
            .bind(mailbox_id)
            .bind(message_id)
            .bind(now)
            .fetch_optional(&mut *tx)
            .await?;

            if deleted.is_some() {
                Self::erase_orphaned_message(&mut tx, message_id).await?;
                purged += 1;
            }

            tx.commit().await?;
        }

        Ok(purged)
    }

    pub async fn queue_expired_external_body_reservations(
        &self,
        now: DateTime<Utc>,
        limit: i64,
    ) -> Result<u64> {
        let mut tx = self.pool.begin().await?;
        let rows: Vec<(Uuid, String)> = sqlx::query_as(
            "SELECT id, object_key
             FROM object_generations
             WHERE lifecycle_state = 'reserved'
               AND reservation_expires_at IS NOT NULL
               AND reservation_expires_at <= $1
               AND finalized_message_id IS NULL
             ORDER BY reservation_expires_at, created_at
             FOR UPDATE SKIP LOCKED
             LIMIT $2",
        )
        .bind(now)
        .bind(limit.clamp(1, 1_000))
        .fetch_all(&mut *tx)
        .await?;

        for (generation_id, object_key) in &rows {
            Self::queue_reserved_generation_cleanup(&mut tx, *generation_id, object_key, now)
                .await?;
        }

        tx.commit().await?;
        Ok(rows.len() as u64)
    }

    /// Lease queued external-body deletions to the object-storage owner.
    ///
    /// Leases recover automatically after five minutes, so a process crash
    /// cannot lose an object deletion. The caller must report success with
    /// `complete_object_deletion` or explicitly schedule a retry.
    pub async fn claim_object_deletion_jobs(
        &self,
        now: DateTime<Utc>,
        limit: i64,
    ) -> Result<Vec<ObjectDeletionJob>> {
        let rows = sqlx::query_as(
            "WITH candidates AS (
                 SELECT q.id
                 FROM object_deletion_queue q
                 JOIN object_generations g ON g.id = q.generation_id
                 WHERE q.next_attempt_at <= $1
                   AND (q.lease_expires_at IS NULL OR q.lease_expires_at <= $1)
                   AND g.lifecycle_state = 'cleanup_queued'
                   AND NOT EXISTS (
                       SELECT 1 FROM messages m
                       WHERE m.body_object_generation_id = q.generation_id
                   )
                 ORDER BY q.next_attempt_at, q.created_at
                 FOR UPDATE SKIP LOCKED
                 LIMIT $2
             ),
             claimed AS (
                 UPDATE object_deletion_queue q
                 SET lease_expires_at = $1 + interval '5 minutes',
                     attempts = q.attempts + 1
                 FROM candidates c
                 WHERE q.id = c.id
                 RETURNING q.id, q.generation_id, q.object_key
             )
             SELECT c.id, c.generation_id, c.object_key
             FROM claimed c
             JOIN object_generations g ON g.id = c.generation_id
             WHERE g.lifecycle_state = 'cleanup_queued'
               AND NOT EXISTS (
                   SELECT 1 FROM messages m
                   WHERE m.body_object_generation_id = c.generation_id
               )",
        )
        .bind(now)
        .bind(limit.clamp(1, 1_000))
        .fetch_all(&self.pool)
        .await?;

        Ok(rows
            .into_iter()
            .map(|(id, generation_id, object_key)| ObjectDeletionJob {
                id,
                generation_id,
                object_key,
            })
            .collect())
    }

    /// Mark an object deletion as confirmed by the object-storage owner.
    pub async fn complete_object_deletion(&self, job_id: Uuid, generation_id: Uuid) -> Result<()> {
        let mut tx = self.pool.begin().await?;
        let deleted: Option<(Uuid,)> = sqlx::query_as(
            "DELETE FROM object_deletion_queue
             WHERE id = $1 AND generation_id = $2
             RETURNING generation_id",
        )
        .bind(job_id)
        .bind(generation_id)
        .fetch_optional(&mut *tx)
        .await?;
        if let Some((generation_id,)) = deleted {
            sqlx::query(
                "UPDATE object_generations
                 SET lifecycle_state = 'deleted'
                 WHERE id = $1
                   AND lifecycle_state = 'cleanup_queued'
                   AND NOT EXISTS (
                       SELECT 1 FROM messages
                       WHERE body_object_generation_id = $1
                   )",
            )
            .bind(generation_id)
            .execute(&mut *tx)
            .await?;
        }
        tx.commit().await?;
        Ok(())
    }

    /// Release a failed deletion for retry without recording failure content.
    pub async fn retry_object_deletion(&self, job_id: Uuid, now: DateTime<Utc>) -> Result<()> {
        sqlx::query(
            "UPDATE object_deletion_queue
             SET lease_expires_at = NULL,
                 next_attempt_at = $2 + interval '1 minute'
             WHERE id = $1",
        )
        .bind(job_id)
        .bind(now)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    async fn erase_orphaned_message(
        tx: &mut Transaction<'_, Postgres>,
        message_id: Uuid,
    ) -> Result<()> {
        let deleted: Option<(Option<Uuid>,)> = sqlx::query_as(
            "DELETE FROM messages
             WHERE id = $1
               AND NOT EXISTS (
                 SELECT 1 FROM mailbox_messages WHERE message_id = $1
               )
             RETURNING body_object_generation_id",
        )
        .bind(message_id)
        .fetch_optional(&mut **tx)
        .await?;

        if let Some((Some(generation_id),)) = deleted {
            let generation: Option<(String,)> = sqlx::query_as(
                "UPDATE object_generations
                 SET lifecycle_state = 'cleanup_queued'
                 WHERE id = $1
                   AND lifecycle_state = 'referenced'
                   AND NOT EXISTS (
                       SELECT 1 FROM messages
                       WHERE body_object_generation_id = $1
                   )
                 RETURNING object_key",
            )
            .bind(generation_id)
            .fetch_optional(&mut **tx)
            .await?;
            if let Some((object_key,)) = generation {
                Self::enqueue_object_deletion_job(tx, generation_id, &object_key, Utc::now())
                    .await?;
            }
        }
        Ok(())
    }

    pub async fn mailbox_for_subject(&self, subject: &str) -> Result<Option<Uuid>> {
        let row: Option<(Uuid,)> =
            sqlx::query_as("SELECT id FROM mailboxes WHERE owner_subject = $1 LIMIT 1")
                .bind(subject)
                .fetch_optional(&self.pool)
                .await?;
        Ok(row.map(|r| r.0))
    }
}
