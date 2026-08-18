use hickory_resolver::TokioAsyncResolver;

/// A destination to try, in the order the RFC says to try them.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MailExchanger {
    pub host: String,
    pub preference: u16,
}

#[derive(Debug, thiserror::Error)]
pub enum MxError {
    /// The domain exists but publishes no way to receive mail. Permanent: it
    /// will not sprout an MX record on a retry.
    #[error("{0} has no mail exchanger")]
    NoMailExchanger(String),
    /// DNS itself failed. Temporary — a resolver hiccup must not bounce mail.
    #[error("dns lookup failed for {domain}: {reason}")]
    Lookup { domain: String, reason: String },
}

/// Resolve where to deliver mail for a domain.
///
/// Sorted by preference, lowest first, because that is the sender's stated
/// order and ignoring it routes around a primary server that wanted the mail.
/// When a domain has no MX record, its A record is used — the implicit MX rule
/// in RFC 5321 §5.1, which plenty of small domains still rely on.
pub async fn resolve(resolver: &TokioAsyncResolver, domain: &str) -> Result<Vec<MailExchanger>, MxError> {
    match resolver.mx_lookup(domain).await {
        Ok(records) => {
            let mut hosts: Vec<MailExchanger> = records
                .iter()
                .map(|r| MailExchanger {
                    host: r.exchange().to_utf8().trim_end_matches('.').to_string(),
                    preference: r.preference(),
                })
                // A single "." exchange is an explicit statement that the
                // domain accepts no mail at all (RFC 7505), not a host to try.
                .filter(|m| !m.host.is_empty())
                .collect();

            if hosts.is_empty() {
                return Err(MxError::NoMailExchanger(domain.to_string()));
            }
            hosts.sort_by_key(|m| m.preference);
            Ok(hosts)
        }
        Err(e) => {
            // No MX is not the same as no DNS. Fall back to the A record before
            // giving up, then decide whether this was permanent or temporary.
            if resolver.lookup_ip(domain).await.is_ok() {
                return Ok(vec![MailExchanger { host: domain.to_string(), preference: 0 }]);
            }
            // "No records" means the domain genuinely publishes no way to
            // receive mail, which is permanent. Every other resolver error is a
            // DNS problem, and bouncing mail because a resolver blinked would
            // be throwing away deliverable mail.
            if matches!(e.kind(), hickory_resolver::error::ResolveErrorKind::NoRecordsFound { .. }) {
                Err(MxError::NoMailExchanger(domain.to_string()))
            } else {
                Err(MxError::Lookup { domain: domain.to_string(), reason: e.to_string() })
            }
        }
    }
}
