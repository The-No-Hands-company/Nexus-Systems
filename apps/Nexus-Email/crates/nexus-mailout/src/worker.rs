use std::time::Duration;

use hickory_resolver::TokioAsyncResolver;
use nexus_maildelivery::Queue;
use nexus_mailstore::MailStore;

use crate::client::{deliver, Attempt};
use crate::mx::{resolve, MxError};

#[derive(Debug, Clone)]
pub struct WorkerConfig {
    /// The name this node gives in EHLO. Should be a hostname that resolves
    /// back here — receivers check, and a mismatch costs reputation.
    pub ehlo_name: String,
    /// How often to look for due work when the queue is empty.
    pub idle_poll: Duration,
    /// How many deliveries to claim at once.
    pub batch: i64,
    pub port: u16,
}

impl Default for WorkerConfig {
    fn default() -> Self {
        Self {
            ehlo_name: "localhost".into(),
            idle_poll: Duration::from_secs(20),
            batch: 10,
            port: 25,
        }
    }
}

/// Drains the outbound queue.
pub struct DeliveryWorker {
    store: MailStore,
    queue: Queue,
    resolver: TokioAsyncResolver,
    config: WorkerConfig,
}

impl DeliveryWorker {
    pub fn new(store: MailStore, queue: Queue, config: WorkerConfig) -> Self {
        Self {
            store,
            queue,
            resolver: TokioAsyncResolver::tokio_from_system_conf()
                .unwrap_or_else(|_| TokioAsyncResolver::tokio(Default::default(), Default::default())),
            config,
        }
    }

    /// Run until cancelled.
    pub async fn run(&self) {
        loop {
            match self.tick().await {
                Ok(0) => tokio::time::sleep(self.config.idle_poll).await,
                Ok(_) => {}
                Err(e) => {
                    tracing::warn!(error = %e, "delivery tick failed");
                    tokio::time::sleep(self.config.idle_poll).await;
                }
            }
        }
    }

    /// One pass. Returns how many deliveries were attempted.
    pub async fn tick(&self) -> Result<usize, String> {
        let due = self
            .queue
            .claim_due(self.config.batch)
            .await
            .map_err(|e| e.to_string())?;
        let count = due.len();

        for item in due {
            // Only SMTP is delivered here. Federated handoffs go over the node
            // channel and are another transport's job.
            if item.route != "smtp" {
                continue;
            }

            let raw = match self.raw_for(item.message_id).await {
                Ok(r) => r,
                Err(e) => {
                    // The message is unreadable, which will not improve with
                    // time — but bouncing on a storage error would lose mail
                    // over a transient database problem, so it is deferred.
                    let _ = self
                        .queue
                        .mark_attempt_failed(item.id, false, &format!("message unreadable: {e}"))
                        .await;
                    continue;
                }
            };

            let outcome = self
                .attempt(&item.envelope_from, &item.recipient, &item.destination, &raw)
                .await;

            match outcome {
                Attempt::Delivered => {
                    let _ = self.queue.mark_delivered(item.id).await;
                }
                Attempt::Rejected(reason) => {
                    let dead = self.queue.mark_attempt_failed(item.id, true, &reason).await;
                    if matches!(dead, Ok(true)) {
                        tracing::info!(recipient = %item.recipient, %reason, "permanent failure; bounce due");
                    }
                }
                Attempt::Deferred(reason) => {
                    let dead = self.queue.mark_attempt_failed(item.id, false, &reason).await;
                    if matches!(dead, Ok(true)) {
                        tracing::info!(recipient = %item.recipient, %reason, "gave up after retries; bounce due");
                    }
                }
            }
        }

        Ok(count)
    }

    /// Try each mail exchanger in preference order until one takes the message.
    async fn attempt(&self, from: &str, recipient: &str, domain: &str, raw: &[u8]) -> Attempt {
        let hosts = match resolve(&self.resolver, domain).await {
            Ok(h) => h,
            // A domain with no mail exchanger will not grow one; that is a real
            // bounce. A DNS failure is not.
            Err(MxError::NoMailExchanger(d)) => {
                return Attempt::Rejected(format!("{d} accepts no mail"))
            }
            Err(e) => return Attempt::Deferred(e.to_string()),
        };

        let mut last = Attempt::Deferred(format!("no mail exchanger for {domain} could be reached"));
        for mx in hosts {
            match deliver(&mx.host, self.config.port, &self.config.ehlo_name, from, recipient, raw).await {
                Attempt::Delivered => return Attempt::Delivered,
                // A permanent refusal from one server is the domain's answer;
                // trying its backup would just collect the same refusal.
                Attempt::Rejected(r) => return Attempt::Rejected(r),
                Attempt::Deferred(r) => last = Attempt::Deferred(r),
            }
        }
        last
    }

    async fn raw_for(&self, message_id: uuid::Uuid) -> Result<Vec<u8>, String> {
        let row: (Option<Vec<u8>>,) =
            sqlx_fetch(self.store.pool(), message_id).await.map_err(|e| e.to_string())?;
        row.0.ok_or_else(|| "message body is not inline".to_string())
    }
}

/// Kept separate so the worker does not need sqlx in its signature.
async fn sqlx_fetch(
    pool: &sqlx::PgPool,
    message_id: uuid::Uuid,
) -> Result<(Option<Vec<u8>>,), sqlx::Error> {
    sqlx::query_as("SELECT body_inline FROM messages WHERE id = $1")
        .bind(message_id)
        .fetch_one(pool)
        .await
}
