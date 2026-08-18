use thiserror::Error;

#[derive(Debug, Error, PartialEq, Eq)]
pub enum DkimError {
    #[error("message could not be parsed")]
    Malformed,

    #[error("no DKIM-Signature header")]
    NoSignature,

    #[error("DKIM-Signature is missing required tag {0}")]
    MissingTag(&'static str),

    #[error("unsupported algorithm: {0}")]
    UnsupportedAlgorithm(String),

    /// The body changed in transit. Reported separately from a bad signature
    /// because it points at a different cause — a relay rewriting content
    /// rather than a forgery.
    #[error("body hash does not match")]
    BodyHashMismatch,

    #[error("signature does not verify")]
    SignatureMismatch,

    #[error("no public key at {0}")]
    NoPublicKey(String),

    #[error("public key is unusable: {0}")]
    BadPublicKey(String),
}

pub type Result<T> = std::result::Result<T, DkimError>;
