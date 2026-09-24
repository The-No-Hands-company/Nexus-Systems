//! The sending side, against a real ingest listener on a loopback port.
//! Needs Postgres with the federation migration applied.

use std::sync::Arc;
use std::time::{SystemTime, UNIX_EPOCH};

use nexus_maildelivery::{Deliverer, FederatedHandoff, FederatedTransport, HandoffOutcome, Queue, Router};
use nexus_mailfed::client::HttpTransport;
use nexus_mailfed::ingest::{self, IngestState};
use nexus_mailfed::{NodeKey, Peer, PeerDirectory};
use nexus_mailmsg::MessageBuilder;
use nexus_mailstore::{Address, MailStore};
use sqlx::postgres::PgPoolOptions;
use sqlx::PgPool;
use uuid::Uuid;

fn now() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_secs() as i64
}

async fn pool() -> PgPool {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap()
}

struct Pair {
    sender: HttpTransport,
    sender_domain: String,
    receiver_domain: String,
    store: MailStore,
}

/// A receiving node listening on loopback, and a sender that has pinned it.
/// `receiver_trusts_sender`: whether the receiver pinned the sender's real key.
async fn pair(receiver_trusts_sender: bool) -> Pair {
    let pool = pool().await;
    let store = MailStore::new(pool.clone());
    let tag = Uuid::now_v7().simple().to_string();
    let sender_domain = format!("s{tag}.test");
    let receiver_domain = format!("r{tag}.test");
    let sender_key = Arc::new(NodeKey::generate());
    let receiver_key = Arc::new(NodeKey::generate());
    let peers = PeerDirectory::new(pool.clone());

    let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();

    // The receiver pins the sender (or, in the refusal case, some other key).
    peers
        .add(&Peer {
            domain: sender_domain.clone(),
            base_url: format!("https://mail.{sender_domain}"),
            public_key: if receiver_trusts_sender {
                sender_key.public_b64()
            } else {
                NodeKey::generate().public_b64()
            },
            may_relay: false,
        })
        .await
        .unwrap();
    // The sender pins the receiver at its loopback address.
    peers
        .add(&Peer {
            domain: receiver_domain.clone(),
            base_url: format!("http://{addr}"),
            public_key: receiver_key.public_b64(),
            may_relay: false,
        })
        .await
        .unwrap();

    let state = Arc::new(IngestState {
        local_domain: receiver_domain.clone(),
        public_host: addr.to_string(),
        key: receiver_key,
        peers: peers.clone(),
        deliverer: Arc::new(Deliverer::new(
            store.clone(),
            Queue::new(pool),
            Router::new().with_local_domain(&receiver_domain),
        )),
        clock: Arc::new(now),
    });
    tokio::spawn(async move { axum::serve(listener, ingest::router(state)).await.unwrap() });

    let sender = HttpTransport::new(sender_domain.clone(), sender_key, peers, Arc::new(now));
    Pair { sender, sender_domain, receiver_domain, store }
}

fn message(from: &str, to: &str) -> Vec<u8> {
    MessageBuilder::new(from, &format!("<{}@test>", Uuid::now_v7().simple()))
        .to(to)
        .subject("hi")
        .text("over the node channel")
        .build()
}

#[tokio::test]
async fn a_handoff_to_a_local_user_of_the_peer_is_delivered() {
    let p = pair(true).await;
    let mb = p.store.create_node_mailbox("Bob").await.unwrap();
    let bob = format!("bob@{}", p.receiver_domain);
    p.store.add_address(mb.id, &Address::parse(&bob).unwrap(), true).await.unwrap();
    let from = format!("alice@{}", p.sender_domain);
    let raw = message(&from, &bob);

    let outcome = p
        .sender
        .send(FederatedHandoff { node: &p.receiver_domain, envelope_from: &from, recipient: &bob, raw: &raw })
        .await;

    assert_eq!(outcome, HandoffOutcome::Delivered);
    assert_eq!(p.store.unseen_count(mb.id).await.unwrap(), 1);
}

#[tokio::test]
async fn a_recipient_the_peer_refuses_is_a_permanent_rejection() {
    let p = pair(true).await;
    let ghost = format!("ghost@{}", p.receiver_domain);
    let from = format!("alice@{}", p.sender_domain);
    let raw = message(&from, &ghost);

    let outcome = p
        .sender
        .send(FederatedHandoff { node: &p.receiver_domain, envelope_from: &from, recipient: &ghost, raw: &raw })
        .await;

    assert!(matches!(outcome, HandoffOutcome::Rejected(_)), "{outcome:?}");
}

#[tokio::test]
async fn a_peer_that_does_not_recognise_us_defers_rather_than_bounces() {
    // A key mismatch is a configuration problem an operator can fix; bouncing
    // would throw the mail away over it.
    let p = pair(false).await;
    let bob = format!("bob@{}", p.receiver_domain);
    let from = format!("alice@{}", p.sender_domain);
    let raw = message(&from, &bob);

    let outcome = p
        .sender
        .send(FederatedHandoff { node: &p.receiver_domain, envelope_from: &from, recipient: &bob, raw: &raw })
        .await;

    assert!(matches!(outcome, HandoffOutcome::Deferred(_)), "{outcome:?}");
}

#[tokio::test]
async fn an_unreachable_or_unpinned_peer_defers() {
    let p = pair(true).await;
    let peers = PeerDirectory::new(pool().await);
    let dead = format!("dead-{}.test", Uuid::now_v7().simple());
    peers
        .add(&Peer {
            domain: dead.clone(),
            base_url: "http://127.0.0.1:1".into(),
            public_key: NodeKey::generate().public_b64(),
            may_relay: false,
        })
        .await
        .unwrap();
    let from = format!("alice@{}", p.sender_domain);
    let raw = message(&from, "x@y.test");

    for node in [dead.as_str(), "never-pinned.test"] {
        let rcpt = format!("x@{node}");
        let outcome = p
            .sender
            .send(FederatedHandoff { node, envelope_from: &from, recipient: &rcpt, raw: &raw })
            .await;
        assert!(matches!(outcome, HandoffOutcome::Deferred(_)), "{node}: {outcome:?}");
    }
}
