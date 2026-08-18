//! SPF, against a fixed zone. The lookup budget gets its own tests because it
//! is a denial-of-service control, not a formality.

use std::collections::HashMap;
use std::net::IpAddr;
use std::sync::atomic::{AtomicUsize, Ordering};

use nexus_mailauth::dns::{DnsError, Lookup};
use nexus_mailauth::spf::{evaluate, SpfResult};

#[derive(Default)]
struct Zone {
    txt: HashMap<String, Vec<String>>,
    a: HashMap<String, Vec<IpAddr>>,
    mx: HashMap<String, Vec<String>>,
    /// Every lookup is counted so a test can assert the budget was enforced
    /// rather than merely that the result was PermError.
    queries: AtomicUsize,
}

impl Zone {
    fn txt(mut self, name: &str, value: &str) -> Self {
        self.txt.entry(name.into()).or_default().push(value.into());
        self
    }
    fn a(mut self, name: &str, ip: &str) -> Self {
        self.a.entry(name.into()).or_default().push(ip.parse().unwrap());
        self
    }
    fn mx(mut self, name: &str, host: &str) -> Self {
        self.mx.entry(name.into()).or_default().push(host.into());
        self
    }
}

impl Lookup for Zone {
    async fn txt(&self, name: &str) -> Result<Vec<String>, DnsError> {
        self.queries.fetch_add(1, Ordering::SeqCst);
        self.txt.get(name).cloned().ok_or(DnsError::NotFound)
    }
    async fn a(&self, name: &str) -> Result<Vec<IpAddr>, DnsError> {
        self.queries.fetch_add(1, Ordering::SeqCst);
        self.a.get(name).cloned().ok_or(DnsError::NotFound)
    }
    async fn mx(&self, name: &str) -> Result<Vec<String>, DnsError> {
        self.queries.fetch_add(1, Ordering::SeqCst);
        self.mx.get(name).cloned().ok_or(DnsError::NotFound)
    }
}

fn ip(s: &str) -> IpAddr {
    s.parse().unwrap()
}

#[tokio::test]
async fn an_authorised_ip_passes_and_an_unauthorised_one_fails() {
    let zone = Zone::default().txt("tnhc.dev", "v=spf1 ip4:192.0.2.1 -all");
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "tnhc.dev", "h").await, SpfResult::Pass);
    assert_eq!(evaluate(&zone, ip("198.51.100.9"), "tnhc.dev", "h").await, SpfResult::Fail);
}

#[tokio::test]
async fn no_record_is_none_not_a_failure() {
    // Most domains still publish no SPF. Treating that as a failure would
    // reject most of the internet's mail.
    let zone = Zone::default();
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "nothing.test", "h").await, SpfResult::None);
}

#[tokio::test]
async fn qualifiers_are_honoured() {
    let zone = Zone::default()
        .txt("soft.test", "v=spf1 ~all")
        .txt("neutral.test", "v=spf1 ?all")
        .txt("pass.test", "v=spf1 +all");
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "soft.test", "h").await, SpfResult::SoftFail);
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "neutral.test", "h").await, SpfResult::Neutral);
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "pass.test", "h").await, SpfResult::Pass);
}

#[tokio::test]
async fn cidr_ranges_match_correctly() {
    let zone = Zone::default().txt("tnhc.dev", "v=spf1 ip4:192.0.2.0/24 -all");
    assert_eq!(evaluate(&zone, ip("192.0.2.77"), "tnhc.dev", "h").await, SpfResult::Pass);
    assert_eq!(evaluate(&zone, ip("192.0.3.1"), "tnhc.dev", "h").await, SpfResult::Fail);
}

#[tokio::test]
async fn a_v6_sender_is_not_matched_by_a_v4_range() {
    // A dual-stack sender must not inherit authorisation from the other family.
    let zone = Zone::default().txt("tnhc.dev", "v=spf1 ip4:0.0.0.0/0 -all");
    assert_eq!(evaluate(&zone, ip("2001:db8::1"), "tnhc.dev", "h").await, SpfResult::Fail);
}

