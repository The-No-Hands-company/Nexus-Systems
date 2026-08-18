use std::net::IpAddr;

use crate::dns::{DnsError, Lookup};

/// An SPF result (RFC 7208 §2.6).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SpfResult {
    /// The sender is authorised.
    Pass,
    /// Explicitly not authorised.
    Fail,
    /// Probably not authorised, but the domain is not ready to say so.
    SoftFail,
    /// The domain publishes a policy that takes no position on this sender.
    Neutral,
    /// No SPF record at all. Not a failure — most domains still have none.
    None,
    /// DNS trouble. The sender should be asked to retry, never rejected: a
    /// resolver problem is our fault, not theirs.
    TempError,
    /// The record is broken, or the evaluation exceeded its budget.
    PermError,
}

/// RFC 7208 §4.6.4: at most ten DNS-consuming mechanisms per evaluation.
///
/// This is a security control, not tidiness. SPF is a recursive language over
/// records other people control, so without a hard budget one lookup can be
/// made to fan out into thousands — which is a denial-of-service amplifier
/// pointed at both us and whoever the records name.
pub const MAX_DNS_LOOKUPS: usize = 10;

/// RFC 7208 §4.6.4: at most two mechanisms may return "no record".
const MAX_VOID_LOOKUPS: usize = 2;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Qualifier {
    Pass,
    Fail,
    SoftFail,
    Neutral,
}

impl Qualifier {
    fn parse(c: char) -> Option<Self> {
        match c {
            '+' => Some(Qualifier::Pass),
            '-' => Some(Qualifier::Fail),
            '~' => Some(Qualifier::SoftFail),
            '?' => Some(Qualifier::Neutral),
            _ => None,
        }
    }

    fn to_result(self) -> SpfResult {
        match self {
            Qualifier::Pass => SpfResult::Pass,
            Qualifier::Fail => SpfResult::Fail,
            Qualifier::SoftFail => SpfResult::SoftFail,
            Qualifier::Neutral => SpfResult::Neutral,
        }
    }
}

/// Evaluate SPF for a sending IP and the domain of the envelope sender.
pub async fn evaluate<L: Lookup>(
    dns: &L,
    ip: IpAddr,
    domain: &str,
    helo: &str,
) -> SpfResult {
    let mut budget = Budget::default();
    match check_host(dns, ip, domain, helo, &mut budget, 0).await {
        Ok(r) => r,
        Err(e) => e,
    }
}

#[derive(Default)]
struct Budget {
    lookups: usize,
    voids: usize,
}

impl Budget {
    /// Charge one DNS-consuming mechanism, refusing once the budget is spent.
    fn spend(&mut self) -> Result<(), SpfResult> {
        self.lookups += 1;
        if self.lookups > MAX_DNS_LOOKUPS {
            return Err(SpfResult::PermError);
        }
        Ok(())
    }

    fn void(&mut self) -> Result<(), SpfResult> {
        self.voids += 1;
        if self.voids > MAX_VOID_LOOKUPS {
            return Err(SpfResult::PermError);
        }
        Ok(())
    }
}

/// Recursion cap, independent of the lookup budget: `include` chains can be
/// arranged to recurse without each step spending a lookup we count.
const MAX_DEPTH: usize = 10;

