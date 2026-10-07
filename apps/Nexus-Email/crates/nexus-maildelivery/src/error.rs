use thiserror::Error;

#[derive(Debug, Error)]
pub enum DeliveryError {
    #[error(transparent)]
    Store(#[from] nexus_mailstore::MailStoreError),

    #[error("database error: {0}")]
    Database(#[from] sqlx::Error),

    #[error("message could not be parsed: {0}")]
    Message(#[from] nexus_mailmsg::MsgError),

    /// Delivery failed in a way that will never succeed — no such mailbox, the
    /// domain does not exist. The sender gets a bounce and the queue stops.
    #[error("permanent delivery failure: {reason}")]
    Permanent { recipient: String, reason: String },

    /// Delivery failed in a way that may succeed later — peer unreachable,
    /// temporary refusal. The queue backs off and tries again.
    #[error("temporary delivery failure: {reason}")]
    Temporary { recipient: String, reason: String },
}

impl DeliveryError {
    /// The full message, recipient included. For answering the person who sent
    /// the mail (their own data); never for a log. `Display` omits addresses
    /// so that every `{e}` in a log line is safe by construction.
    pub fn detail_for_user(&self) -> String {
        match self {
            Self::Permanent { recipient, reason } => format!("permanent failure delivering to {recipient}: {reason}"),
            Self::Temporary { recipient, reason } => format!("temporary failure delivering to {recipient}: {reason}"),
            other => other.to_string(),
        }
    }
}

pub type Result<T> = std::result::Result<T, DeliveryError>;
