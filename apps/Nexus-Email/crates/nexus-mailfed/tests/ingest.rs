//! The receiving side of a handoff: every refusal the spec promises, and the
//! two things a genuine request may do — land in a local inbox, or be queued
//! for relay. Needs Postgres with the federation migration applied.

use std::sync::Arc;

use axum::body::Body;
use axum::http::{Request, StatusCode};
use http_body_util::BodyExt;
use nexus_maildelivery::{Deliverer, Queue, Router as MailRouter};
use nexus_mailfed::ingest::{self, IngestState};
use nexus_mailfed::wire::{self, Outgoing};
use nexus_mailfed::{NodeKey, Peer, PeerDirectory};
use nexus_mailmsg::MessageBuilder;
use nexus_mailstore::{Address, MailStore};
use serde_json::Value;
use sqlx::postgres::PgPoolOptions;
use tower::ServiceExt;
use uuid::Uuid;

const NOW: i64 = 1_790_000_000;

struct Harness {
    app: axum::Router,
    store: MailStore,
    /// This node's domain.
    local: String,
    /// The public host this node's federation listener is reached at.
    host: String,
    /// A pinned peer and its key.
    peer: String,
    peer_key: NodeKey,
}

async fn harness(may_relay: bool) -> Harness {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    let store = MailStore::new(pool.clone());
    let tag = Uuid::now_v7().simple().to_string();
    let local = format!("l{tag}.test");
    let peer = format!("p{tag}.test");
    let other_peer = format!("o{tag}.test");

    let router = MailRouter::new().with_local_domain(&local).with_peer_domain(&other_peer);
    let deliverer = Arc::new(Deliverer::new(store.clone(), Queue::new(pool.clone()), router));
    let peers = PeerDirectory::new(pool);
    let peer_key = NodeKey::generate();
    peers
        .add(&Peer {
            domain: peer.clone(),
            base_url: format!("https://mail.{peer}"),
            public_key: peer_key.public_b64(),
            may_relay,
        })
        .await
        .unwrap();

    let host = format!("mail.{local}");
    let state = Arc::new(IngestState {
        local_domain: local.clone(),
        public_host: host.clone(),
        key: Arc::new(NodeKey::generate()),
        peers,
        deliverer,
        clock: Arc::new(|| NOW),
    });
    Harness { app: ingest::router(state), store, local, host, peer, peer_key }
}

fn message(from: &str, to: &str) -> Vec<u8> {
    MessageBuilder::new(from, &format!("<{}@test>", Uuid::now_v7().simple()))
        .to(to)
        .subject("Across nodes")
        .text("hello")
        .build()
}

/// A handoff from `node`, signed by `key`, built the way the real client does.
fn handoff(h: &Harness, key: &NodeKey, node: &str, from: &str, rcpts: &[String], at: i64) -> Request<Body> {
    let body = message(from, &rcpts[0]);
    let out = Outgoing {
        node: node.into(),
        host: h.host.clone(),
        envelope_from: from.into(),
        recipients: rcpts.to_vec(),
        timestamp: at,
        nonce: format!("n-{}", Uuid::now_v7().simple()),
    };
    let headers = out.sign(key, &body);
    let mut req = Request::post(wire::MAIL_PATH);
    for (name, value) in headers {
        req = req.header(name, value);
    }
    req.body(Body::from(body)).unwrap()
}

async fn send(h: &Harness, req: Request<Body>) -> (StatusCode, Value) {
    let res = h.app.clone().oneshot(req).await.unwrap();
    let status = res.status();
    let bytes = res.into_body().collect().await.unwrap().to_bytes();
    (status, serde_json::from_slice(&bytes).unwrap_or(Value::Null))
}

async fn local_mailbox(h: &Harness, user: &str) -> (Uuid, String) {
    let mb = h.store.create_node_mailbox(user).await.unwrap();
    let addr = format!("{user}@{}", h.local);
    h.store.add_address(mb.id, &Address::parse(&addr).unwrap(), true).await.unwrap();
    (mb.id, addr)
}

