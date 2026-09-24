use thiserror::Error;

#[derive(Debug, Error)]
pub enum FedError {
    #[error("node key: {0}")]
    Key(String),
    #[error("signature does not verify")]
    BadSignature,
    #[error("malformed {0}")]
    Malformed(&'static str),
    #[error("io: {0}")]
    Io(#[from] std::io::Error),
}

pub type Result<T> = std::result::Result<T, FedError>;
