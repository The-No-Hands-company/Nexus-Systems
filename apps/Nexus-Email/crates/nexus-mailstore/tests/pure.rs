//! Tests that need no database: address normalisation and thread subject
//! grouping.

use nexus_mailstore::{normalise_subject, Address};

#[test]
fn addresses_are_lowercased_on_the_way_in() {
    // The schema refuses anything not already normalised, so this is the one
    // place normalisation may happen. If it stopped working, Info@ and info@
    // would become two different mailboxes.
    let a = Address::parse("  Info@TNHC.dev ").unwrap();
    assert_eq!(a.localpart, "info");
    assert_eq!(a.domain, "tnhc.dev");
    assert_eq!(a.as_string(), "info@tnhc.dev");
}

#[test]
fn the_domain_is_whatever_follows_the_last_at() {
    // Quoted localparts may legally contain an @.
    let a = Address::parse(r#""odd@name"@tnhc.dev"#).unwrap();
    assert_eq!(a.domain, "tnhc.dev");
    assert_eq!(a.localpart, r#""odd@name""#);
}

#[test]
fn malformed_addresses_are_rejected() {
    for raw in ["", "no-at-sign", "@tnhc.dev", "info@", "   "] {
        assert!(Address::parse(raw).is_err(), "should reject {raw:?}");
    }
}

#[test]
fn reply_and_forward_prefixes_collapse_to_one_thread_subject() {
    let plain = normalise_subject("Hello");
    for variant in ["Re: Hello", "RE: re: Hello", "Fwd: Re: Hello", "  FW:   Hello  "] {
        assert_eq!(normalise_subject(variant), plain, "{variant:?}");
    }
}

#[test]
fn normalisation_does_not_eat_the_subject_itself() {
    // "Regarding" starts with "re" but is not a reply prefix.
    assert_eq!(normalise_subject("Regarding the invoice"), "regarding the invoice");
    assert_eq!(normalise_subject("Re: Regarding"), "regarding");
}