async fn check_host<L: Lookup>(
    dns: &L,
    ip: IpAddr,
    domain: &str,
    helo: &str,
    budget: &mut Budget,
    depth: usize,
) -> Result<SpfResult, SpfResult> {
    if depth > MAX_DEPTH {
        return Err(SpfResult::PermError);
    }

    let record = match find_spf_record(dns, domain).await {
        Ok(Some(r)) => r,
        Ok(None) => return Ok(SpfResult::None),
        Err(e) => return Err(e),
    };

    let mut redirect: Option<String> = None;

    for term in record.split_whitespace().skip(1) {
        // Modifiers are name=value; mechanisms are not.
        if let Some((name, value)) = term.split_once('=') {
            if name.eq_ignore_ascii_case("redirect") {
                redirect = Some(value.to_string());
            }
            // exp= only supplies an explanation string; it changes no result.
            continue;
        }

        let (qualifier, mechanism) = match Qualifier::parse(term.chars().next().unwrap_or('+')) {
            Some(q) => (q, &term[1..]),
            None => (Qualifier::Pass, term),
        };

        let (name, arg) = match mechanism.split_once(':') {
            Some((n, a)) => (n, Some(a)),
            None => (mechanism, None),
        };

        let matched = match name.to_ascii_lowercase().as_str() {
            "all" => true,
            "ip4" | "ip6" => arg.map(|a| ip_matches(ip, a)).unwrap_or(false),
            "a" => {
                budget.spend()?;
                let target = arg.unwrap_or(domain);
                match dns.a(target).await {
                    Ok(addrs) => addrs.contains(&ip),
                    Err(DnsError::NotFound) => {
                        budget.void()?;
                        false
                    }
                    Err(DnsError::Failed(_)) => return Err(SpfResult::TempError),
                }
            }
            "mx" => {
                budget.spend()?;
                let target = arg.unwrap_or(domain);
                match dns.mx(target).await {
                    Ok(hosts) => {
                        let mut found = false;
                        for host in hosts {
                            // Each MX host resolved is itself a lookup; without
                            // charging for them, `mx` would be an unbounded
                            // fan-out hiding inside one budgeted mechanism.
                            budget.spend()?;
                            if let Ok(addrs) = dns.a(&host).await {
                                if addrs.contains(&ip) {
                                    found = true;
                                    break;
                                }
                            }
                        }
                        found
                    }
                    Err(DnsError::NotFound) => {
                        budget.void()?;
                        false
                    }
                    Err(DnsError::Failed(_)) => return Err(SpfResult::TempError),
                }
            }
            "include" => {
                budget.spend()?;
                let Some(target) = arg else { return Err(SpfResult::PermError) };
                // include only matches on a pass; anything else falls through
                // to the next mechanism, which is the part people misread.
                match Box::pin(check_host(dns, ip, target, helo, budget, depth + 1)).await {
                    Ok(SpfResult::Pass) => true,
                    Ok(SpfResult::TempError) | Err(SpfResult::TempError) => {
                        return Err(SpfResult::TempError)
                    }
                    Ok(SpfResult::None) => return Err(SpfResult::PermError),
                    Ok(_) => false,
                    Err(e) => return Err(e),
                }
            }
            "exists" => {
                budget.spend()?;
                match arg {
                    Some(target) => match dns.a(target).await {
                        Ok(addrs) => !addrs.is_empty(),
                        Err(DnsError::NotFound) => {
                            budget.void()?;
                            false
                        }
                        Err(DnsError::Failed(_)) => return Err(SpfResult::TempError),
                    },
                    None => return Err(SpfResult::PermError),
                }
            }
            // ptr is deprecated by RFC 7208 §5.5 as slow and unreliable, and
            // treating it as no-match is the recommended handling.
            "ptr" => false,
            _ => return Err(SpfResult::PermError),
        };

        if matched {
            return Ok(qualifier.to_result());
        }
    }

    // No mechanism matched. redirect= takes over as the whole answer.
    if let Some(target) = redirect {
        budget.spend()?;
        return match Box::pin(check_host(dns, ip, &target, helo, budget, depth + 1)).await {
            // A redirect to a domain with no SPF record is an error, not a
            // neutral: the domain said "ask over there" and there was nothing.
            Ok(SpfResult::None) => Err(SpfResult::PermError),
            other => other,
        };
    }

    // The default when a record lists no `all`.
    Ok(SpfResult::Neutral)
}

async fn find_spf_record<L: Lookup>(dns: &L, domain: &str) -> Result<Option<String>, SpfResult> {
    let records = match dns.txt(domain).await {
        Ok(r) => r,
        Err(DnsError::NotFound) => return Ok(None),
        Err(DnsError::Failed(_)) => return Err(SpfResult::TempError),
    };

    let mut found: Option<String> = None;
    for r in records {
        if r.trim_start().to_ascii_lowercase().starts_with("v=spf1") {
            // Two SPF records is ambiguous and RFC 7208 §4.5 makes it an error
            // rather than letting an evaluator pick a favourite.
            if found.is_some() {
                return Err(SpfResult::PermError);
            }
            found = Some(r);
        }
    }
    Ok(found)
}

/// Match an IP against an `ip4:`/`ip6:` term, with or without a prefix length.
fn ip_matches(ip: IpAddr, spec: &str) -> bool {
    let (addr, prefix) = match spec.split_once('/') {
        Some((a, p)) => (a, p.parse::<u32>().ok()),
        None => (spec, None),
    };
    let Ok(target) = addr.parse::<IpAddr>() else {
        return false;
    };

    match (ip, target) {
        (IpAddr::V4(a), IpAddr::V4(b)) => {
            let bits = prefix.unwrap_or(32);
            if bits > 32 {
                return false;
            }
            let mask = if bits == 0 { 0 } else { u32::MAX << (32 - bits) };
            u32::from(a) & mask == u32::from(b) & mask
        }
        (IpAddr::V6(a), IpAddr::V6(b)) => {
            let bits = prefix.unwrap_or(128);
            if bits > 128 {
                return false;
            }
            let mask = if bits == 0 { 0 } else { u128::MAX << (128 - bits) };
            u128::from(a) & mask == u128::from(b) & mask
        }
        // A v4 sender is not matched by a v6 term or vice versa.
        _ => false,
    }
}
