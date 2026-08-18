//! Routing decisions. No database needed: this is pure policy.

use nexus_maildelivery::{Route, Router};
use nexus_mailstore::Address;

fn r() -> Router {
    Router::new()
        .with_local_domain("tnhc.dev")
        .with_peer_domain("peer.example")
}

fn addr(s: &str) -> Address {
    Address::parse(s).unwrap()
}

#[test]
fn our_own_domain_is_delivered_locally() {
    assert_eq!(r().route(&addr("info@tnhc.dev")), Route::Local);
}

#[test]
fn a_peer_domain_goes_over_the_node_channel_not_smtp() {
    // The whole point of the design: two Nexus nodes that already trust each
    // other have no reason to speak a protocol built for strangers.
    assert_eq!(
        r().route(&addr("bob@peer.example")),
        Route::Federated { node: "peer.example".into() }
    );
}

#[test]
fn everyone_else_is_external() {
    assert_eq!(r().route(&addr("someone@gmail.com")), Route::External);
}

#[test]
fn domain_matching_ignores_case() {
    // Addresses are normalised on parse, so this also guards that path.
    assert_eq!(r().route(&addr("Info@TNHC.DEV")), Route::Local);
}

#[test]
fn a_domain_listed_as_both_local_and_peer_stays_local() {
    // A misconfiguration, but the safe reading is to deliver here. Handing it
    // to a peer who may hand it back is how mail loops start.
    let router = Router::new()
        .with_local_domain("both.example")
        .with_peer_domain("both.example");
    assert_eq!(router.route(&addr("x@both.example")), Route::Local);
}
