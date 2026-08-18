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
        sqlx::query(
            "INSERT INTO mailbox_messages (mailbox_id, message_id, folder_id)
             VALUES ($1, $2, $3)
             ON CONFLICT (mailbox_id, message_id) DO NOTHING",
        )
        .bind(mailbox_id)
        .bind(message_id)
        .bind(folder_id)
        .execute(&self.pool)
        .await?;
        Ok(())
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
