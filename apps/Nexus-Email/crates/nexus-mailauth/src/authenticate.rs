use std::net::IpAddr;

use crate::dmarc::{evaluate as dmarc_evaluate, Policy};
use crate::dns::Lookup;
use crate::spf::{evaluate as spf_evaluate, SpfResult};
use crate::verify::{key_record_name, parse_signature, verify_with_key};
use crate::DkimError;

/// What the receiving server should do with a message.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Verdict {
    /// Deliver normally.
    Accept,
    /// Deliver, but not to the inbox. The domain asked for suspicious mail to
    /// be set aside rather than destroyed, and honouring that literally is the
    /// difference between quarantine and reject.
    Quarantine,
    /// Refuse at SMTP time. The domain published p=reject and this message
    /// failed, so the sender is told rather than left believing it arrived.
    Reject,
}

/// Everything learned about a message's provenance.
#[derive(Debug, Clone)]
pub struct Authentication {
    pub spf: SpfResult,
    pub spf_domain: Option<String>,
    pub dkim_passed: bool,
    pub dkim_domain: Option<String>,
    pub dkim_error: Option<String>,
    pub dmarc_pass: bool,
    pub dmarc_policy: Policy,
    pub dmarc_published: bool,
    pub verdict: Verdict,
}

impl Authentication {
    /// The Authentication-Results header to prepend (RFC 8601).
    ///
    /// Recorded even when everything passes: a later dispute about whether a
    /// message was authentic is unanswerable if the evidence was thrown away
    /// at delivery time.
    pub fn header(&self, receiving_host: &str) -> String {
        let spf = match self.spf {
            SpfResult::Pass => "pass",
            SpfResult::Fail => "fail",
            SpfResult::SoftFail => "softfail",
            SpfResult::Neutral => "neutral",
            SpfResult::None => "none",
            SpfResult::TempError => "temperror",
            SpfResult::PermError => "permerror",
        };
        let dkim = if self.dkim_domain.is_none() {
            "none"
        } else if self.dkim_passed {
            "pass"
        } else {
            "fail"
        };
        let dmarc = if !self.dmarc_published {
            "none"
        } else if self.dmarc_pass {
            "pass"
        } else {
            "fail"
        };

        let mut out = format!("Authentication-Results: {receiving_host};\r\n\tspf={spf}");
        if let Some(d) = &self.spf_domain {
            out.push_str(&format!(" smtp.mailfrom={d}"));
        }
        out.push_str(&format!(";\r\n\tdkim={dkim}"));
        if let Some(d) = &self.dkim_domain {
            out.push_str(&format!(" header.d={d}"));
        }
        out.push_str(&format!(";\r\n\tdmarc={dmarc}"));
        if self.dmarc_published {
            let p = match self.dmarc_policy {
                Policy::None => "none",
                Policy::Quarantine => "quarantine",
                Policy::Reject => "reject",
            };
            out.push_str(&format!(" p={p}"));
        }
        out.push_str("\r\n");
        out
    }
}

/// Authenticate an inbound message.
///
/// The order matters: SPF and DKIM are evidence, DMARC is the judgement. Only
/// DMARC decides anything, because only DMARC involves the From domain — the
/// address a human actually reads. A DKIM or SPF pass on its own says nothing
/// about who the message claims to be from.
pub async fn authenticate<L: Lookup>(
    dns: &L,
    client_ip: IpAddr,
    helo: &str,
    envelope_from: &str,
    raw_message: &[u8],
) -> Authentication {
    // SPF is evaluated against the envelope sender's domain, not the From
    // header. They are frequently different and conflating them is a classic
    // bug: forwarders rewrite the envelope and leave From alone.
    let spf_domain = domain_of(envelope_from);
    let spf = match &spf_domain {
        Some(d) => spf_evaluate(dns, client_ip, d, helo).await,
        // A null reverse path (a bounce) has no domain to check, so SPF falls
        // back to the HELO name.
        None => spf_evaluate(dns, client_ip, helo, helo).await,
    };

    let (dkim_passed, dkim_domain, dkim_error) = verify_dkim(dns, raw_message).await;

    let from_domain = header_from_domain(raw_message);
    let dmarc = match &from_domain {
        Some(fd) => {
            dmarc_evaluate(
                dns,
                fd,
                dkim_domain.as_deref(),
                dkim_passed,
                spf_domain.as_deref(),
                spf,
            )
            .await
        }
        None => crate::dmarc::DmarcResult {
            pass: false,
            policy: Policy::None,
            dkim_aligned: false,
            spf_aligned: false,
            none: true,
        },
    };

    // A message with no usable From header is malformed. It is not rejected
    // here — that is the SMTP layer's business — but it can never pass DMARC,
    // because there is no identity to align against.
    let verdict = if dmarc.none || dmarc.pass {
        Verdict::Accept
    } else {
        match dmarc.policy {
            // p=none means "we are watching". Acting on it would break mail
            // from every domain still rolling DMARC out, which is most of them.
            Policy::None => Verdict::Accept,
            Policy::Quarantine => Verdict::Quarantine,
            Policy::Reject => Verdict::Reject,
        }
    };

    Authentication {
        spf,
        spf_domain,
        dkim_passed,
        dkim_domain,
        dkim_error,
        dmarc_pass: dmarc.pass,
        dmarc_policy: dmarc.policy,
        dmarc_published: !dmarc.none,
        verdict,
    }
}

async fn verify_dkim<L: Lookup>(
    dns: &L,
    raw: &[u8],
) -> (bool, Option<String>, Option<String>) {
    let Ok(parsed) = nexus_mailmsg::parse(raw, nexus_mailmsg::Limits::default()) else {
        return (false, None, Some("message could not be parsed".into()));
    };
    let Some(raw_sig) = parsed.headers.get("dkim-signature") else {
        return (false, None, None); // unsigned is not a failure
    };
    let sig = match parse_signature(raw_sig) {
        Ok(s) => s,
        Err(e) => return (false, None, Some(e.to_string())),
    };

    let record_name = key_record_name(&sig);
    let records = match dns.txt(&record_name).await {
        Ok(r) => r,
        Err(_) => {
            return (
                false,
                Some(sig.domain.clone()),
                Some(format!("no key at {record_name}")),
            )
        }
    };

    for record in records {
        match verify_with_key(raw, &record) {
            Ok(_) => return (true, Some(sig.domain.clone()), None),
            // Keep trying: a domain may publish several keys during a rotation,
            // and stopping at the first mismatch would fail mail signed with
            // the newer one.
            Err(DkimError::SignatureMismatch) | Err(DkimError::BadPublicKey(_)) => continue,
            Err(e) => return (false, Some(sig.domain.clone()), Some(e.to_string())),
        }
    }
    (false, Some(sig.domain), Some("no published key verified the signature".into()))
}

fn domain_of(address: &str) -> Option<String> {
    let a = address.trim().trim_matches(['<', '>']);
    if a.is_empty() {
        return None;
    }
    a.rsplit_once('@').map(|(_, d)| d.to_ascii_lowercase())
}

/// The domain of the From header — the identity DMARC judges.
fn header_from_domain(raw: &[u8]) -> Option<String> {
    let parsed = nexus_mailmsg::parse(raw, nexus_mailmsg::Limits::default()).ok()?;
    let from = parsed.headers.get("from")?;
    // `Name <addr@domain>` or a bare address.
    let inside = match (from.find('<'), from.find('>')) {
        (Some(o), Some(c)) if c > o => &from[o + 1..c],
        _ => from.trim(),
    };
    domain_of(inside)
}
