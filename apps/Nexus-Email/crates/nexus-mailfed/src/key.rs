use std::path::Path;

use base64::{engine::general_purpose::STANDARD, Engine};
use ed25519_dalek::{Signature, Signer, SigningKey};
use rand::RngCore;

use crate::error::{FedError, Result};

/// This node's mail key: the identity peers pin.
///
/// Losing or replacing it breaks every peer that pinned the old public half,
/// so it is created exactly once and never silently regenerated.
pub struct NodeKey {
    signing: SigningKey,
}

impl NodeKey {
    pub fn generate() -> Self {
        let mut seed = [0u8; 32];
        rand::rngs::OsRng.fill_bytes(&mut seed);
        Self { signing: SigningKey::from_bytes(&seed) }
    }

    /// Load the key at `path`, creating it if the file does not exist.
    ///
    /// A file that exists but is not a key is an error, never overwritten:
    /// replacing it would change this node's identity without anyone noticing
    /// until every peer started refusing our mail.
    pub fn load_or_create(path: &Path) -> Result<Self> {
        match std::fs::read(path) {
            Ok(bytes) => {
                let seed: [u8; 32] = bytes
                    .as_slice()
                    .try_into()
                    .map_err(|_| FedError::Key(format!("{} is not a 32-byte key", path.display())))?;
                Ok(Self { signing: SigningKey::from_bytes(&seed) })
            }
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
                if let Some(dir) = path.parent() {
                    std::fs::create_dir_all(dir)?;
                }
                let key = Self::generate();
                write_private(path, &key.signing.to_bytes())?;
                Ok(key)
            }
            Err(e) => Err(e.into()),
        }
    }

    /// The public half, base64 — what an operator gives to peers.
    pub fn public_b64(&self) -> String {
        STANDARD.encode(self.signing.verifying_key().to_bytes())
    }

    pub(crate) fn sign_bytes(&self, msg: &[u8]) -> Signature {
        self.signing.sign(msg)
    }
}

#[cfg(unix)]
fn write_private(path: &Path, bytes: &[u8]) -> Result<()> {
    use std::io::Write;
    use std::os::unix::fs::OpenOptionsExt;
    // create_new: never clobber a key that appeared between the read and here.
    // The mode is best effort — on the NTFS mount this repo lives on, it is
    // silently ignored, so the key directory belongs outside the repo.
    let mut f = std::fs::OpenOptions::new().write(true).create_new(true).mode(0o600).open(path)?;
    f.write_all(bytes)?;
    Ok(())
}

#[cfg(not(unix))]
fn write_private(path: &Path, bytes: &[u8]) -> Result<()> {
    use std::io::Write;
    let mut f = std::fs::OpenOptions::new().write(true).create_new(true).open(path)?;
    f.write_all(bytes)?;
    Ok(())
}
