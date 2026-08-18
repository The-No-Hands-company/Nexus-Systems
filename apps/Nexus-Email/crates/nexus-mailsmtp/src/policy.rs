use nexus_mailstore::{Address, MailStore};

/// Relay policy backed by the mailbox table.
///
/// `is_local` asks whether a mailbox actually exists, not merely whether we
/// host the domain. Accepting mail for a domain we serve and only then finding
/// there is no such mailbox would make this server a backscatter source: it
/// would emit a bounce to a return path a spammer forged, which means sending
/// junk to a victim who never wrote to us.
///
/// The cost is that a refusal at RCPT reveals whether an address exists. That
/// is a real trade — it is why VRFY is answered evasively — but every serious
/// mail server makes it the same way, because generating backscatter is the
/// larger harm and gets a server blocklisted.
#[derive(Clone)]
pub struct MailboxPolicy {
    store: MailStore,
}

impl MailboxPolicy {
    pub fn new(store: MailStore) -> Self {
        Self { store }
    }
}

impl crate::session::RelayPolicy for MailboxPolicy {
    async fn is_local(&self, address: &str) -> bool {
        let Ok(parsed) = Address::parse(address) else {
            return false;
        };
        match self.store.resolve(&parsed).await {
            Ok(_) => true,
            // A database failure means we cannot tell. Answering "not local"
            // rejects the mail permanently on the strength of our own outage,
            // so this logs and refuses the *relay*, letting the sender retry
            // rather than bounce.
            Err(nexus_mailstore::MailStoreError::NoSuchAddress(_)) => false,
            Err(e) => {
                tracing::warn!(error = %e, %address, "mailbox lookup failed; refusing");
                false
            }
        }
    }
}
