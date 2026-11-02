//! Queue semantics: claiming, backoff, and the point at which a message is
//! declared dead and the sender told.

use nexus_maildelivery::{backoff_for, max_attempts, Queue, Route};
use nexus_mailstore::{Address, FolderKind, MailStore, Transport};
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

async fn finish_submission(store: &MailStore, message: Uuid) {
    let mut tx = store.pool().begin().await.unwrap();
    sqlx::query("SELECT id FROM messages WHERE id = $1 FOR UPDATE")
        .bind(message)
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query("UPDATE message_references SET released_at = now() WHERE message_id = $1 AND kind = 'submission' AND released_at IS NULL")
        .bind(message).execute(&mut *tx).await.unwrap();
    tx.commit().await.unwrap();
}

#[tokio::test]
async fn erasing_final_mailbox_copy_keeps_pending_outbound_delivery() {
    let (store, queue) = harness().await;
    let mailbox = store.create_node_mailbox("reference safety").await.unwrap();
    let msg = a_message(&store).await;
    let inbox = store.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    store.deliver(mailbox.id, msg, inbox).await.unwrap();
    queue
        .enqueue(
            msg,
            "a@test.invalid",
            "recipient@peer.example",
            &Route::External,
        )
        .await
        .unwrap();
    finish_submission(&store, msg).await;

    store.erase_membership(mailbox.id, msg).await.unwrap();

    let (messages, pending): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM outbound_queue WHERE message_id = $1 AND state = 'pending')",
    )
    .bind(msg)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!(
        (messages, pending),
        (1, 1),
        "pending delivery must retain its message"
    );
}

#[tokio::test]
async fn erasure_waits_for_uncommitted_queue_reference() {
    let (store, queue) = harness().await;
    let mailbox = store.create_node_mailbox("queue race").await.unwrap();
    let msg = a_message(&store).await;
    let inbox = store.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    store.deliver(mailbox.id, msg, inbox).await.unwrap();
    finish_submission(&store, msg).await;
    let mut tx = store.begin_reference_transaction().await.unwrap();
    let (blocker,): (i32,) = sqlx::query_as("SELECT pg_backend_pid()")
        .fetch_one(&mut *tx)
        .await
        .unwrap();
    queue
        .enqueue_in_transaction(
            &mut tx,
            msg,
            "a@test.invalid",
            "race@peer.example",
            &Route::External,
        )
        .await
        .unwrap();
    let other = store.clone();
    let eraser = tokio::spawn(async move { other.erase_membership(mailbox.id, msg).await });
    tokio::time::timeout(std::time::Duration::from_secs(5), async {
        loop {
            assert!(!eraser.is_finished(), "erasure must wait for the queue transaction");
            let (blocked,): (bool,) = sqlx::query_as(
                "SELECT EXISTS (SELECT 1 FROM pg_stat_activity WHERE $1 = ANY(pg_blocking_pids(pid)))",
            ).bind(blocker).fetch_one(store.pool()).await.unwrap();
            if blocked { break; }
            tokio::task::yield_now().await;
        }
    }).await.expect("eraser never reached the message row lock");
    tx.commit().await.unwrap();
    eraser.await.unwrap().unwrap();
    let (messages, pending): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM outbound_queue WHERE message_id = $1 AND state = 'pending')",
    )
    .bind(msg)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!((messages, pending), (1, 1));
}

#[tokio::test]
async fn interrupted_reference_transaction_rolls_back_message_and_queue() {
    let (store, queue) = harness().await;
    let from = Address::parse("sender@example.test").unwrap();
    let thread = store.thread_for(None, &[], None).await.unwrap();
    let raw = format!("interrupted-{}", Uuid::now_v7());
    let submission = Uuid::now_v7();
    let mut tx = store.begin_reference_transaction().await.unwrap();
    let msg = store
        .store_message_in_transaction(
            &mut tx,
            raw.as_bytes(),
            thread,
            &from,
            None,
            None,
            None,
            &[],
            Transport::Internal,
            None,
            Some(submission),
        )
        .await
        .unwrap();
    queue
        .enqueue_in_transaction(
            &mut tx,
            msg,
            &from.as_string(),
            "outside@example.test",
            &Route::External,
        )
        .await
        .unwrap();
    tx.rollback().await.unwrap();
    let (messages, references, rows): (i64, i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM message_references WHERE message_id = $1),
                (SELECT count(*) FROM outbound_queue WHERE message_id = $1)",
    )
    .bind(msg)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!((messages, references, rows), (0, 0, 0));
}

#[tokio::test]
async fn inconsistent_active_queue_reference_aborts_erasure() {
    let (store, queue) = harness().await;
    let mailbox = store.create_node_mailbox("inconsistent").await.unwrap();
    let msg = a_message(&store).await;
    let inbox = store.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    store.deliver(mailbox.id, msg, inbox).await.unwrap();
    queue
        .enqueue(
            msg,
            "a@test.invalid",
            "missing-hold@peer.example",
            &Route::External,
        )
        .await
        .unwrap();
    finish_submission(&store, msg).await;
    sqlx::query("UPDATE message_references SET released_at = now() WHERE message_id = $1 AND kind = 'outbound'")
        .bind(msg).execute(store.pool()).await.unwrap();

    assert!(store.erase_membership(mailbox.id, msg).await.is_err());
    let (messages, memberships, queue_rows): (i64, i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM mailbox_messages WHERE message_id = $1),
                (SELECT count(*) FROM outbound_queue WHERE message_id = $1 AND state = 'pending')",
    )
    .bind(msg)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!((messages, memberships, queue_rows), (1, 1, 1));
}

