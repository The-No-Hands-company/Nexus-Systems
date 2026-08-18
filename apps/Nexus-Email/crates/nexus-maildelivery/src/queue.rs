use chrono::{DateTime, Duration, Utc};
use sqlx::PgPool;
use uuid::Uuid;

use crate::error::Result;
use crate::route::Route;

/// How long to keep retrying, and how the gaps grow.
///
/// The shape matters more than the numbers: fast first retries catch a peer
/// that was restarting, then long ones so a node down for a day does not get
/// hammered. After the last entry the message is dead and the sender is told —
/// roughly five days total, which is the convention every mail system settled
/// on because it spans a weekend.
const BACKOFF_MINUTES: &[i64] = &[1, 5, 15, 60, 240, 720, 1440, 1440, 1440, 1440];

pub fn backoff_for(attempts: i32) -> Option<Duration> {
    BACKOFF_MINUTES
        .get(attempts as usize)
        .map(|m| Duration::minutes(*m))
}

/// The maximum number of attempts before a message is given up on.
pub fn max_attempts() -> i32 {
    BACKOFF_MINUTES.len() as i32
}

#[derive(Debug, Clone)]
pub struct QueuedDelivery {
    pub id: Uuid,
    pub message_id: Uuid,
    pub envelope_from: String,
    pub recipient: String,
    pub destination: String,
    pub route: String,
    pub attempts: i32,
}

#[derive(Clone)]
pub struct Queue {
    pool: PgPool,
}

impl Queue {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    /// Queue one recipient of a message.
    ///
    /// Idempotent: re-queueing the same message for the same recipient does
    /// nothing, so a retried submission cannot become two deliveries.
    pub async fn enqueue(
        &self,
        message_id: Uuid,
        envelope_from: &str,
        recipient: &str,
        route: &Route,
    ) -> Result<()> {
        let (route_name, destination) = match route {
            Route::Federated { node } => ("federated", node.clone()),
            Route::External => (
                "smtp",
                recipient.rsplit_once('@').map(|(_, d)| d.to_string()).unwrap_or_default(),
            ),
            // Local delivery is not queued: it is a database write that either
            // happens or fails immediately, with no network to retry against.
            Route::Local => return Ok(()),
        };

        sqlx::query(
            "INSERT INTO outbound_queue (id, message_id, envelope_from, recipient, destination, route)
             VALUES ($1, $2, $3, $4, $5, $6)
             ON CONFLICT (message_id, recipient) DO NOTHING",
        )
        .bind(Uuid::now_v7())
        .bind(message_id)
        .bind(envelope_from)
        .bind(recipient)
        .bind(destination)
        .bind(route_name)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    /// Claim deliveries that are due, marking them in flight.
    ///
    /// `FOR UPDATE SKIP LOCKED` so several delivery workers can run at once
    /// without two of them sending the same message twice.
    pub async fn claim_due(&self, limit: i64) -> Result<Vec<QueuedDelivery>> {
        let rows: Vec<(Uuid, Uuid, String, String, String, String, i32)> = sqlx::query_as(
            "UPDATE outbound_queue SET state = 'delivering'
             WHERE id IN (
                 SELECT id FROM outbound_queue
                 WHERE state = 'pending' AND next_attempt_at <= now()
                 ORDER BY next_attempt_at
                 LIMIT $1
                 FOR UPDATE SKIP LOCKED
             )
             RETURNING id, message_id, envelope_from, recipient, destination, route, attempts",
        )
        .bind(limit)
        .fetch_all(&self.pool)
        .await?;

        Ok(rows
            .into_iter()
            .map(|(id, message_id, envelope_from, recipient, destination, route, attempts)| {
                QueuedDelivery { id, message_id, envelope_from, recipient, destination, route, attempts }
            })
            .collect())
    }

    pub async fn mark_delivered(&self, id: Uuid) -> Result<()> {
        sqlx::query(
            "UPDATE outbound_queue SET state = 'delivered', completed_at = now() WHERE id = $1",
        )
        .bind(id)
        .execute(&self.pool)
        .await?;
        Ok(())
    }

    /// Record a failed attempt.
    ///
    /// Returns true when the delivery is now dead, which is the caller's cue to
    /// generate a bounce. A temporary failure that has run out of attempts is
    /// just as dead as a permanent one — the difference is only how long it
    /// took to find out.
    ///
    /// The attempt count is read from the row inside a transaction rather than
    /// passed in. A caller that supplies its own count can supply a stale one,
    /// and the failure mode is a message that retries forever without ever
    /// bouncing — silent, and only visible as a queue that never drains.
    pub async fn mark_attempt_failed(
        &self,
        id: Uuid,
        permanent: bool,
        reason: &str,
    ) -> Result<bool> {
        let mut tx = self.pool.begin().await?;

        let (attempts,): (i32,) =
            sqlx::query_as("SELECT attempts FROM outbound_queue WHERE id = $1 FOR UPDATE")
                .bind(id)
                .fetch_one(&mut *tx)
                .await?;

        // The attempt being recorded is the one that just failed, so the next
        // delay is indexed by the count after incrementing.
        let next = if permanent { None } else { backoff_for(attempts + 1) };

        let dead = match next {
            Some(delay) => {
                let at: DateTime<Utc> = Utc::now() + delay;
                sqlx::query(
                    "UPDATE outbound_queue
                     SET state = 'pending', attempts = attempts + 1,
                         next_attempt_at = $1, last_error = $2
                     WHERE id = $3",
                )
                .bind(at)
                .bind(reason)
                .bind(id)
                .execute(&mut *tx)
                .await?;
                false
            }
            None => {
                sqlx::query(
                    "UPDATE outbound_queue
                     SET state = 'failed', attempts = attempts + 1,
                         last_error = $1, completed_at = now()
                     WHERE id = $2",
                )
                .bind(reason)
                .bind(id)
                .execute(&mut *tx)
                .await?;
                true
            }
        };

        tx.commit().await?;
        Ok(dead)
    }

    pub async fn pending_count(&self) -> Result<i64> {
        let row: (i64,) =
            sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE state = 'pending'")
                .fetch_one(&self.pool)
                .await?;
        Ok(row.0)
    }
}
