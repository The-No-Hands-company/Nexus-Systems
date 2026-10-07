use thiserror::Error;

#[derive(Debug, Error)]
pub enum MailStoreError {
    #[error("database error: {0}")]
    Database(#[from] sqlx::Error),

    /// The address could not be parsed into a localpart and a domain.
    #[error("malformed address")]
    MalformedAddress(String),

    /// No address record routes this recipient to a mailbox. Distinct from a
    /// database error: it is the ordinary answer for mail addressed to
    /// somebody who does not exist here, and the SMTP layer must be able to
    /// tell the two apart to choose between a 550 and a 451.
    #[error("no mailbox routes this address")]
    NoSuchAddress(String),
}

impl MailStoreError {
    /// The full message, address included. For the person who supplied the
    /// address; never for a log (`Display` omits it).
    pub fn detail_for_user(&self) -> String {
        match self {
            Self::MalformedAddress(a) => format!("malformed address: {a}"),
            Self::NoSuchAddress(a) => format!("no mailbox routes {a}"),
            other => other.to_string(),
        }
    }
}

pub type Result<T> = std::result::Result<T, MailStoreError>;
