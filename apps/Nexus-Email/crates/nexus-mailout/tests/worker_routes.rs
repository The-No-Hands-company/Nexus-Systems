//! Which transport each queued delivery takes. Needs Postgres.
//!
//! These drive `DeliveryWorker::process` on the test's own row rather than
//! `tick`, which claims whatever is due in the shared test database —
//! including rows that belong to tests running in parallel.

use std::future::Future;
use std::pin::Pin;
use std::sync::{Arc, Mutex};

use nexus_maildelivery::{FederatedHandoff, FederatedTransport, HandoffOutcome, Queue, QueuedDelivery, Route};
use nexus_mailauth::{public_key_record, verify_with_key, Canon, DkimSigner};
use nexus_mailout::{DeliveryWorker, Egress, WorkerConfig};
use nexus_mailstore::{Address, MailStore, Transport};
use sqlx::postgres::PgPoolOptions;
use uuid::Uuid;

/// Records every handoff and answers with a fixed outcome.
struct FakeTransport {
    outcome: HandoffOutcome,
    calls: Mutex<Vec<(String, String, Vec<u8>)>>,
}

impl FakeTransport {
    fn answering(outcome: HandoffOutcome) -> Arc<Self> {
        Arc::new(Self { outcome, calls: Mutex::new(Vec::new()) })
    }
    fn calls(&self) -> Vec<(String, String, Vec<u8>)> {
        self.calls.lock().unwrap().clone()
    }
}

impl FederatedTransport for FakeTransport {
    fn send<'a>(&'a self, h: FederatedHandoff<'a>) -> Pin<Box<dyn Future<Output = HandoffOutcome> + Send + 'a>> {
        self.calls.lock().unwrap().push((h.node.into(), h.recipient.into(), h.raw.to_vec()));
        let outcome = self.outcome.clone();
        Box::pin(async move { outcome })
    }
}

async fn setup() -> (MailStore, Queue) {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    (MailStore::new(pool.clone()), Queue::new(pool))
}

/// Store a message, queue it for `rcpt` over `route`, and return the row.
async fn queued(store: &MailStore, queue: &Queue, rcpt: &str, route: Route) -> (QueuedDelivery, Vec<u8>) {
    queued_from(store, queue, "alice@origin.test", rcpt, route).await
}

async fn queued_from(
    store: &MailStore,
    queue: &Queue,
    from: &str,
    rcpt: &str,
    route: Route,
) -> (QueuedDelivery, Vec<u8>) {
    let from = Address::parse(from).unwrap();
    let thread = store.thread_for(None, &[], Some("w")).await.unwrap();
    let raw = format!(
        "From: {}\r\nTo: {rcpt}\r\nSubject: w\r\nMessage-ID: <{}@t>\r\n\r\nbody\r\n",
        from.as_string(),
        Uuid::now_v7().simple()
    )
    .into_bytes();
    let id = store
        .store_message(&raw, thread, &from, Some("w"), None, None, &[], Transport::Internal, None)
        .await
        .unwrap();
    queue.enqueue(id, &from.as_string(), rcpt, &route).await.unwrap();
    let row: (Uuid, Uuid, String, String, String, String, i32) = sqlx::query_as(
        "SELECT id, message_id, envelope_from, recipient, destination, route, attempts
           FROM outbound_queue WHERE recipient = $1",
    )
    .bind(rcpt)
    .fetch_one(store.pool())
    .await
    .unwrap();
    let item = QueuedDelivery {
        id: row.0,
        message_id: row.1,
        envelope_from: row.2,
        recipient: row.3,
        destination: row.4,
        route: row.5,
        attempts: row.6,
    };
    (item, raw)
}

async fn state_of(store: &MailStore, rcpt: &str) -> (String, i32) {
    sqlx::query_as("SELECT state, attempts FROM outbound_queue WHERE recipient = $1")
        .bind(rcpt)
        .fetch_one(store.pool())
        .await
        .unwrap()
}

fn unique(domain: &str) -> String {
    format!("u-{}@{domain}", Uuid::now_v7().simple())
}

#[tokio::test]
async fn a_federated_delivery_goes_to_its_peer_over_the_transport() {
    let (store, queue) = setup().await;
    let rcpt = unique("peer.test");
    let (item, raw) = queued(&store, &queue, &rcpt, Route::Federated { node: "peer.test".into() }).await;
    let fake = FakeTransport::answering(HandoffOutcome::Delivered);
    let worker = DeliveryWorker::new(store.clone(), queue, WorkerConfig::default())
        .with_transport(fake.clone());

    worker.process(item).await;

    assert_eq!(fake.calls(), vec![("peer.test".to_string(), rcpt.clone(), raw)]);
    assert_eq!(state_of(&store, &rcpt).await.0, "delivered");
}

