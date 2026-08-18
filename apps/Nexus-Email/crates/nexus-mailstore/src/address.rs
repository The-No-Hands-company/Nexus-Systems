use crate::error::{MailStoreError, Result};

/// A normalised email address.
///
/// Normalisation happens once, here, at the edge — the schema then refuses
/// anything that was not normalised (see the `address_lowercase` check), so a
/// path that forgets to call this fails loudly at the database rather than
/// silently creating a second mailbox for `Info@` alongside `info@`.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Address {
    pub localpart: String,
    pub domain: String,
}

impl Address {
    pub fn parse(raw: &str) -> Result<Self> {
        let trimmed = raw.trim();
        // rsplit_once, not split_once: quoted localparts may legally contain
        // an @, and the domain is whatever follows the final one.
        let (localpart, domain) = trimmed
            .rsplit_once('@')
            .ok_or_else(|| MailStoreError::MalformedAddress(raw.to_string()))?;

        if localpart.is_empty() || domain.is_empty() || domain.contains('@') {
            return Err(MailStoreError::MalformedAddress(raw.to_string()));
        }
        // A domain with no dot is not routable on the public internet, but it
        // is legal in an envelope and normal in testing, so it is accepted
        // here and judged by the delivery layer that actually has to resolve
        // it.
        Ok(Self {
            localpart: localpart.to_lowercase(),
            domain: domain.to_lowercase(),
        })
    }

    pub fn as_string(&self) -> String {
        format!("{}@{}", self.localpart, self.domain)
    }
}

impl std::fmt::Display for Address {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}@{}", self.localpart, self.domain)
    }
}
