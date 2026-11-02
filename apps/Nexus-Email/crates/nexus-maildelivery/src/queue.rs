use chrono::{DateTime, Duration, Utc};
use nexus_mailstore::MailStore;
use sqlx::{PgPool, Postgres, Transaction};
use uuid::Uuid;

use crate::error::Result;
use crate::route::Route;

const BACKOFF_MINUTES: &[i64] = &[1, 5, 15, 60, 240, 720, 1440, 1440, 1440, 1440];

pub fn backoff_for(attempts: i32) -> Option<Duration> {
    BACKOFF_MINUTES
        .get(attempts as usize)
        .map(|m| Duration::minutes(*m))
}

pub fn max_attempts() -> i32 {
    BACKOFF_MINUTES.len() as i32
}

#[derive(Debug, Clone)]
pub struct ClaimedDelivery {
    pub queue_id: Uuid,
    pub message_id: Uuid,
    pub lease_token: Uuid,
    pub envelope_from: String,
    pub recipient: String,
    pub destination: String,
    pub route: String,
    pub attempts: i32,
}

pub type QueuedDelivery = ClaimedDelivery;

#[derive(Clone)]
pub struct Queue {
    pool: PgPool,
}

impl Queue {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub async fn enqueue(
        &self,
        message_id: Uuid,
        envelope_from: &str,
        recipient: &str,
        route: &Route,
    ) -> Result<()> {
        let mut tx = self.pool.begin().await?;
        self.enqueue_in_transaction(&mut tx, message_id, envelope_from, recipient, route)
            .await?;
        tx.commit().await?;
        Ok(())
    }

