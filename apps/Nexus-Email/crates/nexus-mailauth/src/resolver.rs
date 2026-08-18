use std::net::IpAddr;

use hickory_resolver::error::ResolveErrorKind;
use hickory_resolver::TokioAsyncResolver;

use crate::dns::{DnsError, Lookup};

/// The real DNS, behind the same trait the tests fake.
pub struct SystemDns {
    resolver: TokioAsyncResolver,
}

impl SystemDns {
    pub fn new() -> Self {
        Self {
            resolver: TokioAsyncResolver::tokio_from_system_conf()
                .unwrap_or_else(|_| TokioAsyncResolver::tokio(Default::default(), Default::default())),
        }
    }
}

impl Default for SystemDns {
    fn default() -> Self {
        Self::new()
    }
}

/// Distinguish "there is no such record" from "the lookup failed".
///
/// The difference decides whether a sender is judged or asked to retry: a
/// missing SPF record is an answer, while a resolver failure must become
/// temperror so we never reject mail over our own DNS wobbling.
fn classify(e: &hickory_resolver::error::ResolveError) -> DnsError {
    match e.kind() {
        ResolveErrorKind::NoRecordsFound { .. } => DnsError::NotFound,
        other => DnsError::Failed(other.to_string()),
    }
}

impl Lookup for SystemDns {
    async fn txt(&self, name: &str) -> Result<Vec<String>, DnsError> {
        match self.resolver.txt_lookup(name).await {
            Ok(res) => Ok(res
                .iter()
                // A TXT record is a list of strings that must be concatenated:
                // anything longer than 255 bytes is split, and DKIM keys always
                // are. Joining with a separator would corrupt every long record.
                .map(|txt| {
                    txt.iter()
                        .map(|b| String::from_utf8_lossy(b).into_owned())
                        .collect::<Vec<_>>()
                        .concat()
                })
                .collect()),
            Err(e) => Err(classify(&e)),
        }
    }

    async fn a(&self, name: &str) -> Result<Vec<IpAddr>, DnsError> {
        match self.resolver.lookup_ip(name).await {
            Ok(res) => Ok(res.iter().collect()),
            Err(e) => Err(classify(&e)),
        }
    }

    async fn mx(&self, name: &str) -> Result<Vec<String>, DnsError> {
        match self.resolver.mx_lookup(name).await {
            Ok(res) => Ok(res
                .iter()
                .map(|r| r.exchange().to_utf8().trim_end_matches('.').to_string())
                .collect()),
            Err(e) => Err(classify(&e)),
        }
    }
}
