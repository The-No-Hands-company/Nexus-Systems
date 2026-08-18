use base64::{engine::general_purpose::STANDARD, Engine};
use rsa::pkcs1v15::SigningKey;
use rsa::signature::{SignatureEncoding, Signer};
use rsa::RsaPrivateKey;
use sha2::{Digest, Sha256};

use crate::canon::{body as canon_body, header as canon_header, Canon};
use crate::error::{DkimError, Result};

/// Headers signed by default.
///
/// From is mandatory — a signature that does not cover From proves nothing,
/// because the whole question is who sent this. The rest are the fields a
/// recipient actually judges the message by, so leaving them unsigned would
/// let a relay rewrite the subject without breaking the signature.
pub const DEFAULT_SIGNED_HEADERS: &[&str] = &[
    "from", "to", "cc", "subject", "date", "message-id",
    "in-reply-to", "references", "mime-version", "content-type",
];

pub struct Signer_ {
    pub domain: String,
    pub selector: String,
    pub key: RsaPrivateKey,
    pub header_canon: Canon,
    pub body_canon: Canon,
}

/// Sign a message, returning the DKIM-Signature header to prepend.
///
/// The returned line already ends with CRLF and belongs at the top of the
/// message, before every header it covers.
pub fn sign(cfg: &Signer_, raw: &[u8], signed_headers: &[&str]) -> Result<String> {
    let parsed = nexus_mailmsg::parse(raw, nexus_mailmsg::Limits::default())
        .map_err(|_| DkimError::Malformed)?;

    // 1. Body hash over the canonicalized body.
    let bh = STANDARD.encode(Sha256::digest(canon_body(cfg.body_canon, &parsed.body)));

    // Only headers actually present are listed in h=. Naming an absent header
    // is legal and means "this was not present", but listing one we did not
    // hash would make the signature unverifiable.
    let present: Vec<&str> = signed_headers
        .iter()
        .copied()
        .filter(|h| parsed.headers.get(h).is_some())
        .collect();

    let timestamp = chrono::Utc::now().timestamp();
    let h_tag = present.join(":");

    // 2. Build the signature header with an empty b=, which is what gets signed.
    let unsigned = format!(
        "v=1; a=rsa-sha256; c={}/{}; d={}; s={}; t={}; h={}; bh={}; b=",
        cfg.header_canon.as_str(),
        cfg.body_canon.as_str(),
        cfg.domain,
        cfg.selector,
        timestamp,
        h_tag,
        bh
    );

    // 3. Hash the canonicalized signed headers, then the signature header
    //    itself with its b= value empty and no trailing CRLF.
    let mut to_sign = Vec::new();
    for name in &present {
        if let Some(value) = parsed.headers.get(name) {
            to_sign.extend_from_slice(canon_header(cfg.header_canon, name, value).as_bytes());
        }
    }
    to_sign.extend_from_slice(
        canon_header(cfg.header_canon, "DKIM-Signature", &format!(" {unsigned}"))
            .trim_end_matches("\r\n")
            .as_bytes(),
    );

    let signing_key = SigningKey::<Sha256>::new(cfg.key.clone());
    let signature = signing_key.sign(&to_sign);
    let b = STANDARD.encode(signature.to_bytes());

    Ok(format!("DKIM-Signature: {unsigned}{b}\r\n"))
}

/// Prepend a signature to a message.
pub fn signed_message(header_line: &str, raw: &[u8]) -> Vec<u8> {
    let mut out = Vec::with_capacity(raw.len() + header_line.len());
    out.extend_from_slice(header_line.as_bytes());
    out.extend_from_slice(raw);
    out
}
