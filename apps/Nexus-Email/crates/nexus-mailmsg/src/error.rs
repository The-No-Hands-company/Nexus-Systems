use thiserror::Error;

#[derive(Debug, Error, PartialEq, Eq)]
pub enum MsgError {
    /// The input had no header/body separator at all. Everything else about a
    /// malformed message is recoverable; this is not, because there is no way
    /// to tell headers from body.
    #[error("no header/body separator found")]
    NoSeparator,

    #[error("header block exceeds the {limit} byte limit")]
    HeadersTooLarge { limit: usize },

    #[error("message exceeds the {limit} byte limit")]
    MessageTooLarge { limit: usize },
}

pub type Result<T> = std::result::Result<T, MsgError>;
