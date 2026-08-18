use base64::{engine::general_purpose::STANDARD, Engine};
use rsa::pkcs8::{EncodePrivateKey, EncodePublicKey, LineEnding};
use rsa::{RsaPrivateKey, RsaPublicKey};

use crate::error::{DkimError, Result};

/// Generate a signing key.
///
/// 2048 bits: 1024 is still accepted but is being retired by large receivers,
/// and anything larger than 2048 does not fit comfortably in a single DNS TXT
/// string, which is a practical constraint people discover the hard way.
pub fn generate(bits: usize) -> Result<RsaPrivateKey> {
    RsaPrivateKey::new(&mut rand::thread_rng(), bits)
        .map_err(|e| DkimError::BadPublicKey(e.to_string()))
}

pub fn private_key_pem(key: &RsaPrivateKey) -> Result<String> {
    key.to_pkcs8_pem(LineEnding::LF)
        .map(|p| p.to_string())
        .map_err(|e| DkimError::BadPublicKey(e.to_string()))
}

/// The DNS TXT record to publish at `<selector>._domainkey.<domain>`.
pub fn public_key_record(key: &RsaPrivateKey) -> Result<String> {
    let der = RsaPublicKey::from(key)
        .to_public_key_der()
        .map_err(|e| DkimError::BadPublicKey(e.to_string()))?;
    Ok(format!("v=DKIM1; k=rsa; p={}", STANDARD.encode(der.as_bytes())))
}
