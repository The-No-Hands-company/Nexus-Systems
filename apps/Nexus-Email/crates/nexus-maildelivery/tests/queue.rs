//! Queue semantics: claiming, backoff, and the point at which a message is
//! declared dead and the sender told.

use nexus_maildelivery::{backoff_for, max_attempts, Queue, Route};
use nexus_mailstore::{Address, MailStore, Transport};
use sqlx::postgres::PgPoolOptions;
use uuid::Uuid;

async fn harness() -> (MailStore, Queue) {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    (MailStore::new(pool.clone()), Queue::new(pool))
}

/// This test's own queue row.
///
/// Deliberately not claim_due(): these tests run in parallel against one
/// database, and claim_due takes whatever is pending — including rows belonging
/// to other tests, which then cannot find their own work. Claiming is exercised
/// by its own test, which filters by recipient.
async fn queue_row(store: &MailStore, recipient: &str) -> (Uuid, i32) {
    sqlx::query_as("SELECT id, attempts FROM outbound_queue WHERE recipient = $1")
        .bind(recipient)
        .fetch_one(store.pool())
        .await
        .unwrap()
}

async fn a_message(store: &MailStore) -> Uuid {
    let from = Address::parse("a@test.invalid").unwrap();
    let thread = store.thread_for(None, &[], Some("q")).await.unwrap();
    let raw = format!("Subject: q\r\n\r\n{}", Uuid::now_v7());
    store
        .store_message(raw.as_bytes(), thread, &from, Some("q"), None, None, &[], Transport::Internal, None)
        .await
        .unwrap()
}

#[test]
fn backoff_grows_and_then_gives_up() {
    // Fast early retries catch a peer that was restarting; long later ones
    // avoid hammering a node that is down for a day.
    let first = backoff_for(0).unwrap();
    let second = backoff_for(1).unwrap();
    assert!(second > first);
    assert!(backoff_for(max_attempts()).is_none(), "must eventually give up");

    // Roughly five days in total, which is the convention every mail system
    // settled on because it spans a weekend.
    let total: i64 = (0..max_attempts())
        .filter_map(backoff_for)
        .map(|d| d.num_minutes())
        .sum();
    assert!((4..=7).contains(&(total / 1440)), "total retry window was {} days", total / 1440);
}

#[tokio::test]
async fn queueing_the_same_recipient_twice_creates_one_delivery() {
    // A retried submission must not become two deliveries.
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let route = Route::Federated { node: "peer.example".into() };

    queue.enqueue(msg, "a@test.invalid", "bob@peer.example", &route).await.unwrap();
    queue.enqueue(msg, "a@test.invalid", "bob@peer.example", &route).await.unwrap();

    let rows: (i64,) = sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE message_id = $1")
        .bind(msg)
        .fetch_one(store.pool())
        .await
        .unwrap();
    assert_eq!(rows.0, 1);
}

#[tokio::test]
async fn local_recipients_are_never_queued() {
    // Local delivery is a database write that either happens or fails now;
    // there is no network to retry against.
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    queue.enqueue(msg, "a@test.invalid", "someone@local", &Route::Local).await.unwrap();

    let rows: (i64,) = sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE message_id = $1")
        .bind(msg)
        .fetch_one(store.pool())
        .await
        .unwrap();
    assert_eq!(rows.0, 0);
}

#[tokio::test]
async fn a_claimed_delivery_is_not_handed_to_a_second_worker() {
    // Two delivery workers must never send the same message twice.
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let rcpt = format!("bob-{}@peer.example", Uuid::now_v7().simple());
    queue
        .enqueue(msg, "a@test.invalid", &rcpt, &Route::Federated { node: "peer.example".into() })
        .await
        .unwrap();

    let first = queue.claim_due(100).await.unwrap();
    assert!(first.iter().any(|d| d.recipient == rcpt));

    let second = queue.claim_due(100).await.unwrap();
    assert!(
        !second.iter().any(|d| d.recipient == rcpt),
        "a claimed delivery must not be claimable again"
    );
}

#[tokio::test]
async fn a_temporary_failure_is_rescheduled_not_dropped() {
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let rcpt = format!("bob-{}@peer.example", Uuid::now_v7().simple());
    queue
        .enqueue(msg, "a@test.invalid", &rcpt, &Route::Federated { node: "peer.example".into() })
        .await
        .unwrap();

    let (id, _) = queue_row(&store, &rcpt).await;
    let dead = queue.mark_attempt_failed(id, false, "peer unreachable").await.unwrap();

    assert!(!dead, "a first temporary failure must not be terminal");
    let (state, attempts, err): (String, i32, Option<String>) = sqlx::query_as(
        "SELECT state, attempts, last_error FROM outbound_queue WHERE id = $1",
    )
    .bind(id)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!(state, "pending");
    assert_eq!(attempts, 1);
    assert_eq!(err.unwrap(), "peer unreachable");
}

#[tokio::test]
async fn a_permanent_failure_is_terminal_immediately() {
    // No such mailbox will not start existing on a retry; the sender needs to
    // know now rather than in five days.
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let rcpt = format!("ghost-{}@peer.example", Uuid::now_v7().simple());
    queue
        .enqueue(msg, "a@test.invalid", &rcpt, &Route::Federated { node: "peer.example".into() })
        .await
        .unwrap();

    let (id, _) = queue_row(&store, &rcpt).await;
    let dead = queue.mark_attempt_failed(id, true, "no such mailbox").await.unwrap();

    assert!(dead, "a permanent failure must be terminal at once");
    let (state, completed): (String, Option<chrono::DateTime<chrono::Utc>>) =
        sqlx::query_as("SELECT state, completed_at FROM outbound_queue WHERE id = $1")
            .bind(id)
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(state, "failed");
    assert!(completed.is_some(), "a terminal row must record when it finished");
}

#[tokio::test]
async fn retries_run_out_and_the_delivery_dies() {
    // The bounce boundary. Without this, mail retries forever and the sender
    // is never told it failed.
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let rcpt = format!("bob-{}@peer.example", Uuid::now_v7().simple());
    queue
        .enqueue(msg, "a@test.invalid", &rcpt, &Route::Federated { node: "peer.example".into() })
        .await
        .unwrap();

    let (id, _) = queue_row(&store, &rcpt).await;

    // Fail it as many times as the policy allows; the last one must be fatal.
    let mut dead = false;
    for _ in 0..max_attempts() {
        dead = queue.mark_attempt_failed(id, false, "still unreachable").await.unwrap();
    }
    assert!(dead, "after {} attempts the delivery must be dead", max_attempts());
}