    pub async fn enqueue_in_transaction(
        &self,
        tx: &mut Transaction<'_, Postgres>,
        message_id: Uuid,
        envelope_from: &str,
        recipient: &str,
        route: &Route,
    ) -> Result<()> {
        if matches!(route, Route::Local) {
            return Ok(());
        }
        if !MailStore::lock_message_in_transaction(tx, message_id).await? {
            return Err(sqlx::Error::RowNotFound.into());
        }
        let existing: Option<(Uuid,)> = sqlx::query_as(
            "SELECT id FROM outbound_queue WHERE message_id = $1 AND recipient = $2 LIMIT 1",
        )
        .bind(message_id)
        .bind(recipient)
        .fetch_optional(&mut **tx)
        .await?;
        if let Some((id,)) = existing {
            let (valid,): (bool,) = sqlx::query_as(
                "SELECT EXISTS (SELECT 1 FROM mail_submission_recipients r
                 JOIN mail_submissions s ON s.id = r.submission_id
                 WHERE r.queue_id = $1 AND s.message_id = $2)",
            )
            .bind(id)
            .bind(message_id)
            .fetch_one(&mut **tx)
            .await?;
            if !valid {
                return Err(
                    sqlx::Error::Protocol("queue row has no recipient intent".into()).into(),
                );
            }
            return Ok(());
        }
        let submission = MailStore::ensure_submission_in_transaction(tx, message_id).await?;
        let id = self
            .enqueue_submission_in_transaction(tx, message_id, envelope_from, recipient, route)
            .await?;
        let route = match route {
            Route::Federated { .. } => "federated",
            _ => "smtp",
        };
        sqlx::query(
            "INSERT INTO mail_submission_recipients (id, submission_id, recipient, route, state, queue_id)
             VALUES ($1, $2, $3, $4, 'outbound_pending', $5)",
        ).bind(Uuid::now_v7()).bind(submission).bind(recipient).bind(route).bind(id)
            .execute(&mut **tx).await?;
        Ok(())
    }

    pub(crate) async fn enqueue_submission_in_transaction(
        &self,
        tx: &mut Transaction<'_, Postgres>,
        message_id: Uuid,
        envelope_from: &str,
        recipient: &str,
        route: &Route,
    ) -> Result<Uuid> {
        let (route_name, destination) = match route {
            Route::Federated { node } => ("federated", node.clone()),
            Route::External => (
                "smtp",
                recipient
                    .rsplit_once('@')
                    .map(|(_, d)| d.to_string())
                    .unwrap_or_default(),
            ),
            Route::Local => {
                return Err(sqlx::Error::Protocol("local delivery cannot be queued".into()).into())
            }
        };
        let id = Uuid::now_v7();
        sqlx::query(
            "INSERT INTO outbound_queue (id, message_id, envelope_from, recipient, destination, route)
             VALUES ($1, $2, $3, $4, $5, $6)",
        ).bind(id).bind(message_id).bind(envelope_from).bind(recipient).bind(destination)
            .bind(route_name).execute(&mut **tx).await?;
        Ok(id)
    }

    pub async fn claim_due(&self, limit: i64) -> Result<Vec<QueuedDelivery>> {
        let mut tx = self.pool.begin().await?;
        let rows: Vec<(Uuid, Uuid, String, String, String, String, i32)> = sqlx::query_as(
            "SELECT q.id, q.message_id, q.envelope_from, q.recipient, q.destination, q.route, q.attempts
             FROM outbound_queue q
             JOIN mail_submission_recipients r ON r.queue_id = q.id
             JOIN mail_submissions s ON s.id = r.submission_id AND s.message_id = q.message_id
             WHERE r.state = 'outbound_pending'
               AND ((q.state = 'pending' AND q.next_attempt_at <= now())
                    OR (q.state = 'delivering' AND q.lease_expires_at <= now()))
             ORDER BY q.next_attempt_at, q.id LIMIT $1 FOR UPDATE OF q SKIP LOCKED",
        ).bind(limit.max(0)).fetch_all(&mut *tx).await?;
        let mut claimed = Vec::with_capacity(rows.len());
        for (id, message_id, envelope_from, recipient, destination, route, attempts) in rows {
            let lease_token = Uuid::now_v7();
            sqlx::query(
                "UPDATE outbound_queue SET state = 'delivering', lease_token = $2,
                 lease_expires_at = now() + interval '5 minutes' WHERE id = $1",
            )
            .bind(id)
            .bind(lease_token)
            .execute(&mut *tx)
            .await?;
            claimed.push(ClaimedDelivery {
                queue_id: id,
                message_id,
                lease_token,
                envelope_from,
                recipient,
                destination,
                route,
                attempts,
            });
        }
        tx.commit().await?;
        Ok(claimed)
    }

    async fn lock_claim(
        tx: &mut Transaction<'_, Postgres>,
        id: Uuid,
        token: Uuid,
    ) -> Result<(Uuid, i32)> {
        let (message_id,): (Uuid,) =
            sqlx::query_as("SELECT message_id FROM outbound_queue WHERE id = $1")
                .bind(id)
                .fetch_one(&mut **tx)
                .await?;
        if !MailStore::lock_message_in_transaction(tx, message_id).await? {
            return Err(sqlx::Error::RowNotFound.into());
        }
        let row: Option<(i32,)> = sqlx::query_as(
            "SELECT q.attempts FROM outbound_queue q
             JOIN mail_submission_recipients r ON r.queue_id = q.id
             JOIN mail_submissions s ON s.id = r.submission_id AND s.message_id = q.message_id
             WHERE q.id = $1 AND q.lease_token = $2 AND q.state = 'delivering'
               AND r.state = 'outbound_pending' FOR UPDATE OF q, r",
        )
        .bind(id)
        .bind(token)
        .fetch_optional(&mut **tx)
        .await?;
        match row {
            Some((attempts,)) => Ok((message_id, attempts)),
            None => Err(sqlx::Error::Protocol(
                "stale delivery lease or inconsistent intent".into(),
            )
            .into()),
        }
    }

    pub async fn mark_delivered(&self, id: Uuid, token: Uuid) -> Result<()> {
        let mut tx = self.pool.begin().await?;
        let (message_id, _) = Self::lock_claim(&mut tx, id, token).await?;
        sqlx::query("UPDATE outbound_queue SET state = 'delivered', completed_at = now(), lease_token = NULL, lease_expires_at = NULL WHERE id = $1 AND lease_token = $2")
            .bind(id).bind(token).execute(&mut *tx).await?;
        sqlx::query(
            "UPDATE mail_submission_recipients SET state = 'delivered' WHERE queue_id = $1",
        )
        .bind(id)
        .execute(&mut *tx)
        .await?;
        MailStore::finish_deferred_erasure_in_transaction(&mut tx, message_id).await?;
        tx.commit().await?;
        Ok(())
    }

    pub async fn mark_attempt_failed(
        &self,
        id: Uuid,
        token: Uuid,
        permanent: bool,
        reason: &str,
    ) -> Result<bool> {
        let mut tx = self.pool.begin().await?;
        let (message_id, attempts) = Self::lock_claim(&mut tx, id, token).await?;
        let next = if permanent { None } else { backoff_for(attempts + 1) };
        let dead = if let Some(delay) = next {
            let at: DateTime<Utc> = Utc::now() + delay;
            sqlx::query(
                "UPDATE outbound_queue SET state = 'pending', attempts = attempts + 1,
                 next_attempt_at = $3, last_error = $4, lease_token = NULL, lease_expires_at = NULL
                 WHERE id = $1 AND lease_token = $2",
            )
            .bind(id)
            .bind(token)
            .bind(at)
            .bind(reason)
            .execute(&mut *tx)
            .await?;
            false
        } else {
            sqlx::query(
                "UPDATE outbound_queue SET state = 'failed', attempts = attempts + 1,
                 last_error = $3, completed_at = now(), lease_token = NULL, lease_expires_at = NULL
                 WHERE id = $1 AND lease_token = $2",
            )
            .bind(id)
            .bind(token)
            .bind(reason)
            .execute(&mut *tx)
            .await?;
            sqlx::query(
                "UPDATE mail_submission_recipients SET state = 'failed' WHERE queue_id = $1",
            )
            .bind(id)
            .execute(&mut *tx)
            .await?;
            MailStore::finish_deferred_erasure_in_transaction(&mut tx, message_id).await?;
            true
        };
        tx.commit().await?;
        Ok(dead)
    }

    pub async fn purge_finished(
        &self,
        delivered_after: Duration,
        failed_after: Duration,
    ) -> Result<u64> {
        let delivered_before = Utc::now() - delivered_after;
        let failed_before = Utc::now() - failed_after;
        let rows: Vec<(Uuid, Uuid)> = sqlx::query_as(
            "SELECT id, message_id FROM outbound_queue
             WHERE (state = 'delivered' AND completed_at < $1) OR (state = 'failed' AND completed_at < $2)",
        ).bind(delivered_before).bind(failed_before).fetch_all(&self.pool).await?;
        let mut purged = 0;
        for (id, message_id) in rows {
            let mut tx = self.pool.begin().await?;
            if !MailStore::lock_message_in_transaction(&mut tx, message_id).await? {
                continue;
            }
            let state: Option<(String,)> = sqlx::query_as(
                "SELECT state FROM outbound_queue WHERE id = $1
                 AND ((state = 'delivered' AND completed_at < $2) OR (state = 'failed' AND completed_at < $3))
                 FOR UPDATE",
            ).bind(id).bind(delivered_before).bind(failed_before).fetch_optional(&mut *tx).await?;
            if let Some((state,)) = state {
                let removed = sqlx::query(
                    "DELETE FROM mail_submission_recipients WHERE queue_id = $1 AND state = $2",
                )
                .bind(id)
                .bind(&state)
                .execute(&mut *tx)
                .await?;
                if removed.rows_affected() != 1 {
                    return Err(
                        sqlx::Error::Protocol("terminal queue and intent disagree".into()).into(),
                    );
                }
                sqlx::query("DELETE FROM outbound_queue WHERE id = $1")
                    .bind(id)
                    .execute(&mut *tx)
                    .await?;
                MailStore::erase_orphaned_message_in_transaction(&mut tx, message_id).await?;
                purged += 1;
            }
            tx.commit().await?;
        }
        Ok(purged)
    }

    pub async fn purge_message(&self, message_id: Uuid) -> Result<bool> {
        let mut tx = self.pool.begin().await?;
        if !MailStore::lock_message_in_transaction(&mut tx, message_id).await? {
            return Ok(false);
        }
        let deleted = MailStore::erase_orphaned_message_in_transaction(&mut tx, message_id).await?;
        if deleted {
            tx.commit().await?;
            return Ok(true);
        }
        let (retained,): (bool,) = sqlx::query_as(
            "SELECT EXISTS (SELECT 1 FROM mailbox_messages WHERE message_id = $1)
                OR EXISTS (SELECT 1 FROM mail_submission_recipients r
                           JOIN mail_submissions s ON s.id = r.submission_id
                           WHERE s.message_id = $1 AND r.state IN ('local_pending', 'outbound_pending'))",
        ).bind(message_id).fetch_one(&mut *tx).await?;
        if retained {
            tx.commit().await?;
            return Ok(false);
        }
        sqlx::query(
            "DELETE FROM mail_submission_recipients WHERE queue_id IN
             (SELECT id FROM outbound_queue WHERE message_id = $1 AND state IN ('delivered', 'failed'))",
        ).bind(message_id).execute(&mut *tx).await?;
        sqlx::query(
            "DELETE FROM outbound_queue WHERE message_id = $1 AND state IN ('delivered', 'failed')",
        )
        .bind(message_id)
        .execute(&mut *tx)
        .await?;
        let deleted = MailStore::erase_orphaned_message_in_transaction(&mut tx, message_id).await?;
        tx.commit().await?;
        Ok(deleted)
    }

    pub async fn pending_count(&self) -> Result<i64> {
        let (count,): (i64,) =
            sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE state = 'pending'")
                .fetch_one(&self.pool)
                .await?;
        Ok(count)
    }
}
