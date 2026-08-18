//! DMARC. Most of these are about alignment, because alignment is the part
//! that carries the security: an unaligned pass proves only that *somebody*
//! authenticated, not that the domain in From did.

use std::collections::HashMap;
use std::net::IpAddr;

use nexus_mailauth::dmarc::{evaluate, parse_record, Alignment, Policy};
use nexus_mailauth::dns::{DnsError, Lookup};
use nexus_mailauth::{aligned, organizational_domain, SpfResult};

#[derive(Default)]
struct Zone {
    txt: HashMap<String, Vec<String>>,
}

impl Zone {
    fn txt(mut self, name: &str, value: &str) -> Self {
        self.txt.entry(name.into()).or_default().push(value.into());
        self
    }
}

impl Lookup for Zone {
    async fn txt(&self, name: &str) -> Result<Vec<String>, DnsError> {
        self.txt.get(name).cloned().ok_or(DnsError::NotFound)
    }
    async fn a(&self, _: &str) -> Result<Vec<IpAddr>, DnsError> {
        Err(DnsError::NotFound)
    }
    async fn mx(&self, _: &str) -> Result<Vec<String>, DnsError> {
        Err(DnsError::NotFound)
    }
}

fn zone(policy: &str) -> Zone {
    Zone::default().txt("_dmarc.tnhc.dev", policy)
}

// ── Alignment ───────────────────────────────────────────────────────────────

#[test]
fn relaxed_alignment_accepts_a_subdomain_but_strict_does_not() {
    // Mail is commonly sent from a subdomain, which is why relaxed is the
    // default — and why strict has to actually be strict.
    assert!(aligned("mail.tnhc.dev", "tnhc.dev", Alignment::Relaxed));
    assert!(!aligned("mail.tnhc.dev", "tnhc.dev", Alignment::Strict));
    assert!(aligned("tnhc.dev", "tnhc.dev", Alignment::Strict));
}

#[test]
fn an_unrelated_domain_never_aligns() {
    // The whole point. Without this an attacker signs with their own domain
    // and claims a DMARC pass for yours.
    assert!(!aligned("evil.test", "tnhc.dev", Alignment::Relaxed));
    assert!(!aligned("tnhc.dev.evil.test", "tnhc.dev", Alignment::Relaxed));
}

#[test]
fn two_domains_under_the_same_public_suffix_do_not_align() {
    // Naive "last two labels" would call a.co.uk and b.co.uk the same
    // organization, which under DMARC means accepting forged mail as aligned.
    assert_eq!(organizational_domain("mail.example.co.uk"), "example.co.uk");
    assert!(!aligned("attacker.co.uk", "example.co.uk", Alignment::Relaxed));
    assert!(aligned("mail.example.co.uk", "example.co.uk", Alignment::Relaxed));
}

// ── Record parsing ──────────────────────────────────────────────────────────

#[test]
fn a_record_parses_with_sensible_defaults() {
    let r = parse_record("v=DMARC1; p=reject").unwrap();
    assert_eq!(r.policy, Policy::Reject);
    // Relaxed on both, per the RFC, because most senders need it.
    assert_eq!(r.dkim_alignment, Alignment::Relaxed);
    assert_eq!(r.spf_alignment, Alignment::Relaxed);
    assert_eq!(r.percent, 100);
}

#[test]
fn strict_alignment_tags_are_honoured() {
    let r = parse_record("v=DMARC1; p=quarantine; adkim=s; aspf=s; pct=50").unwrap();
    assert_eq!(r.policy, Policy::Quarantine);
    assert_eq!(r.dkim_alignment, Alignment::Strict);
    assert_eq!(r.spf_alignment, Alignment::Strict);
    assert_eq!(r.percent, 50);
}

#[test]
fn something_that_is_not_a_dmarc_record_is_rejected() {
    assert!(parse_record("v=spf1 -all").is_none());
    assert!(parse_record("just some text").is_none());
}

// ── Evaluation ──────────────────────────────────────────────────────────────

#[tokio::test]
async fn either_dkim_or_spf_is_enough_when_aligned() {
    // Either, not both — that is the design, and it is what lets mail survive
    // forwarding, which breaks SPF while leaving DKIM intact.
    let z = zone("v=DMARC1; p=reject");

    let dkim_only = evaluate(&z, "tnhc.dev", Some("tnhc.dev"), true, None, SpfResult::Fail).await;
    assert!(dkim_only.pass);

    let spf_only =
        evaluate(&z, "tnhc.dev", None, false, Some("tnhc.dev"), SpfResult::Pass).await;
    assert!(spf_only.pass);
}

#[tokio::test]
async fn an_unaligned_dkim_pass_does_not_pass_dmarc() {
    // The attack this prevents: sign with a domain you control, put someone
    // else's address in From, and claim their reputation.
    let z = zone("v=DMARC1; p=reject");
    let r = evaluate(&z, "tnhc.dev", Some("evil.test"), true, None, SpfResult::None).await;

    assert!(!r.pass);
    assert!(!r.dkim_aligned);
    assert_eq!(r.policy, Policy::Reject);
}

#[tokio::test]
async fn an_unaligned_spf_pass_does_not_pass_dmarc() {
    // The same attack through the other mechanism: an attacker's own domain
    // can trivially publish an SPF record authorising their own server.
    let z = zone("v=DMARC1; p=reject");
    let r = evaluate(&z, "tnhc.dev", None, false, Some("evil.test"), SpfResult::Pass).await;
    assert!(!r.pass);
    assert!(!r.spf_aligned);
}

#[tokio::test]
async fn a_failing_dkim_signature_does_not_count_even_when_aligned() {
    let z = zone("v=DMARC1; p=reject");
    let r = evaluate(&z, "tnhc.dev", Some("tnhc.dev"), false, None, SpfResult::None).await;
    assert!(!r.pass);
}

#[tokio::test]
async fn a_subdomain_sender_aligns_under_the_default_policy() {
    let z = zone("v=DMARC1; p=reject");
    let r = evaluate(&z, "tnhc.dev", Some("mail.tnhc.dev"), true, None, SpfResult::None).await;
    assert!(r.pass, "relaxed alignment should accept a subdomain signature");
}

#[tokio::test]
async fn strict_alignment_rejects_the_subdomain_that_relaxed_accepts() {
    let z = zone("v=DMARC1; p=reject; adkim=s");
    let r = evaluate(&z, "tnhc.dev", Some("mail.tnhc.dev"), true, None, SpfResult::None).await;
    assert!(!r.pass);
}

#[tokio::test]
async fn no_dmarc_record_is_reported_as_none_rather_than_a_failure() {
    // Most domains publish no DMARC. Treating that as a failure would reject
    // most of the internet.
    let z = Zone::default();
    let r = evaluate(&z, "nothing.test", Some("nothing.test"), true, None, SpfResult::Pass).await;
    assert!(r.none);
    assert_eq!(r.policy, Policy::None);
}

#[tokio::test]
async fn a_published_p_none_is_reported_as_none_not_treated_as_reject() {
    // p=none means "we are watching". Acting on it as reject breaks real mail
    // from domains still rolling DMARC out, which is most of them.
    let z = zone("v=DMARC1; p=none");
    let r = evaluate(&z, "tnhc.dev", Some("evil.test"), true, None, SpfResult::Fail).await;
    assert!(!r.pass);
    assert_eq!(r.policy, Policy::None);
    assert!(!r.none, "a published p=none is not the same as no record at all");
}
