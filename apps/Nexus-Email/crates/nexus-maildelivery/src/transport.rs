use crate::error::Result;

/// A message handed to another Nexus node.
#[derive(Debug, Clone)]
pub struct FederatedHandoff<'a> {
    pub node: &'a str,
    pub envelope_from: &'a str,
    pub recipient: &'a str,
    /// The message exactly as stored. Bytes, not a re-serialised struct: what
    /// the peer stores must hash to the same content address we hold, and
    /// re-encoding on the way out would break that.
    pub raw: &'a [u8],
}

/// How a message reaches another Nexus node.
///
/// A trait rather than a concrete client so delivery can be tested without a
/// second node running, and so the wire format can change — the node-to-node
/// channel is ours, so it is allowed to improve — without touching the queue
/// or the retry policy.
pub trait FederatedTransport: Send + Sync {
    fn send(&self, handoff: FederatedHandoff<'_>) -> Result<()>;
}
