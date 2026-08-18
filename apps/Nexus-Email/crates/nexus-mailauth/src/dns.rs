use std::future::Future;
use std::net::IpAddr;

/// The DNS a policy evaluation needs.
///
/// A trait rather than a resolver so SPF and DMARC can be tested against a
/// fixed zone. That matters more than usual here: SPF is a recursive language
/// with a lookup budget, and the budget is a security control — testing it
/// against real DNS would be slow, flaky, and dependent on someone else's
/// records staying put.
pub trait Lookup: Send + Sync {
    fn txt(&self, name: &str) -> impl Future<Output = std::result::Result<Vec<String>, DnsError>> + Send;
    fn a(&self, name: &str) -> impl Future<Output = std::result::Result<Vec<IpAddr>, DnsError>> + Send;
    fn mx(&self, name: &str) -> impl Future<Output = std::result::Result<Vec<String>, DnsError>> + Send;
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum DnsError {
    /// The name exists but has no record of this type, or does not exist.
    /// Distinct from a failure: it is an answer.
    NotFound,
    /// The lookup itself failed. In SPF this becomes temperror, so the sender
    /// is asked to try again rather than being judged on missing information.
    Failed(String),
}
