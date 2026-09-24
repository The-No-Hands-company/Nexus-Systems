use base64::{engine::general_purpose::STANDARD, Engine};
use ed25519_dalek::{Signature, VerifyingKey};
use sha2::{Digest, Sha256};

use crate::error::{FedError, Result};
use crate::key::NodeKey;

/// Everything about a handoff that the signature vouches for.
///
/// If a field can change what the receiver does, it is in here — otherwise a
/// captured request could be edited in flight and still verify.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SignedRequest {
    pub method: String,
    pub path: String,
    pub host: String,
    pub timestamp: i64,
    pub nonce: String,
    pub envelope_from: String,
    pub recipients: Vec<String>,
    /// Lowercase hex SHA-256 of the body.
    pub body_sha256: String,
}

impl SignedRequest {
    pub fn body_digest(body: &[u8]) -> String {
        hex::encode(Sha256::digest(body))
    }

    /// The exact bytes that are signed.
    ///
    /// Recipients are length-prefixed rather than joined, so no choice of
    /// addresses can make two different lists encode identically.
    pub fn canonical(&self) -> String {
        let recipients: Vec<String> =
            self.recipients.iter().map(|r| format!("{}:{}", r.len(), r)).collect();
        [
            "nexus-mail-v1".to_string(),
            self.method.to_ascii_uppercase(),
            self.path.clone(),
            self.host.to_ascii_lowercase(),
            self.timestamp.to_string(),
            self.nonce.clone(),
            self.envelope_from.clone(),
            format!("{}|{}", self.recipients.len(), recipients.join("")),
            self.body_sha256.clone(),
        ]
        .join("\n")
    }
}

pub fn sign(key: &NodeKey, req: &SignedRequest) -> String {
    STANDARD.encode(key.sign_bytes(req.canonical().as_bytes()).to_bytes())
}

/// Verify `sig_b64` over `req` against a pinned public key.
///
/// Strict verification: rejects the malleable and small-order encodings that
/// plain Ed25519 verification tolerates.
pub fn verify(public_b64: &str, req: &SignedRequest, sig_b64: &str) -> Result<()> {
    let public: [u8; 32] = STANDARD
        .decode(public_b64)
        .map_err(|_| FedError::Malformed("public key"))?
        .try_into()
        .map_err(|_| FedError::Malformed("public key"))?;
    let key = VerifyingKey::from_bytes(&public).map_err(|_| FedError::Malformed("public key"))?;

    let sig: [u8; 64] = STANDARD
        .decode(sig_b64)
        .map_err(|_| FedError::Malformed("signature"))?
        .try_into()
        .map_err(|_| FedError::Malformed("signature"))?;

    key.verify_strict(req.canonical().as_bytes(), &Signature::from_bytes(&sig))
        .map_err(|_| FedError::BadSignature)
}
