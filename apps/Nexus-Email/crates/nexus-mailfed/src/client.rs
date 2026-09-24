//! Sending handoffs to pinned peers.

use std::future::Future;
use std::pin::Pin;
use std::sync::Arc;
use std::time::Duration;

use nexus_maildelivery::{FederatedHandoff, FederatedTransport, HandoffOutcome};

use crate::key::NodeKey;
use crate::peers::PeerDirectory;
use crate::wire::{self, HandoffResponse, Outgoing};

pub struct HttpTransport {
    /// This node's domain, sent as `X-Nexus-Node`.
    local_domain: String,
    key: Arc<NodeKey>,
    peers: PeerDirectory,
    clock: Arc<dyn Fn() -> i64 + Send + Sync>,
    http: reqwest::Client,
}

impl HttpTransport {
    pub fn new(
        local_domain: String,
        key: Arc<NodeKey>,
        peers: PeerDirectory,
        clock: Arc<dyn Fn() -> i64 + Send + Sync>,
    ) -> Self {
        let http = reqwest::Client::builder()
            .timeout(Duration::from_secs(60))
            .connect_timeout(Duration::from_secs(10))
            // A peer's federation URL is pinned; following a redirect would
            // send signed mail somewhere the operator never pinned.
            .redirect(reqwest::redirect::Policy::none())
            .build()
            .expect("reqwest client");
        Self { local_domain, key, peers, clock, http }
    }

    async fn deliver(&self, h: FederatedHandoff<'_>) -> HandoffOutcome {
        let peer = match self.peers.get(h.node).await {
            Ok(Some(p)) => p,
            Ok(None) => return HandoffOutcome::Deferred(format!("{} is not a pinned peer", h.node)),
            Err(e) => return HandoffOutcome::Deferred(format!("peer lookup: {e}")),
        };

        let url = match reqwest::Url::parse(&format!("{}{}", peer.base_url, wire::MAIL_PATH)) {
            Ok(u) => u,
            Err(e) => return HandoffOutcome::Deferred(format!("pinned URL for {} is invalid: {e}", peer.domain)),
        };
        // The host the peer verifies against is the authority we pinned.
        let host = match (url.host_str(), url.port()) {
            (Some(h), Some(p)) => format!("{h}:{p}"),
            (Some(h), None) => h.to_string(),
            (None, _) => return HandoffOutcome::Deferred(format!("pinned URL for {} has no host", peer.domain)),
        };

        let out = Outgoing {
            node: self.local_domain.clone(),
            host,
            envelope_from: h.envelope_from.to_string(),
            recipients: vec![h.recipient.to_string()],
            timestamp: (self.clock)(),
            nonce: wire::new_nonce(),
        };
        let mut req = self.http.post(url).body(h.raw.to_vec());
        for (name, value) in out.sign(&self.key, h.raw) {
            req = req.header(name, value);
        }

        let res = match req.send().await {
            Ok(r) => r,
            Err(e) => return HandoffOutcome::Deferred(format!("{} unreachable: {e}", peer.domain)),
        };
        let status = res.status();
        if !status.is_success() {
            let body = res.text().await.unwrap_or_default();
            return HandoffOutcome::Deferred(format!("{} answered {status}: {body}", peer.domain));
        }
        let parsed: HandoffResponse = match res.json().await {
            Ok(p) => p,
            Err(e) => return HandoffOutcome::Deferred(format!("{} sent an unreadable answer: {e}", peer.domain)),
        };
        match parsed.results.into_iter().find(|r| r.recipient == h.recipient) {
            Some(r) if r.accepted => HandoffOutcome::Delivered,
            Some(r) => HandoffOutcome::Rejected(r.reason.unwrap_or_else(|| "refused by peer".into())),
            None => HandoffOutcome::Deferred(format!("{} did not answer for {}", peer.domain, h.recipient)),
        }
    }
}

impl FederatedTransport for HttpTransport {
    fn send<'a>(
        &'a self,
        handoff: FederatedHandoff<'a>,
    ) -> Pin<Box<dyn Future<Output = HandoffOutcome> + Send + 'a>> {
        Box::pin(self.deliver(handoff))
    }
}
