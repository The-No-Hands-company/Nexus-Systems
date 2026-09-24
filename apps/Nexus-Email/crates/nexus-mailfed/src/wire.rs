//! The handoff on the wire: paths, header names, and how a request is signed.
//! Shared by the sender and the receiver so the two cannot drift apart.

use crate::key::NodeKey;
use crate::signature::{sign, SignedRequest};

pub const MAIL_PATH: &str = "/federation/v1/mail";
pub const KEY_PATH: &str = "/federation/v1/key";

pub const H_NODE: &str = "x-nexus-node";
pub const H_FROM: &str = "x-nexus-envelope-from";
pub const H_RECIPIENTS: &str = "x-nexus-recipients";
pub const H_TIMESTAMP: &str = "x-nexus-timestamp";
pub const H_NONCE: &str = "x-nexus-nonce";
pub const H_SIGNATURE: &str = "x-nexus-signature";

/// How far a request's timestamp may be from the receiver's clock, either way.
pub const MAX_SKEW_SECS: i64 = 300;

/// Hard cap on a handoff body. Matches what the SMTP side will accept, with
/// headroom for a DKIM-Signature header added at the origin.
pub const MAX_BODY_BYTES: usize = 26 * 1024 * 1024;

/// A handoff about to be sent.
#[derive(Debug, Clone)]
pub struct Outgoing {
    /// The sending node's domain — what the receiver looks the key up by.
    pub node: String,
    /// The receiver's public federation host, as pinned. Signed, so a request
    /// meant for one node is useless at another.
    pub host: String,
    pub envelope_from: String,
    pub recipients: Vec<String>,
    pub timestamp: i64,
    pub nonce: String,
}

impl Outgoing {
    pub fn signed_request(&self, body: &[u8]) -> SignedRequest {
        SignedRequest {
            method: "POST".into(),
            path: MAIL_PATH.into(),
            host: self.host.clone(),
            timestamp: self.timestamp,
            nonce: self.nonce.clone(),
            envelope_from: self.envelope_from.clone(),
            recipients: self.recipients.clone(),
            body_sha256: SignedRequest::body_digest(body),
        }
    }

    /// The headers to send alongside `body`.
    ///
    /// Recipients are comma-joined on the wire; an address containing a comma
    /// is not something this system accepts, and the signature covers the
    /// parsed list, so a smuggled comma fails verification rather than adding
    /// a recipient.
    pub fn sign(&self, key: &NodeKey, body: &[u8]) -> Vec<(&'static str, String)> {
        let signature = sign(key, &self.signed_request(body));
        vec![
            (H_NODE, self.node.clone()),
            (H_FROM, self.envelope_from.clone()),
            (H_RECIPIENTS, self.recipients.join(",")),
            (H_TIMESTAMP, self.timestamp.to_string()),
            (H_NONCE, self.nonce.clone()),
            (H_SIGNATURE, signature),
        ]
    }
}

/// A fresh 128-bit nonce, hex.
pub fn new_nonce() -> String {
    use rand::RngCore;
    let mut b = [0u8; 16];
    rand::rngs::OsRng.fill_bytes(&mut b);
    hex::encode(b)
}

/// The per-recipient answer to a handoff.
#[derive(Debug, Clone, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
pub struct RecipientResult {
    pub recipient: String,
    pub accepted: bool,
    #[serde(default, skip_serializing_if = "Option::is_none")]
    pub reason: Option<String>,
}

#[derive(Debug, Clone, serde::Serialize, serde::Deserialize)]
pub struct HandoffResponse {
    pub results: Vec<RecipientResult>,
}