async fn queued(h: &Harness, rcpt: &str) -> Option<String> {
    sqlx::query_as::<_, (String,)>("SELECT route FROM outbound_queue WHERE recipient = $1")
        .bind(rcpt)
        .fetch_optional(h.store.pool())
        .await
        .unwrap()
        .map(|r| r.0)
}

fn accepted(body: &Value, rcpt: &str) -> bool {
    body["results"]
        .as_array()
        .unwrap()
        .iter()
        .find(|r| r["recipient"] == rcpt)
        .unwrap_or_else(|| panic!("no result for {rcpt} in {body}"))["accepted"]
        .as_bool()
        .unwrap()
}

#[tokio::test]
async fn a_peers_mail_for_a_local_user_lands_in_their_inbox() {
    let h = harness(false).await;
    let (mb, bob) = local_mailbox(&h, "bob").await;
    let from = format!("alice@{}", h.peer);
    let (status, body) = send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[bob.clone()], NOW)).await;

    assert_eq!(status, StatusCode::OK, "{body}");
    assert!(accepted(&body, &bob));
    assert_eq!(h.store.unseen_count(mb).await.unwrap(), 1);
}

#[tokio::test]
async fn a_relay_peers_outside_mail_is_queued_for_smtp() {
    let h = harness(true).await;
    let rcpt = format!("someone-{}@outside.example", Uuid::now_v7().simple());
    let from = format!("info@{}", h.peer);
    let (status, body) = send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[rcpt.clone()], NOW)).await;

    assert_eq!(status, StatusCode::OK, "{body}");
    assert!(accepted(&body, &rcpt));
    assert_eq!(queued(&h, &rcpt).await.as_deref(), Some("smtp"));
}

#[tokio::test]
async fn a_peer_without_the_relay_grant_cannot_send_outside() {
    // The open-relay test for the federation: pinned is not the same as
    // trusted to spend this node's reputation.
    let h = harness(false).await;
    let rcpt = format!("someone-{}@outside.example", Uuid::now_v7().simple());
    let from = format!("info@{}", h.peer);
    let (status, body) = send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[rcpt.clone()], NOW)).await;

    assert_eq!(status, StatusCode::OK, "{body}");
    assert!(!accepted(&body, &rcpt));
    assert_eq!(queued(&h, &rcpt).await, None);
}

#[tokio::test]
async fn mail_is_not_forwarded_from_one_peer_to_another() {
    let h = harness(true).await;
    let rcpt = format!("x-{}@{}", Uuid::now_v7().simple(), h.peer.replacen('p', "o", 1));
    let from = format!("info@{}", h.peer);
    let (_, body) = send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[rcpt.clone()], NOW)).await;
    assert!(!accepted(&body, &rcpt));
    assert_eq!(queued(&h, &rcpt).await, None);
}

#[tokio::test]
async fn an_unknown_local_user_is_refused_per_recipient() {
    let h = harness(false).await;
    let (_, bob) = local_mailbox(&h, "bob").await;
    let ghost = format!("ghost@{}", h.local);
    let from = format!("alice@{}", h.peer);
    let (status, body) =
        send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[bob.clone(), ghost.clone()], NOW)).await;

    assert_eq!(status, StatusCode::OK);
    assert!(accepted(&body, &bob));
    assert!(!accepted(&body, &ghost));
}

#[tokio::test]
async fn an_unpinned_node_is_refused() {
    let h = harness(true).await;
    let stranger = NodeKey::generate();
    let (_, bob) = local_mailbox(&h, "bob").await;
    let (status, _) =
        send(&h, handoff(&h, &stranger, "stranger.test", "x@stranger.test", &[bob], NOW)).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
}

#[tokio::test]
async fn a_request_signed_by_the_wrong_key_is_refused() {
    let h = harness(true).await;
    let (mb, bob) = local_mailbox(&h, "bob").await;
    let from = format!("alice@{}", h.peer);
    let (status, _) = send(&h, handoff(&h, &NodeKey::generate(), &h.peer, &from, &[bob], NOW)).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
    assert_eq!(h.store.unseen_count(mb).await.unwrap(), 0);
}

