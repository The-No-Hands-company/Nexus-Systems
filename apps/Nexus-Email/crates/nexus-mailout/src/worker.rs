use std::sync::Arc;
use std::time::Duration;

use hickory_resolver::TokioAsyncResolver;
use nexus_mailauth::{sign, signed_message, DkimSigner, DEFAULT_SIGNED_HEADERS};
use nexus_maildelivery::{FederatedHandoff, FederatedTransport, HandoffOutcome, Queue, QueuedDelivery};
use nexus_mailstore::MailStore;

use crate::client::{deliver, Attempt};
use crate::mx::{resolve, MxError};

/// How mail for the outside world leaves this node.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Egress {
    /// MX lookup and SMTP from here. Needs an unfiltered port 25.
    Direct,
    /// Hand it to this pinned peer, which delivers it from its own address.
    Peer(String),
}

impl Egress {
    /// Parse `NEXUS_EMAIL_EGRESS`: empty or `direct`, or `peer:<domain>`.
    ///
    /// Anything else is an error rather than a fallback to direct: a typo that
    /// silently meant "direct" would leave every message waiting on a filtered
    /// port while the operator believed a peer was delivering it.
    pub fn parse(setting: &str) -> Result<Self, String> {
        let s = setting.trim();
        if s.is_empty() || s == "direct" {
            return Ok(Egress::Direct);
        }
        match s.strip_prefix("peer:").map(str::trim) {
            Some(domain) if !domain.is_empty() => Ok(Egress::Peer(domain.to_ascii_lowercase())),
            _ => Err(format!("NEXUS_EMAIL_EGRESS must be `direct` or `peer:<domain>`, not {setting:?}")),
        }
    }
}

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
    pub egress: Egress,
}

impl Default for WorkerConfig {
    fn default() -> Self {
        Self {
            ehlo_name: "localhost".into(),
            idle_poll: Duration::from_secs(20),
            batch: 10,
            port: 25,
            egress: Egress::Direct,
        }
    }
}

/// Drains the outbound queue.
pub struct DeliveryWorker {
    store: MailStore,
    queue: Queue,
    resolver: TokioAsyncResolver,
    config: WorkerConfig,
    transport: Option<Arc<dyn FederatedTransport>>,
    dkim: Option<Arc<DkimSigner>>,
}

impl DeliveryWorker {
    pub fn new(store: MailStore, queue: Queue, config: WorkerConfig) -> Self {
        Self {
            store,
            queue,
            resolver: TokioAsyncResolver::tokio_from_system_conf()
                .unwrap_or_else(|_| TokioAsyncResolver::tokio(Default::default(), Default::default())),
            config,
            transport: None,
            dkim: None,
        }
    }

    /// The node-to-node channel, for federated mail and for peer egress.
    pub fn with_transport(mut self, transport: Arc<dyn FederatedTransport>) -> Self {
        self.transport = Some(transport);
        self
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
            self.process(item).await;
        }
        Ok(count)
    }

    /// Sign mail from this domain on its way out, so a receiver — or a peer
    /// relaying it — sees a signature aligned with the sender's domain.
    pub fn with_dkim(mut self, signer: Arc<DkimSigner>) -> Self {
        self.dkim = Some(signer);
        self
    }

    /// The bytes that leave this node. The stored message is never rewritten:
    /// the signature is added to the copy in flight.
    ///
    /// Only mail whose envelope-from is in the signing domain is signed; our
    /// key vouching for another domain's mail would be a false claim. A signing
    /// failure sends the message unsigned rather than holding it, because it
    /// would fail the same way on every retry and end as a bounce.
    fn outgoing(&self, envelope_from: &str, raw: Vec<u8>) -> Vec<u8> {
        let Some(signer) = &self.dkim else { return raw };
        let from_domain = envelope_from.rsplit_once('@').map(|(_, d)| d.to_ascii_lowercase());
        if from_domain.as_deref() != Some(signer.domain.to_ascii_lowercase().as_str()) {
            return raw;
        }
        match sign(signer, &raw, DEFAULT_SIGNED_HEADERS) {
            Ok(header) => signed_message(&header, &raw),
            Err(e) => {
                tracing::warn!(error = %e, from = envelope_from, "DKIM signing failed; sending unsigned");
                raw
            }
        }
    }

    /// Deliver one claimed row by whichever path its route and the egress
    /// setting call for, and record the outcome.
    pub async fn process(&self, item: QueuedDelivery) {
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
                return;
            }
        };
        let raw = self.outgoing(&item.envelope_from, raw);

        let outcome = match (item.route.as_str(), &self.config.egress) {
            ("smtp", Egress::Direct) => {
                self.attempt(&item.envelope_from, &item.recipient, &item.destination, &raw).await
            }
            ("smtp", Egress::Peer(peer)) => self.hand_off(peer, &item, &raw).await,
            ("federated", _) => self.hand_off(&item.destination, &item, &raw).await,
            (other, _) => Attempt::Deferred(format!("unknown route {other:?}")),
        };

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

    /// Hand a message to a peer over the node channel.
    async fn hand_off(&self, node: &str, item: &QueuedDelivery, raw: &[u8]) -> Attempt {
        let Some(transport) = &self.transport else {
            // Deferred, never dropped: the day the channel is configured, the
            // mail that waited for it goes out.
            return Attempt::Deferred("no federation transport configured on this node".into());
        };
        let handoff = FederatedHandoff {
            node,
            envelope_from: &item.envelope_from,
            recipient: &item.recipient,
            raw,
        };
        match transport.send(handoff).await {
            HandoffOutcome::Delivered => Attempt::Delivered,
            HandoffOutcome::Rejected(r) => Attempt::Rejected(r),
            HandoffOutcome::Deferred(r) => Attempt::Deferred(r),
        }
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
