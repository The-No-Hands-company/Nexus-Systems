//! The peer directory and the replay guard. Needs a real Postgres with the
//! federation migration applied; panics rather than skips without one.

use std::time::Duration;

use nexus_mailfed::peers::{Peer, PeerDirectory};
use nexus_mailfed::NodeKey;
use sqlx::postgres::PgPoolOptions;
use uuid::Uuid;

async fn directory() -> PeerDirectory {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    PeerDirectory::new(pool)
}

/// A domain no other test uses, so parallel tests never see each other's rows.
fn unique_domain() -> String {
    format!("peer-{}.test", Uuid::now_v7().simple())
}

fn peer(domain: &str, may_relay: bool) -> Peer {
    Peer {
        domain: domain.into(),
        base_url: format!("https://mail.{domain}"),
        public_key: NodeKey::generate().public_b64(),
        may_relay,
    }
}

#[tokio::test]
async fn a_pinned_peer_can_be_read_back() {
    let dir = directory().await;
    let domain = unique_domain();
    let p = peer(&domain, true);
    dir.add(&p).await.unwrap();

    assert_eq!(dir.get(&domain).await.unwrap(), Some(p));
    assert!(dir.list().await.unwrap().iter().any(|x| x.domain == domain));

    dir.remove(&domain).await.unwrap();
    assert_eq!(dir.get(&domain).await.unwrap(), None);
}

#[tokio::test]
async fn domains_are_matched_case_insensitively() {
    let dir = directory().await;
    let domain = unique_domain();
    dir.add(&peer(&domain.to_uppercase(), false)).await.unwrap();
    assert!(dir.get(&domain).await.unwrap().is_some());
    assert!(dir.get(&domain.to_uppercase()).await.unwrap().is_some());
    dir.remove(&domain).await.unwrap();
}

#[tokio::test]
async fn re_pinning_replaces_the_key_and_the_relay_grant() {
    let dir = directory().await;
    let domain = unique_domain();
    dir.add(&peer(&domain, true)).await.unwrap();
    let rotated = peer(&domain, false);
    dir.add(&rotated).await.unwrap();

    assert_eq!(dir.get(&domain).await.unwrap(), Some(rotated));
    dir.remove(&domain).await.unwrap();
}

#[tokio::test]
async fn a_malformed_key_or_url_is_refused_by_the_database() {
    let dir = directory().await;
    let domain = unique_domain();
    let mut bad_key = peer(&domain, false);
    bad_key.public_key = "short".into();
    assert!(dir.add(&bad_key).await.is_err());

    let mut bad_url = peer(&domain, false);
    bad_url.base_url = "mail.example".into();
    assert!(dir.add(&bad_url).await.is_err());
}

#[tokio::test]
async fn a_nonce_is_accepted_once() {
    let dir = directory().await;
    let node = unique_domain();
    assert!(dir.record_nonce(&node, "n1").await.unwrap(), "first sighting must be accepted");
    assert!(!dir.record_nonce(&node, "n1").await.unwrap(), "replay must be refused");
    // The same nonce from a different node is a different request.
    assert!(dir.record_nonce(&unique_domain(), "n1").await.unwrap());
}

#[tokio::test]
async fn pruning_forgets_only_nonces_older_than_the_window() {
    let dir = directory().await;
    let node = unique_domain();
    dir.record_nonce(&node, "old").await.unwrap();
    sqlx::query("UPDATE federation_nonces SET seen_at = now() - interval '1 hour' WHERE node = $1 AND nonce = 'old'")
        .bind(&node)
        .execute(dir.pool())
        .await
        .unwrap();
    dir.record_nonce(&node, "fresh").await.unwrap();

    dir.prune_nonces(Duration::from_secs(600)).await.unwrap();

    assert!(dir.record_nonce(&node, "old").await.unwrap(), "pruned nonce is forgotten");
    assert!(!dir.record_nonce(&node, "fresh").await.unwrap(), "recent nonce is still remembered");
}
