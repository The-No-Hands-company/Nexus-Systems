use nexus_mailout::Egress;

#[test]
fn egress_settings_parse() {
    assert_eq!(Egress::parse(""), Ok(Egress::Direct));
    assert_eq!(Egress::parse("direct"), Ok(Egress::Direct));
    assert_eq!(Egress::parse("peer:Relay.Example"), Ok(Egress::Peer("relay.example".into())));
    assert_eq!(Egress::parse(" peer:relay.example "), Ok(Egress::Peer("relay.example".into())));
}

#[test]
fn a_mistyped_egress_is_an_error_not_a_silent_default() {
    // Falling back to direct on a typo would quietly queue everything behind a
    // filtered port 25 while the operator believes a peer is delivering it.
    for bad in ["peer:", "peer", "relay:x.example", "Direct!"] {
        assert!(Egress::parse(bad).is_err(), "{bad:?}");
    }
}
