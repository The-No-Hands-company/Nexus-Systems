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
    #[error("permanent failure delivering to {recipient}: {reason}")]
    Permanent { recipient: String, reason: String },

    /// Delivery failed in a way that may succeed later — peer unreachable,
    /// temporary refusal. The queue backs off and tries again.
    #[error("temporary failure delivering to {recipient}: {reason}")]
    Temporary { recipient: String, reason: String },
}

pub type Result<T> = std::result::Result<T, DeliveryError>;