#[tokio::test]
async fn under_peer_egress_outside_mail_is_handed_to_the_egress_peer() {
    let (store, queue) = setup().await;
    let rcpt = unique("gmail.example");
    let (item, _) = queued(&store, &queue, &rcpt, Route::External).await;
    let fake = FakeTransport::answering(HandoffOutcome::Delivered);
    let config = WorkerConfig { egress: Egress::Peer("egress.test".into()), ..WorkerConfig::default() };
    let worker = DeliveryWorker::new(store.clone(), queue, config).with_transport(fake.clone());

    worker.process(item).await;

    let calls = fake.calls();
    assert_eq!(calls.len(), 1);
    assert_eq!((calls[0].0.as_str(), calls[0].1.as_str()), ("egress.test", rcpt.as_str()));
    assert_eq!(state_of(&store, &rcpt).await.0, "delivered");
}

#[tokio::test]
async fn a_deferred_handoff_stays_queued_for_a_retry() {
    let (store, queue) = setup().await;
    let rcpt = unique("peer.test");
    let (item, _) = queued(&store, &queue, &rcpt, Route::Federated { node: "peer.test".into() }).await;
    let fake = FakeTransport::answering(HandoffOutcome::Deferred("peer down".into()));
    let worker = DeliveryWorker::new(store.clone(), queue, WorkerConfig::default())
        .with_transport(fake);

    worker.process(item).await;

    assert_eq!(state_of(&store, &rcpt).await, ("pending".to_string(), 1));
}

#[tokio::test]
async fn a_rejected_handoff_is_a_permanent_failure() {
    let (store, queue) = setup().await;
    let rcpt = unique("peer.test");
    let (item, _) = queued(&store, &queue, &rcpt, Route::Federated { node: "peer.test".into() }).await;
    let fake = FakeTransport::answering(HandoffOutcome::Rejected("no such user".into()));
    let worker = DeliveryWorker::new(store.clone(), queue, WorkerConfig::default())
        .with_transport(fake);

    worker.process(item).await;

    assert_eq!(state_of(&store, &rcpt).await.0, "failed");
}

#[tokio::test]
async fn federated_mail_with_no_transport_configured_is_deferred_not_dropped() {
    let (store, queue) = setup().await;
    let rcpt = unique("peer.test");
    let (item, _) = queued(&store, &queue, &rcpt, Route::Federated { node: "peer.test".into() }).await;
    let worker = DeliveryWorker::new(store.clone(), queue, WorkerConfig::default());

    worker.process(item).await;

    assert_eq!(state_of(&store, &rcpt).await, ("pending".to_string(), 1));
}

fn dkim_key() -> &'static rsa::RsaPrivateKey {
    static KEY: std::sync::OnceLock<rsa::RsaPrivateKey> = std::sync::OnceLock::new();
    KEY.get_or_init(|| nexus_mailauth::generate(2048).unwrap())
}

fn origin_signer() -> Arc<DkimSigner> {
    Arc::new(DkimSigner {
        domain: "origin.test".into(),
        selector: "nexus".into(),
        key: dkim_key().clone(),
        header_canon: Canon::Relaxed,
        body_canon: Canon::Relaxed,
    })
}

#[tokio::test]
async fn mail_from_the_signing_domain_leaves_dkim_signed_and_the_stored_copy_is_untouched() {
    let (store, queue) = setup().await;
    let rcpt = unique("peer.test");
    let (item, raw) = queued(&store, &queue, &rcpt, Route::Federated { node: "peer.test".into() }).await;
    let message_id = item.message_id;
    let fake = FakeTransport::answering(HandoffOutcome::Delivered);
    let worker = DeliveryWorker::new(store.clone(), queue, WorkerConfig::default())
        .with_transport(fake.clone())
        .with_dkim(origin_signer());

    worker.process(item).await;

    let sent = &fake.calls()[0].2;
    assert!(sent.starts_with(b"DKIM-Signature:"), "{}", String::from_utf8_lossy(sent));
    let record = public_key_record(dkim_key()).unwrap();
    let sig = verify_with_key(sent, &record).expect("the peer must receive a verifiable signature");
    assert_eq!(sig.domain, "origin.test");

    let (stored,): (Option<Vec<u8>>,) = sqlx::query_as("SELECT body_inline FROM messages WHERE id = $1")
        .bind(message_id)
        .fetch_one(store.pool())
        .await
        .unwrap();
    assert_eq!(stored.unwrap(), raw, "signing must not rewrite the stored message");
}

#[tokio::test]
async fn mail_from_another_domain_is_not_signed_with_our_key() {
    let (store, queue) = setup().await;
    let rcpt = unique("peer.test");
    let (item, raw) =
        queued_from(&store, &queue, "bob@someone-else.test", &rcpt, Route::Federated { node: "peer.test".into() })
            .await;
    let fake = FakeTransport::answering(HandoffOutcome::Delivered);
    let worker = DeliveryWorker::new(store.clone(), queue, WorkerConfig::default())
        .with_transport(fake.clone())
        .with_dkim(origin_signer());

    worker.process(item).await;

    assert_eq!(fake.calls()[0].2, raw);
}
