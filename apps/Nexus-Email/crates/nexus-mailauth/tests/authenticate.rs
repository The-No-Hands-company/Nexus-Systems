//! The combined verdict: what a receiving server should actually do.

use std::collections::HashMap;
use std::net::IpAddr;

use nexus_mailauth::dns::{DnsError, Lookup};
use nexus_mailauth::{authenticate, Verdict};

#[derive(Default)]
struct Zone {
    txt: HashMap<String, Vec<String>>,
    a: HashMap<String, Vec<IpAddr>>,
}

impl Zone {
    fn txt(mut self, name: &str, v: &str) -> Self {
        self.txt.entry(name.into()).or_default().push(v.into());
        self
    }
}

impl Lookup for Zone {
    async fn txt(&self, name: &str) -> Result<Vec<String>, DnsError> {
        self.txt.get(name).cloned().ok_or(DnsError::NotFound)
    }
    async fn a(&self, name: &str) -> Result<Vec<IpAddr>, DnsError> {
        self.a.get(name).cloned().ok_or(DnsError::NotFound)
    }
    async fn mx(&self, _: &str) -> Result<Vec<String>, DnsError> {
        Err(DnsError::NotFound)
    }
}

fn message(from_header: &str) -> Vec<u8> {
    format!(
        "From: {from_header}\r\nTo: info@tnhc.dev\r\nSubject: test\r\n\r\nbody\r\n"
    )
    .into_bytes()
}

fn ip(s: &str) -> IpAddr {
    s.parse().unwrap()
}

#[tokio::test]
async fn an_unauthenticated_message_from_a_domain_with_no_policy_is_accepted() {
    // Most of the internet. Rejecting it would reject most legitimate mail.
    let zone = Zone::default();
    let auth = authenticate(&zone, ip("192.0.2.1"), "sender.test", "a@sender.test",
                            &message("a@sender.test")).await;
    assert_eq!(auth.verdict, Verdict::Accept);
    assert!(!auth.dmarc_published);
}

#[tokio::test]
async fn a_spoofed_sender_is_rejected_when_the_domain_says_reject() {
    // The attack DMARC exists to stop: someone else's server, our From header.
    let zone = Zone::default()
        .txt("bank.test", "v=spf1 ip4:203.0.113.1 -all")
        .txt("_dmarc.bank.test", "v=DMARC1; p=reject");

    let auth = authenticate(&zone, ip("198.51.100.66"), "evil.test", "attacker@evil.test",
                            &message("security@bank.test")).await;

    assert_eq!(auth.verdict, Verdict::Reject);
    assert!(!auth.dmarc_pass);
}

#[tokio::test]
async fn the_same_spoof_is_quarantined_when_the_domain_asks_for_that() {
    let zone = Zone::default()
        .txt("bank.test", "v=spf1 ip4:203.0.113.1 -all")
        .txt("_dmarc.bank.test", "v=DMARC1; p=quarantine");

    let auth = authenticate(&zone, ip("198.51.100.66"), "evil.test", "attacker@evil.test",
                            &message("security@bank.test")).await;
    assert_eq!(auth.verdict, Verdict::Quarantine);
}

#[tokio::test]
async fn a_domain_publishing_p_none_is_still_delivered() {
    // p=none means "we are watching". Acting on it would break mail from every
    // domain still rolling DMARC out.
    let zone = Zone::default()
        .txt("early.test", "v=spf1 ip4:203.0.113.1 -all")
        .txt("_dmarc.early.test", "v=DMARC1; p=none");

    let auth = authenticate(&zone, ip("198.51.100.66"), "evil.test", "x@evil.test",
                            &message("someone@early.test")).await;
    assert_eq!(auth.verdict, Verdict::Accept);
    assert!(!auth.dmarc_pass, "it still failed; it is simply not enforced");
}

#[tokio::test]
async fn an_aligned_spf_pass_satisfies_a_reject_policy() {
    // The legitimate case must survive, or enforcing DMARC just loses mail.
    let zone = Zone::default()
        .txt("good.test", "v=spf1 ip4:203.0.113.1 -all")
        .txt("_dmarc.good.test", "v=DMARC1; p=reject");

    let auth = authenticate(&zone, ip("203.0.113.1"), "good.test", "a@good.test",
                            &message("a@good.test")).await;
    assert_eq!(auth.verdict, Verdict::Accept);
    assert!(auth.dmarc_pass);
}

#[tokio::test]
async fn spf_is_evaluated_against_the_envelope_sender_not_the_from_header() {
    // A classic bug: forwarders rewrite the envelope and leave From alone, so
    // conflating the two makes every forwarded message fail.
    let zone = Zone::default()
        .txt("forwarder.test", "v=spf1 ip4:203.0.113.9 -all")
        .txt("author.test", "v=spf1 -all");

    let auth = authenticate(&zone, ip("203.0.113.9"), "forwarder.test",
                            "bounces@forwarder.test", &message("writer@author.test")).await;

    assert_eq!(auth.spf, nexus_mailauth::SpfResult::Pass);
    assert_eq!(auth.spf_domain.as_deref(), Some("forwarder.test"));
}

#[tokio::test]
async fn the_authentication_results_header_records_what_happened() {
    // Kept even on a pass: a later dispute about whether a message was
    // authentic is unanswerable if the evidence was discarded at delivery.
    let zone = Zone::default()
        .txt("good.test", "v=spf1 ip4:203.0.113.1 -all")
        .txt("_dmarc.good.test", "v=DMARC1; p=reject");

    let auth = authenticate(&zone, ip("203.0.113.1"), "good.test", "a@good.test",
                            &message("a@good.test")).await;
    let header = auth.header("mail.tnhc.dev");

    assert!(header.starts_with("Authentication-Results: mail.tnhc.dev;"));
    assert!(header.contains("spf=pass"));
    assert!(header.contains("dmarc=pass"));
    assert!(header.contains("p=reject"));
    assert!(header.ends_with("\r\n"));
}

#[tokio::test]
async fn a_message_with_no_from_header_cannot_pass_dmarc() {
    // There is no identity to align against, so it must not be treated as
    // authentic — but with no policy to consult it is still delivered.
    let zone = Zone::default();
    let auth = authenticate(&zone, ip("192.0.2.1"), "h.test", "a@h.test",
                            b"Subject: no from header\r\n\r\nbody\r\n").await;
    assert!(!auth.dmarc_pass);
    assert_eq!(auth.verdict, Verdict::Accept);
}
