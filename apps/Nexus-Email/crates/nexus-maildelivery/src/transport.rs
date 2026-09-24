use std::future::Future;
use std::pin::Pin;

/// A message handed to another Nexus node.
#[derive(Debug, Clone)]
pub struct FederatedHandoff<'a> {
    /// The pinned peer to hand it to.
    pub node: &'a str,
    pub envelope_from: &'a str,
    pub recipient: &'a str,
    /// The message exactly as stored. Bytes, not a re-serialised struct: what
    /// the peer stores must hash to the same content address we hold, and
    /// re-encoding on the way out would break that.
    pub raw: &'a [u8],
}

/// What became of a handoff.
///
/// The same asymmetry as outbound SMTP: only an explicit refusal of the
/// recipient is permanent. An unreachable peer, a timeout, a key mismatch — all
/// of those defer, because bouncing would lose mail over something an operator
/// can fix.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HandoffOutcome {
    Delivered,
    Rejected(String),
    Deferred(String),
}

/// How a message reaches another Nexus node.
///
/// A trait rather than a concrete client so delivery can be tested without a
/// second node running, and so the wire format can change — the node-to-node
/// channel is ours, so it is allowed to improve — without touching the queue
/// or the retry policy. Returns a boxed future so it stays object-safe.
pub trait FederatedTransport: Send + Sync {
    fn send<'a>(
        &'a self,
        handoff: FederatedHandoff<'a>,
    ) -> Pin<Box<dyn Future<Output = HandoffOutcome> + Send + 'a>>;
}