#[tokio::test]
async fn duplicate_queueing_rejects_a_missing_outbound_hold() {
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let recipient = format!("duplicate-{}@outside.example", Uuid::now_v7());
    queue
        .enqueue(msg, "sender@example.test", &recipient, &Route::External)
        .await
        .unwrap();
    sqlx::query("UPDATE message_references SET released_at = now() WHERE message_id = $1 AND kind = 'outbound'")
        .bind(msg).execute(store.pool()).await.unwrap();
    assert!(queue
        .enqueue(msg, "sender@example.test", &recipient, &Route::External)
        .await
        .is_err());
}

#[tokio::test]
async fn outbound_holds_survive_retry_and_release_on_terminal_state() {
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let mailbox = store.create_node_mailbox("outbound cleanup").await.unwrap();
    let inbox = store.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    store.deliver(mailbox.id, msg, inbox).await.unwrap();
    let rcpt = format!("hold-{}@peer.example", Uuid::now_v7());
    queue
        .enqueue(msg, "a@test.invalid", &rcpt, &Route::External)
        .await
        .unwrap();
    queue
        .enqueue(msg, "a@test.invalid", &rcpt, &Route::External)
        .await
        .unwrap();
    finish_submission(&store, msg).await;
    store.erase_membership(mailbox.id, msg).await.unwrap();
    let (id, _) = queue_row(&store, &rcpt).await;
    assert!(!queue
        .mark_attempt_failed(id, false, "temporary")
        .await
        .unwrap());
    let (active,): (i64,) = sqlx::query_as(
        "SELECT count(*) FROM message_references WHERE message_id = $1 AND kind = 'outbound' AND released_at IS NULL",
    ).bind(msg).fetch_one(store.pool()).await.unwrap();
    assert_eq!(active, 1);
    queue.mark_delivered(id).await.unwrap();
    let (active,): (i64,) = sqlx::query_as(
        "SELECT count(*) FROM message_references WHERE message_id = $1 AND kind = 'outbound' AND released_at IS NULL",
    ).bind(msg).fetch_one(store.pool()).await.unwrap();
    assert_eq!(active, 0);
    let (messages, rows): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM outbound_queue WHERE message_id = $1)",
    )
    .bind(msg)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!(
        (messages, rows),
        (0, 0),
        "terminal delivery must clean up an orphan"
    );
}

#[tokio::test]
async fn terminal_failure_releases_outbound_hold() {
    let (store, queue) = harness().await;
    let msg = a_message(&store).await;
    let rcpt = format!("failed-hold-{}@peer.example", Uuid::now_v7());
    queue
        .enqueue(msg, "a@test.invalid", &rcpt, &Route::External)
        .await
        .unwrap();
    finish_submission(&store, msg).await;
    let (id, _) = queue_row(&store, &rcpt).await;
    assert!(queue
        .mark_attempt_failed(id, true, "permanent")
        .await
        .unwrap());
    let (active,): (i64,) = sqlx::query_as(
        "SELECT count(*) FROM message_references WHERE message_id = $1 AND kind = 'outbound' AND released_at IS NULL",
    ).bind(msg).fetch_one(store.pool()).await.unwrap();
    assert_eq!(active, 0);
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

/// Finished deliveries are forgotten: a delivered row (which names its
/// recipient) after an hour, a dead one after a week. Pending work is never
/// touched. Before this, every recipient address was kept forever.
#[tokio::test]
async fn test_purge_finished_when_rows_are_old_enough_should_forget_them_and_keep_the_rest() {
    let (store, queue) = harness().await;
    let mut rows = Vec::new();
    for label in ["delivered-old", "delivered-new", "failed-old", "failed-new", "pending"] {
        let msg = a_message(&store).await;
        let rcpt = format!("{label}-{}@peer.example", Uuid::now_v7().simple());
        queue.enqueue(msg, "a@test.invalid", &rcpt, &Route::Federated { node: "peer.example".into() }).await.unwrap();
        let (id, _) = queue_row(&store, &rcpt).await;
        rows.push((label, id, rcpt));
    }
    let id_of = |label: &str| rows.iter().find(|r| r.0 == label).unwrap().1;
    queue.mark_delivered(id_of("delivered-old")).await.unwrap();
    queue.mark_delivered(id_of("delivered-new")).await.unwrap();
    queue.mark_attempt_failed(id_of("failed-old"), true, "no such mailbox").await.unwrap();
    queue.mark_attempt_failed(id_of("failed-new"), true, "no such mailbox").await.unwrap();
    for (label, hours) in [("delivered-old", 2), ("failed-old", 24 * 8), ("failed-new", 24 * 3)] {
        sqlx::query("UPDATE outbound_queue SET completed_at = now() - make_interval(hours => $1) WHERE id = $2")
            .bind(hours).bind(id_of(label)).execute(store.pool()).await.unwrap();
    }

    queue.purge_finished(chrono::Duration::hours(1), chrono::Duration::days(7)).await.unwrap();

    for (label, _, rcpt) in &rows {
        let (n,): (i64,) = sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE recipient = $1")
            .bind(rcpt).fetch_one(store.pool()).await.unwrap();
        let kept = !matches!(*label, "delivered-old" | "failed-old");
        assert_eq!(n == 1, kept, "{label} should be {}", if kept { "kept" } else { "forgotten" });
    }
}
