use thiserror::Error;

#[derive(Debug, Error)]
pub enum MailStoreError {
    #[error("database error: {0}")]
    Database(#[from] sqlx::Error),

    /// The address could not be parsed into a localpart and a domain.
    #[error("malformed address: {0}")]
    MalformedAddress(String),

    /// No address record routes this recipient to a mailbox. Distinct from a
    /// database error: it is the ordinary answer for mail addressed to
    /// somebody who does not exist here, and the SMTP layer must be able to
    /// tell the two apart to choose between a 550 and a 451.
    #[error("no mailbox routes {0}")]
    NoSuchAddress(String),
}

pub type Result<T> = std::result::Result<T, MailStoreError>;
