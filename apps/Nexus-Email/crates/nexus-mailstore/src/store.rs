use chrono::Utc;
use sha2::{Digest, Sha256};
use sqlx::PgPool;
use uuid::Uuid;

use crate::address::Address;
use crate::error::{MailStoreError, Result};
use crate::models::*;

/// Bodies at or below this size are stored inline in Postgres; larger ones go
/// to object storage. 256 KiB covers the overwhelming majority of messages
/// that carry no attachment, so the common read is a single query, while the
/// bytes that actually make mail large never enter the database.
pub const INLINE_BODY_LIMIT: usize = 256 * 1024;

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
        self.create_mailbox(OwnerKind::Node, None, display_name).await
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
            sqlx::query(
                "INSERT INTO folders (id, mailbox_id, name, kind) VALUES ($1, $2, $3, $4)",
            )
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
        let row: Option<(Uuid,)> = sqlx::query_as(
            "SELECT mailbox_id FROM addresses WHERE localpart = $1 AND domain = $2",
        )
        .bind(&address.localpart)
        .bind(&address.domain)
        .fetch_optional(&self.pool)
        .await?;

        row.map(|r| r.0)
            .ok_or_else(|| MailStoreError::NoSuchAddress(address.as_string()))
    }

    pub async fn folder(&self, mailbox_id: Uuid, kind: FolderKind) -> Result<Uuid> {
        let row: (Uuid,) = sqlx::query_as(
            "SELECT id FROM folders WHERE mailbox_id = $1 AND kind = $2",
        )
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
            let found: Option<(Uuid,)> = sqlx::query_as(
                "SELECT thread_id FROM messages WHERE rfc822_msg_id = $1 LIMIT 1",
            )
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
        object_key: Option<&str>,
    ) -> Result<Uuid> {
        let hash = content_hash(raw);

        if let Some((existing,)) = sqlx::query_as::<_, (Uuid,)>(
            "SELECT id FROM messages WHERE content_hash = $1",
        )
        .bind(&hash)
        .fetch_optional(&self.pool)
        .await?
        {
            return Ok(existing);
        }

        // Over the inline limit the caller must have written the bytes to
        // object storage and passed the key. Refusing here rather than
        // silently inlining a huge body keeps the size policy in one place.
        let (inline, key): (Option<&[u8]>, Option<&str>) =
            if raw.len() <= INLINE_BODY_LIMIT && object_key.is_none() {
                (Some(raw), None)
            } else {
                (None, object_key)
            };

        let id = Uuid::now_v7();
        sqlx::query(
            "INSERT INTO messages (id, content_hash, rfc822_msg_id, thread_id, in_reply_to,
                                   references_ids, subject, from_address, transport,
                                   size_bytes, body_inline, body_object_key)
             VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12)",
        )
        .bind(id)
        .bind(&hash)
        .bind(rfc822_msg_id)
        .bind(thread_id)
        .bind(in_reply_to)
        .bind(references)
        .bind(subject)
        .bind(from.as_string())
        .bind(transport.as_str())
        .bind(raw.len() as i64)
        .bind(inline)
        .bind(key)
        .execute(&self.pool)
        .await?;

        sqlx::query("UPDATE threads SET last_activity_at = $1 WHERE id = $2")
            .bind(Utc::now())
            .bind(thread_id)
            .execute(&self.pool)
            .await?;

        Ok(id)
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
    pub async fn list_by_uid(
        &self,
        mailbox_id: Uuid,
        folder_id: Uuid,
    ) -> Result<Vec<ImapMessage>> {
        let rows: Vec<ImapRow> =
            sqlx::query_as(
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
            "answered" => "UPDATE mailbox_messages SET answered = $1 WHERE mailbox_id = $2 AND uid = $3",
            "flagged" => "UPDATE mailbox_messages SET flagged = $1 WHERE mailbox_id = $2 AND uid = $3",
            "draft" => "UPDATE mailbox_messages SET draft = $1 WHERE mailbox_id = $2 AND uid = $3",
            "deleted" => "UPDATE mailbox_messages SET deleted = $1 WHERE mailbox_id = $2 AND uid = $3",
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
type ImapRow = (i64, Uuid, i64, bool, bool, bool, bool, bool, chrono::DateTime<Utc>);

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
        let rows: Vec<SummaryRow> =
            sqlx::query_as(
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
        limit: i64,
    ) -> Result<Vec<MessageSummary>> {
        let rows: Vec<SummaryRow> =
            sqlx::query_as(
                "SELECT m.id, m.thread_id, m.subject, m.from_address, m.received_at,
                        mm.seen, mm.flagged, left(m.search_text, 200)
                 FROM mailbox_messages mm
                 JOIN messages m ON m.id = mm.message_id
                 WHERE mm.mailbox_id = $1
                   AND NOT mm.deleted
                   AND m.search_vector @@ plainto_tsquery('simple', $2)
                 ORDER BY ts_rank(m.search_vector, plainto_tsquery('simple', $2)) DESC,
                          m.received_at DESC
                 LIMIT $3",
            )
            .bind(mailbox_id)
            .bind(query)
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
        sqlx::query(
            "UPDATE mailbox_messages SET folder_id = $1
             WHERE mailbox_id = $2 AND message_id = $3",
        )
        .bind(folder_id)
        .bind(mailbox_id)
        .bind(message_id)
        .execute(&self.pool)
        .await?;
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