#[tokio::test]
async fn include_matches_only_on_a_pass() {
    // The part people misread: a failing include falls through to the next
    // mechanism rather than failing the whole evaluation.
    let zone = Zone::default()
        .txt("tnhc.dev", "v=spf1 include:provider.test ip4:203.0.113.5 -all")
        .txt("provider.test", "v=spf1 ip4:192.0.2.1 -all");

    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "tnhc.dev", "h").await, SpfResult::Pass);
    // Not in the include, but matched by the mechanism after it.
    assert_eq!(evaluate(&zone, ip("203.0.113.5"), "tnhc.dev", "h").await, SpfResult::Pass);
    assert_eq!(evaluate(&zone, ip("198.51.100.1"), "tnhc.dev", "h").await, SpfResult::Fail);
}

#[tokio::test]
async fn the_ten_lookup_budget_is_enforced() {
    // A security control: SPF is a recursive language over records other people
    // control, so without a hard cap one evaluation can be made to fan out into
    // thousands of queries — an amplifier pointed at us and at whoever is named.
    let mut zone = Zone::default();
    let mut record = String::from("v=spf1");
    for i in 0..30 {
        record.push_str(&format!(" include:hop{i}.test"));
        zone = zone.txt(&format!("hop{i}.test"), "v=spf1 -all");
    }
    zone = zone.txt("greedy.test", &format!("{record} -all"));

    let result = evaluate(&zone, ip("192.0.2.1"), "greedy.test", "h").await;
    assert_eq!(result, SpfResult::PermError);

    // And it actually stopped early rather than running all thirty.
    let used = zone.queries.load(std::sync::atomic::Ordering::SeqCst);
    assert!(used <= 14, "should stop near the budget, used {used} lookups");
}

#[tokio::test]
async fn an_include_loop_terminates() {
    // Two domains including each other must not recurse forever.
    let zone = Zone::default()
        .txt("a.test", "v=spf1 include:b.test -all")
        .txt("b.test", "v=spf1 include:a.test -all");
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "a.test", "h").await, SpfResult::PermError);
}

#[tokio::test]
async fn two_spf_records_are_an_error_not_a_guess() {
    // RFC 7208 §4.5. Picking a favourite would let a domain get a different
    // answer from different receivers.
    let zone = Zone::default()
        .txt("ambiguous.test", "v=spf1 ip4:192.0.2.1 -all")
        .txt("ambiguous.test", "v=spf1 -all");
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "ambiguous.test", "h").await, SpfResult::PermError);
}

#[tokio::test]
async fn a_dns_failure_is_temporary_never_a_rejection() {
    // A resolver problem is our fault, not the sender's. Rejecting on it would
    // bounce good mail whenever our DNS wobbles.
    struct Broken;
    impl Lookup for Broken {
        async fn txt(&self, _: &str) -> Result<Vec<String>, DnsError> {
            Err(DnsError::Failed("resolver down".into()))
        }
        async fn a(&self, _: &str) -> Result<Vec<IpAddr>, DnsError> {
            Err(DnsError::Failed("resolver down".into()))
        }
        async fn mx(&self, _: &str) -> Result<Vec<String>, DnsError> {
            Err(DnsError::Failed("resolver down".into()))
        }
    }
    assert_eq!(evaluate(&Broken, ip("192.0.2.1"), "any.test", "h").await, SpfResult::TempError);
}

#[tokio::test]
async fn the_a_and_mx_mechanisms_work() {
    let zone = Zone::default()
        .txt("tnhc.dev", "v=spf1 a mx -all")
        .a("tnhc.dev", "192.0.2.10")
        .mx("tnhc.dev", "mail.tnhc.dev")
        .a("mail.tnhc.dev", "192.0.2.20");

    assert_eq!(evaluate(&zone, ip("192.0.2.10"), "tnhc.dev", "h").await, SpfResult::Pass);
    assert_eq!(evaluate(&zone, ip("192.0.2.20"), "tnhc.dev", "h").await, SpfResult::Pass);
    assert_eq!(evaluate(&zone, ip("192.0.2.99"), "tnhc.dev", "h").await, SpfResult::Fail);
}

#[tokio::test]
async fn redirect_hands_over_the_whole_answer() {
    let zone = Zone::default()
        .txt("old.test", "v=spf1 redirect=new.test")
        .txt("new.test", "v=spf1 ip4:192.0.2.1 -all");
    assert_eq!(evaluate(&zone, ip("192.0.2.1"), "old.test", "h").await, SpfResult::Pass);
    assert_eq!(evaluate(&zone, ip("198.51.100.1"), "old.test", "h").await, SpfResult::Fail);
}