#[tokio::test]
async fn a_tampered_body_is_refused() {
    let h = harness(true).await;
    let (mb, bob) = local_mailbox(&h, "bob").await;
    let from = format!("alice@{}", h.peer);
    let req = handoff(&h, &h.peer_key, &h.peer, &from, &[bob], NOW);
    let (parts, _) = req.into_parts();
    let forged = Request::from_parts(parts, Body::from(message(&from, "evil@x.test")));
    let (status, _) = send(&h, forged).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
    assert_eq!(h.store.unseen_count(mb).await.unwrap(), 0);
}

#[tokio::test]
async fn a_request_signed_for_another_host_is_refused() {
    // Stops a request captured on its way to one node being replayed at another
    // node that pins the same sender.
    let mut h = harness(true).await;
    let (_, bob) = local_mailbox(&h, "bob").await;
    let from = format!("alice@{}", h.peer);
    let real_host = std::mem::replace(&mut h.host, "mail.somewhere-else.test".into());
    let req = handoff(&h, &h.peer_key, &h.peer, &from, &[bob], NOW);
    h.host = real_host;
    let (status, _) = send(&h, req).await;
    assert_eq!(status, StatusCode::UNAUTHORIZED);
}

#[tokio::test]
async fn a_stale_or_future_timestamp_is_refused() {
    let h = harness(true).await;
    let (_, bob) = local_mailbox(&h, "bob").await;
    let from = format!("alice@{}", h.peer);
    for at in [NOW - 301, NOW + 301] {
        let (status, _) = send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[bob.clone()], at)).await;
        assert_eq!(status, StatusCode::UNAUTHORIZED, "timestamp {at}");
    }
    let (status, _) = send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[bob], NOW - 299)).await;
    assert_eq!(status, StatusCode::OK, "inside the window is fine");
}

#[tokio::test]
async fn a_replayed_request_is_refused() {
    let h = harness(true).await;
    let (mb, bob) = local_mailbox(&h, "bob").await;
    let from = format!("alice@{}", h.peer);
    let first = handoff(&h, &h.peer_key, &h.peer, &from, &[bob], NOW);
    let (parts, body) = first.into_parts();
    let bytes = body.collect().await.unwrap().to_bytes();
    let replay = Request::from_parts(parts.clone(), Body::from(bytes.clone()));
    let original = Request::from_parts(parts, Body::from(bytes));

    assert_eq!(send(&h, original).await.0, StatusCode::OK);
    assert_eq!(send(&h, replay).await.0, StatusCode::UNAUTHORIZED);
    assert_eq!(h.store.unseen_count(mb).await.unwrap(), 1, "delivered exactly once");
}

#[tokio::test]
async fn a_peer_cannot_send_as_another_domain() {
    let h = harness(true).await;
    let (mb, bob) = local_mailbox(&h, "bob").await;
    let rcpt = format!("someone-{}@outside.example", Uuid::now_v7().simple());
    for from in ["ceo@bank.example".to_string(), format!("admin@{}", h.local)] {
        let (status, _) =
            send(&h, handoff(&h, &h.peer_key, &h.peer, &from, &[bob.clone(), rcpt.clone()], NOW)).await;
        assert_eq!(status, StatusCode::FORBIDDEN, "envelope-from {from}");
    }
    assert_eq!(h.store.unseen_count(mb).await.unwrap(), 0);
    assert_eq!(queued(&h, &rcpt).await, None);
}

#[tokio::test]
async fn a_request_missing_its_federation_headers_is_malformed() {
    let h = harness(true).await;
    let req = Request::post(wire::MAIL_PATH).body(Body::from("x")).unwrap();
    assert_eq!(send(&h, req).await.0, StatusCode::BAD_REQUEST);
}

#[tokio::test]
async fn the_public_key_is_published() {
    let h = harness(true).await;
    let req = Request::get(wire::KEY_PATH).body(Body::empty()).unwrap();
    let (status, body) = send(&h, req).await;
    assert_eq!(status, StatusCode::OK);
    assert_eq!(body["domain"], h.local.as_str());
    assert_eq!(body["public_key"].as_str().unwrap().len(), 44);
}
