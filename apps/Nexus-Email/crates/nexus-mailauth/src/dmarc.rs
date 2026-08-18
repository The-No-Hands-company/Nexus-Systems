use crate::dns::{DnsError, Lookup};
use crate::spf::SpfResult;

/// What a domain asks receivers to do with mail that fails DMARC.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Policy {
    /// Report only. The overwhelming majority of published policies, and it
    /// means "we are still watching" — treating it as reject breaks real mail.
    None,
    Quarantine,
    Reject,
}

impl Policy {
    fn parse(s: &str) -> Option<Self> {
        match s.trim().to_ascii_lowercase().as_str() {
            "none" => Some(Policy::None),
            "quarantine" => Some(Policy::Quarantine),
            "reject" => Some(Policy::Reject),
            _ => None,
        }
    }
}

/// How closely an authenticated domain must match the From domain.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Alignment {
    /// Same organizational domain. The default, and what most senders need
    /// because mail is commonly sent from a subdomain.
    Relaxed,
    /// Exactly equal.
    Strict,
}

#[derive(Debug, Clone)]
pub struct DmarcRecord {
    pub policy: Policy,
    /// Policy for subdomains, if it differs.
    pub subdomain_policy: Option<Policy>,
    pub dkim_alignment: Alignment,
    pub spf_alignment: Alignment,
    /// Percentage of failing mail the policy applies to.
    pub percent: u8,
}

impl Default for DmarcRecord {
    fn default() -> Self {
        Self {
            policy: Policy::None,
            subdomain_policy: None,
            dkim_alignment: Alignment::Relaxed,
            spf_alignment: Alignment::Relaxed,
            percent: 100,
        }
    }
}

pub fn parse_record(txt: &str) -> Option<DmarcRecord> {
    if !txt.trim_start().to_ascii_lowercase().starts_with("v=dmarc1") {
        return None;
    }
    let mut record = DmarcRecord::default();

    for part in txt.split(';') {
        let Some((k, v)) = part.split_once('=') else { continue };
        match k.trim().to_ascii_lowercase().as_str() {
            "p" => record.policy = Policy::parse(v)?,
            "sp" => record.subdomain_policy = Policy::parse(v),
            "adkim" => {
                record.dkim_alignment =
                    if v.trim().eq_ignore_ascii_case("s") { Alignment::Strict } else { Alignment::Relaxed }
            }
            "aspf" => {
                record.spf_alignment =
                    if v.trim().eq_ignore_ascii_case("s") { Alignment::Strict } else { Alignment::Relaxed }
            }
            "pct" => record.percent = v.trim().parse().unwrap_or(100).min(100),
            _ => {}
        }
    }
    Some(record)
}

/// The outcome of a DMARC evaluation.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct DmarcResult {
    pub pass: bool,
    /// What the domain asks us to do when it fails. Meaningless on a pass.
    pub policy: Policy,
    pub dkim_aligned: bool,
    pub spf_aligned: bool,
    /// No DMARC record published.
    pub none: bool,
}

/// Evaluate DMARC.
///
/// DMARC passes when **either** DKIM or SPF passes *and* is aligned with the
/// From domain. Either, not both — that is the whole design, and it is what
/// lets mail survive forwarding, which breaks SPF while leaving DKIM intact.
///
/// Alignment is the part that carries the security: an unaligned DKIM pass
/// only proves somebody signed the message, not that the domain in From did.
/// Without the alignment check, any attacker could sign with their own domain
/// and claim a DMARC pass for yours.
pub async fn evaluate<L: Lookup>(
    dns: &L,
    from_domain: &str,
    dkim_domain: Option<&str>,
    dkim_passed: bool,
    spf_domain: Option<&str>,
    spf_result: SpfResult,
) -> DmarcResult {
    let record = match find_record(dns, from_domain).await {
        Some(r) => r,
        None => {
            return DmarcResult {
                pass: false,
                policy: Policy::None,
                dkim_aligned: false,
                spf_aligned: false,
                none: true,
            }
        }
    };

    let dkim_aligned = dkim_passed
        && dkim_domain
            .map(|d| aligned(d, from_domain, record.dkim_alignment))
            .unwrap_or(false);

    let spf_aligned = spf_result == SpfResult::Pass
        && spf_domain
            .map(|d| aligned(d, from_domain, record.spf_alignment))
            .unwrap_or(false);

    DmarcResult {
        pass: dkim_aligned || spf_aligned,
        policy: record.policy,
        dkim_aligned,
        spf_aligned,
        none: false,
    }
}

async fn find_record<L: Lookup>(dns: &L, domain: &str) -> Option<DmarcRecord> {
    match dns.txt(&format!("_dmarc.{domain}")).await {
        Ok(records) => records.iter().find_map(|r| parse_record(r)),
        Err(DnsError::NotFound) | Err(DnsError::Failed(_)) => None,
    }
}

/// Whether an authenticated domain aligns with the From domain.
pub fn aligned(authenticated: &str, from: &str, mode: Alignment) -> bool {
    let a = authenticated.trim_end_matches('.').to_ascii_lowercase();
    let f = from.trim_end_matches('.').to_ascii_lowercase();

    match mode {
        Alignment::Strict => a == f,
        Alignment::Relaxed => organizational_domain(&a) == organizational_domain(&f),
    }
}

/// Approximate the registrable domain.
///
/// A correct implementation needs the Public Suffix List, which is a
/// downloaded, frequently-changing dataset. Without it, naive "last two
/// labels" would treat `a.co.uk` and `b.co.uk` as the same organization — and
/// under DMARC that means accepting forged mail as aligned, which is the exact
/// failure this check exists to prevent.
///
/// So a small set of common multi-label suffixes is handled explicitly, and
/// everything else falls back to two labels. This is a known approximation,
/// documented rather than hidden; adopting a real PSL is its own piece of work.
pub fn organizational_domain(domain: &str) -> String {
    let labels: Vec<&str> = domain.split('.').filter(|l| !l.is_empty()).collect();
    if labels.len() <= 2 {
        return labels.join(".");
    }

    const TWO_LABEL_SUFFIXES: &[&str] = &[
        "co.uk", "org.uk", "gov.uk", "ac.uk", "me.uk", "net.uk", "sch.uk",
        "com.au", "net.au", "org.au", "edu.au", "gov.au",
        "co.nz", "net.nz", "org.nz", "govt.nz",
        "co.jp", "or.jp", "ne.jp", "ac.jp", "go.jp",
        "co.za", "org.za", "gov.za",
        "com.br", "net.br", "org.br", "gov.br",
        "co.in", "net.in", "org.in", "gov.in",
        "com.cn", "net.cn", "org.cn", "gov.cn",
        "com.mx", "com.ar", "com.tr", "com.sg", "com.hk", "com.tw",
    ];

    let last_two = labels[labels.len() - 2..].join(".");
    if TWO_LABEL_SUFFIXES.contains(&last_two.as_str()) && labels.len() >= 3 {
        return labels[labels.len() - 3..].join(".");
    }
    last_two
}
