use base64::{engine::general_purpose::STANDARD, Engine};
use rsa::pkcs1v15::{Signature, VerifyingKey};
use rsa::pkcs8::DecodePublicKey;
use rsa::signature::Verifier;
use rsa::RsaPublicKey;
use sha2::{Digest, Sha256};

use crate::canon::{body as canon_body, header as canon_header, Canon};
use crate::error::{DkimError, Result};

/// The tags of a DKIM-Signature header we care about.
#[derive(Debug, Clone)]
pub struct Signature_ {
    pub domain: String,
    pub selector: String,
    pub headers: Vec<String>,
    pub body_hash: String,
    pub signature: String,
    pub header_canon: Canon,
    pub body_canon: Canon,
    /// The raw header value, needed to rebuild what was signed.
    pub raw_value: String,
}

/// Parse a DKIM-Signature header value.
pub fn parse_signature(value: &str) -> Result<Signature_> {
    let mut tags = std::collections::HashMap::new();
    for part in value.split(';') {
        if let Some((k, v)) = part.split_once('=') {
            // Values may be folded across lines, so whitespace inside base64 is
            // normal and must be stripped rather than treated as content.
            tags.insert(
                k.trim().to_string(),
                v.chars().filter(|c| !c.is_whitespace()).collect::<String>(),
            );
        }
    }

    let get = |k: &'static str| tags.get(k).cloned().ok_or(DkimError::MissingTag(k));

    let algorithm = tags.get("a").cloned().unwrap_or_else(|| "rsa-sha256".into());
    if !algorithm.eq_ignore_ascii_case("rsa-sha256") {
        return Err(DkimError::UnsupportedAlgorithm(algorithm));
    }

    // c= is "header/body"; either half may be omitted and defaults to simple.
    let c = tags.get("c").cloned().unwrap_or_else(|| "simple/simple".into());
    let (hc, bc) = c.split_once('/').unwrap_or((c.as_str(), "simple"));

    Ok(Signature_ {
        domain: get("d")?,
        selector: get("s")?,
        headers: get("h")?.split(':').map(|h| h.trim().to_string()).collect(),
        body_hash: get("bh")?,
        signature: get("b")?,
        header_canon: Canon::parse(hc).unwrap_or(Canon::Simple),
        body_canon: Canon::parse(bc).unwrap_or(Canon::Simple),
        raw_value: value.to_string(),
    })
}

/// Where the public key lives.
pub fn key_record_name(sig: &Signature_) -> String {
    format!("{}._domainkey.{}", sig.selector, sig.domain)
}

/// Verify a message against a public key already fetched from DNS.
///
/// Split from the DNS lookup deliberately: verification is pure, so it is
/// testable without a resolver, and the network half cannot hide a logic bug.
pub fn verify_with_key(raw: &[u8], key_record: &str) -> Result<Signature_> {
    let parsed = nexus_mailmsg::parse(raw, nexus_mailmsg::Limits::default())
        .map_err(|_| DkimError::Malformed)?;

    let raw_sig = parsed
        .headers
        .get("dkim-signature")
        .ok_or(DkimError::NoSignature)?;
    let sig = parse_signature(raw_sig)?;

    // 1. The body hash. Checked first because a mismatch here means the body
    //    changed in transit, which is a different diagnosis from a forged
    //    signature and worth reporting separately.
    let computed_bh = STANDARD.encode(Sha256::digest(canon_body(sig.body_canon, &parsed.body)));
    if computed_bh != sig.body_hash {
        return Err(DkimError::BodyHashMismatch);
    }

    // 2. Rebuild exactly what the signer hashed: the listed headers in the
    //    order h= gives them, then the DKIM-Signature itself with b= emptied.
    let mut to_verify = Vec::new();
    for name in &sig.headers {
        if let Some(value) = parsed.headers.get(name) {
            to_verify.extend_from_slice(canon_header(sig.header_canon, name, value).as_bytes());
        }
    }
    // The single space after the colon is restored deliberately.
    //
    // Our header parser unfolds and left-trims values, so the original bytes of
    // the DKIM-Signature line are gone by the time we get here. Under `relaxed`
    // that does not matter — canonicalization trims anyway — but `simple` hashes
    // the field verbatim, so signer and verifier must agree on that whitespace
    // or every simple-canonicalized signature fails.
    //
    // One space is what `Name: value` means in practice: every generator emits
    // it, including ours. A sender who emitted `DKIM-Signature:v=1` with no
    // space would fail simple verification here, which is the limitation this
    // comment exists to record.
    let stripped = strip_b_value(&sig.raw_value);
    to_verify.extend_from_slice(
        canon_header(sig.header_canon, "DKIM-Signature", &format!(" {stripped}"))
            .trim_end_matches("\r\n")
            .as_bytes(),
    );

    // 3. The key, from the DNS TXT record's p= tag.
    let public_key = public_key_from_record(key_record)?;
    let signature_bytes = STANDARD
        .decode(sig.signature.as_bytes())
        .map_err(|_| DkimError::SignatureMismatch)?;
    let signature =
        Signature::try_from(signature_bytes.as_slice()).map_err(|_| DkimError::SignatureMismatch)?;

    VerifyingKey::<Sha256>::new(public_key)
        .verify(&to_verify, &signature)
        .map_err(|_| DkimError::SignatureMismatch)?;

    Ok(sig)
}

/// Remove the b= value while keeping the tag, which is what the signer hashed.
fn strip_b_value(value: &str) -> String {
    let mut out = String::with_capacity(value.len());
    for (i, part) in value.split(';').enumerate() {
        if i > 0 {
            out.push(';');
        }
        let trimmed = part.trim_start();
        if trimmed.starts_with("b=") {
            // Preserve the leading whitespace the signer had, then an empty b=.
            let lead = &part[..part.len() - trimmed.len()];
            out.push_str(lead);
            out.push_str("b=");
        } else {
            out.push_str(part);
        }
    }
    out
}

/// Extract the RSA public key from a DKIM DNS TXT record.
pub fn public_key_from_record(record: &str) -> Result<RsaPublicKey> {
    let mut p = None;
    for part in record.split(';') {
        if let Some((k, v)) = part.split_once('=') {
            if k.trim() == "p" {
                p = Some(v.chars().filter(|c| !c.is_whitespace()).collect::<String>());
            }
        }
    }
    // An empty p= is how a key is revoked, and must fail rather than be
    // treated as "no opinion".
    let p = p.filter(|s| !s.is_empty()).ok_or_else(|| {
        DkimError::BadPublicKey("record has no p= tag, or the key is revoked".into())
    })?;

    let der = STANDARD
        .decode(p.as_bytes())
        .map_err(|e| DkimError::BadPublicKey(e.to_string()))?;
    RsaPublicKey::from_public_key_der(&der).map_err(|e| DkimError::BadPublicKey(e.to_string()))
}
