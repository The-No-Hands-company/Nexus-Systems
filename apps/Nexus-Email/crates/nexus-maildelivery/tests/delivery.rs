//! End-to-end delivery against a real PostgreSQL.
//!
//! Run with NEXUS_EMAIL_TEST_DATABASE_URL set; see the crate README. These
//! panic rather than skip without it, because a silently skipped integration
//! test reads as a passing one.

use nexus_maildelivery::{Deliverer, Disposition, Queue, Router};
use nexus_mailmsg::MessageBuilder;
use nexus_mailstore::{Address, FolderKind, MailStore};
use sqlx::postgres::PgPoolOptions;
use uuid::Uuid;

async fn harness() -> (Deliverer, MailStore, Queue, String) {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    // Queue claims and retention affect all rows in the shared test database.
    sqlx::query("SELECT pg_advisory_lock(811239651)")
        .execute(&pool)
        .await
        .unwrap();
    let store = MailStore::new(pool.clone());
    let queue = Queue::new(pool);
    // A domain unique to this test run, so tests can share one database
    // without colliding on addresses.
    let local = format!("t{}.test", Uuid::now_v7().simple());
    let router = Router::new()
        .with_local_domain(&local)
        .with_peer_domain("peer.example");
    (Deliverer::new(store.clone(), queue.clone(), router), store, queue, local)
}

fn msg(from: &str, to: &str, subject: &str, body: &str) -> Vec<u8> {
    MessageBuilder::new(from, &format!("<{}@test>", Uuid::now_v7().simple()))
        .to(to)
        .subject(subject)
        .text(body)
        .build()
}

#[tokio::test]
async fn submission_commits_all_recipient_intents_or_none() {
    let (d, store, _, local) = harness().await;
    let mailbox = store.create_node_mailbox("intent recipient").await.unwrap();
    let sender = Address::parse("sender@example.test").unwrap();
    let local_address = Address::parse(&format!("local@{local}")).unwrap();
    let external = Address::parse("person@outside.example").unwrap();
    store
        .add_address(mailbox.id, &local_address, true)
        .await
        .unwrap();
    let raw = msg(&sender.as_string(), "mixed", "intents", "body");
    let outcomes = d
        .submit(&raw, &sender, &[local_address, external], None)
        .await
        .unwrap();
    assert_eq!(outcomes.len(), 2);

    let (has_intents,): (bool,) =
        sqlx::query_as("SELECT to_regclass('mail_submission_recipients') IS NOT NULL")
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert!(
        has_intents,
        "accepted recipients must have durable intent records"
    );
    let intents: Vec<(String, String)> = sqlx::query_as(
        "SELECT r.route, r.state FROM mail_submission_recipients r
         JOIN mail_submissions s ON s.id = r.submission_id
         JOIN messages m ON m.id = s.message_id
         WHERE m.content_hash = encode(sha256($1), 'hex') ORDER BY r.route",
    )
    .bind(&raw[..])
    .fetch_all(store.pool())
    .await
    .unwrap();
    assert_eq!(
        intents,
        vec![
            ("local".to_string(), "delivered".to_string()),
            ("smtp".to_string(), "outbound_pending".to_string()),
        ]
    );
}

#[tokio::test]
async fn queue_insert_failure_rolls_back_all_submission_work() {
    let (d, store, _, local) = harness().await;
    let mailbox = store
        .create_node_mailbox("rollback recipient")
        .await
        .unwrap();
    let sender = Address::parse("sender@example.test").unwrap();
    let local_address = Address::parse(&format!("local@{local}")).unwrap();
    let suffix = Uuid::now_v7().simple().to_string();
    let external = Address::parse(&format!("rollback-{suffix}@outside.example")).unwrap();
    store
        .add_address(mailbox.id, &local_address, true)
        .await
        .unwrap();
    let subject = format!("rollback-{suffix}");
    let raw = msg(&sender.as_string(), "mixed", &subject, "body");
    let function = format!("reject_queue_{suffix}");
    let trigger = format!("reject_queue_{suffix}");
    sqlx::raw_sql(&format!(
        "CREATE FUNCTION {function}() RETURNS trigger LANGUAGE plpgsql AS $$
         BEGIN RAISE EXCEPTION 'injected queue failure'; END $$;
         CREATE TRIGGER {trigger} BEFORE INSERT ON outbound_queue
         FOR EACH ROW WHEN (NEW.recipient = '{}') EXECUTE FUNCTION {function}();",
        external.as_string(),
    ))
    .execute(store.pool())
    .await
    .unwrap();
    let result = d
        .submit(&raw, &sender, &[local_address, external], None)
        .await;
    sqlx::raw_sql(&format!(
        "DROP TRIGGER {trigger} ON outbound_queue; DROP FUNCTION {function}();",
    ))
    .execute(store.pool())
    .await
    .unwrap();
    assert!(result.is_err());
    let (messages, memberships, threads): (i64, i64, i64) = sqlx::query_as(
        "SELECT
           (SELECT count(*) FROM messages WHERE content_hash = encode(sha256($1), 'hex')),
           (SELECT count(*) FROM mailbox_messages WHERE mailbox_id = $2),
           (SELECT count(*) FROM threads WHERE subject_normalised = $3)",
    )
    .bind(&raw[..])
    .bind(mailbox.id)
    .bind(&subject)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!((messages, memberships, threads), (0, 0, 0));
}

#[tokio::test]
async fn entirely_rejected_submission_without_sender_copy_does_not_leak_a_message() {
    let (d, store, _, local) = harness().await;
    let sender = Address::parse("sender@example.test").unwrap();
    let missing = Address::parse(&format!("missing@{local}")).unwrap();
    let raw = msg(
        &sender.as_string(),
        &missing.as_string(),
        "rejected",
        "body",
    );
    let outcomes = d.submit(&raw, &sender, &[missing], None).await.unwrap();
    assert!(matches!(outcomes[0].disposition, Disposition::Rejected(_)));
    let (exists,): (bool,) = sqlx::query_as(
        "SELECT EXISTS (SELECT 1 FROM messages WHERE content_hash = encode(sha256($1), 'hex'))",
    )
    .bind(&raw[..])
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert!(!exists);
}

#[tokio::test]
async fn rejected_only_submission_does_not_leave_its_subject_in_threads() {
    let (d, store, _, local) = harness().await;
    let sender = Address::parse("sender@example.test").unwrap();
    let missing = Address::parse(&format!("missing@{local}")).unwrap();
    let subject = format!("rejected-subject-{}", Uuid::now_v7());
    let raw = msg(&sender.as_string(), &missing.as_string(), &subject, "body");
    d.submit(&raw, &sender, &[missing], None).await.unwrap();
    let (threads,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM threads WHERE subject_normalised = $1")
            .bind(subject)
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(
        threads, 0,
        "an orphaned thread must not retain the rejected subject"
    );
}

#[tokio::test]
async fn deleting_one_message_preserves_a_thread_used_by_another() {
    let (d, store, _, local) = harness().await;
    let mailbox = store.create_node_mailbox("shared thread").await.unwrap();
    let sender = Address::parse(&format!("sender@{local}")).unwrap();
    let first_id = format!("<{}@test>", Uuid::now_v7());
    let first = MessageBuilder::new(&sender.as_string(), &first_id)
        .subject("shared thread")
        .text("first")
        .build();
    let reply = MessageBuilder::new(&sender.as_string(), &format!("<{}@test>", Uuid::now_v7()))
        .in_reply_to(&first_id)
        .subject("Re: shared thread")
        .text("second")
        .build();
    d.submit(&first, &sender, &[], Some(mailbox.id))
        .await
        .unwrap();
    d.submit(&reply, &sender, &[], Some(mailbox.id))
        .await
        .unwrap();
    let (first_message, thread): (Uuid, Uuid) = sqlx::query_as(
        "SELECT id, thread_id FROM messages WHERE content_hash = encode(sha256($1), 'hex')",
    )
    .bind(&first[..])
    .fetch_one(store.pool())
    .await
    .unwrap();
    let (reply_thread,): (Uuid,) = sqlx::query_as(
        "SELECT thread_id FROM messages WHERE content_hash = encode(sha256($1), 'hex')",
    )
    .bind(&reply[..])
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!(thread, reply_thread);
    store
        .erase_membership(mailbox.id, first_message)
        .await
        .unwrap();
    let (exists,): (bool,) = sqlx::query_as("SELECT EXISTS (SELECT 1 FROM threads WHERE id = $1)")
        .bind(thread)
        .fetch_one(store.pool())
        .await
        .unwrap();
    assert!(exists, "another live message still references the thread");
}

#[tokio::test]
async fn retry_after_terminal_queue_retention_does_not_send_again() {
    let (d, store, queue, local) = harness().await;
    let mailbox = store
        .create_node_mailbox("retry after retention")
        .await
        .unwrap();
    let sender = Address::parse(&format!("sender@{local}")).unwrap();
    for failed in [false, true] {
        let recipient =
            Address::parse(&format!("retry-{}@outside.example", Uuid::now_v7())).unwrap();
        let raw = msg(
            &sender.as_string(),
            &recipient.as_string(),
            "retry retention",
            "body",
        );
        d.submit(
            &raw,
            &sender,
            std::slice::from_ref(&recipient),
            Some(mailbox.id),
        )
        .await
        .unwrap();
        let (id,): (Uuid,) = sqlx::query_as("SELECT id FROM outbound_queue WHERE recipient = $1")
            .bind(recipient.as_string())
            .fetch_one(store.pool())
            .await
            .unwrap();
        let token = queue
            .claim_due(10_000)
            .await
            .unwrap()
            .into_iter()
            .find(|row| row.queue_id == id)
            .unwrap()
            .lease_token;
        if failed {
            queue
                .mark_attempt_failed(id, token, true, "permanent")
                .await
                .unwrap();
        } else {
            queue.mark_delivered(id, token).await.unwrap();
        }
        queue
            .purge_finished(chrono::Duration::zero(), chrono::Duration::zero())
            .await
            .unwrap();

        d.submit(
            &raw,
            &sender,
            std::slice::from_ref(&recipient),
            Some(mailbox.id),
        )
        .await
        .unwrap();
        let (queue_rows, recipient_rows): (i64, i64) = sqlx::query_as(
            "SELECT (SELECT count(*) FROM outbound_queue WHERE recipient = $1),
                    (SELECT count(*) FROM mail_submission_recipients WHERE recipient = $1)",
        )
        .bind(recipient.as_string())
        .fetch_one(store.pool())
        .await
        .unwrap();
        assert_eq!((queue_rows, recipient_rows), (0, 0));
    }
}

#[tokio::test]
async fn retry_counts_retained_terminal_identity_for_repeated_recipients() {
    let (d, store, queue, local) = harness().await;
    let mailbox = store
        .create_node_mailbox("duplicate retention")
        .await
        .unwrap();
    let sender = Address::parse(&format!("sender@{local}")).unwrap();
    let recipient =
        Address::parse(&format!("duplicate-{}@outside.example", Uuid::now_v7())).unwrap();
    let raw = msg(
        &sender.as_string(),
        &recipient.as_string(),
        "duplicate retention",
        "body",
    );
    let recipients = [recipient.clone(), recipient.clone()];
    d.submit(&raw, &sender, &recipients, Some(mailbox.id))
        .await
        .unwrap();
    let rows: Vec<(Uuid,)> =
        sqlx::query_as("SELECT id FROM outbound_queue WHERE recipient = $1 ORDER BY id")
            .bind(recipient.as_string())
            .fetch_all(store.pool())
            .await
            .unwrap();
    assert_eq!(rows.len(), 2);
    let token = queue
        .claim_due(10_000)
        .await
        .unwrap()
        .into_iter()
        .find(|row| row.queue_id == rows[0].0)
        .unwrap()
        .lease_token;
    queue.mark_delivered(rows[0].0, token).await.unwrap();
    queue
        .purge_finished(chrono::Duration::zero(), chrono::Duration::zero())
        .await
        .unwrap();
    d.submit(&raw, &sender, &recipients, Some(mailbox.id))
        .await
        .unwrap();
    let (rows,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE recipient = $1")
            .bind(recipient.as_string())
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(
        rows, 1,
        "the purged terminal recipient still counts toward retry identity"
    );
}

#[tokio::test]
async fn retry_after_retention_does_not_redeliver_when_route_changes() {
    let (d, store, queue, local) = harness().await;
    let mailbox = store.create_node_mailbox("changed route").await.unwrap();
    let sender = Address::parse(&format!("sender@{local}")).unwrap();
    let recipient = Address::parse(&format!("route-{}@outside.example", Uuid::now_v7())).unwrap();
    let raw = msg(
        &sender.as_string(),
        &recipient.as_string(),
        "route changed",
        "body",
    );
    d.submit(
        &raw,
        &sender,
        std::slice::from_ref(&recipient),
        Some(mailbox.id),
    )
    .await
    .unwrap();
    let (id,): (Uuid,) = sqlx::query_as("SELECT id FROM outbound_queue WHERE recipient = $1")
        .bind(recipient.as_string())
        .fetch_one(store.pool())
        .await
        .unwrap();
    let token = queue
        .claim_due(10_000)
        .await
        .unwrap()
        .into_iter()
        .find(|row| row.queue_id == id)
        .unwrap()
        .lease_token;
    queue.mark_delivered(id, token).await.unwrap();
    queue
        .purge_finished(chrono::Duration::zero(), chrono::Duration::zero())
        .await
        .unwrap();
    let changed = Deliverer::new(
        store.clone(),
        queue,
        Router::new()
            .with_local_domain(&local)
            .with_peer_domain("outside.example"),
    );
    changed
        .submit(
            &raw,
            &sender,
            std::slice::from_ref(&recipient),
            Some(mailbox.id),
        )
        .await
        .unwrap();
    let (rows,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE recipient = $1")
            .bind(recipient.as_string())
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(
        rows, 0,
        "a route update must not turn a retry into a second delivery"
    );
}

#[tokio::test]
async fn retrying_submission_does_not_duplicate_outbound_work() {
    let (d, store, _, _) = harness().await;
    let sender = Address::parse("sender@example.test").unwrap();
    let recipient = Address::parse(&format!("retry-{}@outside.example", Uuid::now_v7())).unwrap();
    let raw = msg(&sender.as_string(), &recipient.as_string(), "retry", "body");
    for _ in 0..2 {
        d.submit(&raw, &sender, std::slice::from_ref(&recipient), None)
            .await
            .unwrap();
    }
    let (count,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM outbound_queue WHERE recipient = $1")
            .bind(recipient.as_string())
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(count, 1, "retry must not create another delivery");
}

#[tokio::test]
async fn retrying_a_stored_message_does_not_leave_an_extra_thread() {
    let (d, store, _, local) = harness().await;
    let mailbox = store.create_node_mailbox("thread retry").await.unwrap();
    let sender = Address::parse(&format!("sender@{local}")).unwrap();
    let recipient = Address::parse(&format!("retry-{}@outside.example", Uuid::now_v7())).unwrap();
    let subject = format!("thread-retry-{}", Uuid::now_v7());
    let raw = msg(
        &sender.as_string(),
        &recipient.as_string(),
        &subject,
        "body",
    );
    for _ in 0..2 {
        d.submit(
            &raw,
            &sender,
            std::slice::from_ref(&recipient),
            Some(mailbox.id),
        )
        .await
        .unwrap();
    }
    let (threads,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM threads WHERE subject_normalised = $1")
            .bind(subject)
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(
        threads, 1,
        "deduplicated messages must not leave new orphan threads"
    );
}

#[tokio::test]
async fn repeated_envelope_recipients_each_have_a_durable_intent() {
    let (d, store, _, _) = harness().await;
    let sender = Address::parse("sender@example.test").unwrap();
    let recipient = Address::parse(&format!("twice-{}@outside.example", Uuid::now_v7())).unwrap();
    let raw = msg(&sender.as_string(), &recipient.as_string(), "twice", "body");
    for _ in 0..2 {
        d.submit(&raw, &sender, &[recipient.clone(), recipient.clone()], None)
            .await
            .unwrap();
    }
    let (intents, queue): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM mail_submission_recipients WHERE recipient = $1),
                (SELECT count(*) FROM outbound_queue WHERE recipient = $1)",
    )
    .bind(recipient.as_string())
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!((intents, queue), (2, 2));
}

#[tokio::test]
async fn unfinished_legacy_submission_cannot_be_silently_superseded() {
    let (d, store, _, _) = harness().await;
    let sender = Address::parse("sender@example.test").unwrap();
    let recipient = Address::parse("outside@example.test").unwrap();
    let raw = msg(
        &sender.as_string(),
        &recipient.as_string(),
        "legacy",
        "body",
    );
    let thread = store.thread_for(None, &[], Some("legacy")).await.unwrap();
    let message = store
        .store_message(
            &raw,
            thread,
            &sender,
            Some("legacy"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
        )
        .await
        .unwrap();
    assert!(d.submit(&raw, &sender, &[recipient], None).await.is_err());
    let (messages, submissions): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM mail_submissions WHERE message_id = $1)",
    )
    .bind(message)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!((messages, submissions), (1, 0));
}

#[tokio::test]
async fn empty_peer_relay_does_not_store_an_orphan() {
    let (d, store, _, _) = harness().await;
    let sender = Address::parse("sender@example.test").unwrap();
    let raw = msg(&sender.as_string(), "nobody", "empty relay", "body");
    assert!(d.relay_for_peer(&raw, &sender, &[]).await.is_err());
    let (exists,): (bool,) = sqlx::query_as(
        "SELECT EXISTS (SELECT 1 FROM messages WHERE content_hash = encode(sha256($1), 'hex'))",
    )
    .bind(&raw[..])
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert!(!exists);
}

#[tokio::test]
async fn reciprocal_submissions_lock_mailboxes_in_a_consistent_order() {
    let (d, store, _, local) = harness().await;
    let left = store.create_node_mailbox("left").await.unwrap();
    let right = store.create_node_mailbox("right").await.unwrap();
    let a = Address::parse(&format!("a@{local}")).unwrap();
    let b = Address::parse(&format!("b@{local}")).unwrap();
    store.add_address(left.id, &a, true).await.unwrap();
    store.add_address(right.id, &b, true).await.unwrap();
    let first = msg(&a.as_string(), &b.as_string(), "one", "a");
    let second = msg(&b.as_string(), &a.as_string(), "two", "b");
    let (one, two) = tokio::time::timeout(std::time::Duration::from_secs(10), async {
        tokio::join!(
            d.submit(&first, &a, std::slice::from_ref(&b), Some(left.id)),
            d.submit(&second, &b, std::slice::from_ref(&a), Some(right.id))
        )
    })
    .await
    .expect("reciprocal submissions deadlocked");
    one.unwrap();
    two.unwrap();
    for mailbox in [left.id, right.id] {
        let inbox = store.folder(mailbox, FolderKind::Inbox).await.unwrap();
        let (count,): (i64,) = sqlx::query_as(
            "SELECT count(*) FROM mailbox_messages WHERE mailbox_id = $1 AND folder_id = $2",
        )
        .bind(mailbox)
        .bind(inbox)
        .fetch_one(store.pool())
        .await
        .unwrap();
        assert_eq!(count, 1);
    }
}

#[tokio::test]
async fn mixed_submission_leaves_delivered_and_pending_intents() {
    let (d, store, _, local) = harness().await;
    let mailbox = store.create_node_mailbox("mixed").await.unwrap();
    let sender = Address::parse("sender@example.test").unwrap();
    let local_address = Address::parse(&format!("local@{local}")).unwrap();
    let external = Address::parse("someone@outside.example").unwrap();
    store
        .add_address(mailbox.id, &local_address, true)
        .await
        .unwrap();
    let raw = msg(&sender.as_string(), "mixed", "mixed", "body");
    let outcomes = d
        .submit(&raw, &sender, &[local_address, external], None)
        .await
        .unwrap();
    assert_eq!(outcomes[0].disposition, Disposition::DeliveredLocally);
    assert_eq!(outcomes[1].disposition, Disposition::Queued);

    let kinds: Vec<(String,)> = sqlx::query_as(
        "SELECT r.state FROM mail_submission_recipients r
         JOIN mail_submissions s ON s.id = r.submission_id
         JOIN messages m ON m.id = s.message_id
         WHERE m.content_hash = encode(sha256($1), 'hex') ORDER BY r.state",
    )
    .bind(&raw[..])
    .fetch_all(store.pool())
    .await
    .unwrap();
    assert_eq!(
        kinds,
        vec![("delivered".to_string(),), ("outbound_pending".to_string(),)]
    );
}

#[tokio::test]
async fn purging_finished_queue_forgets_outbound_disposition_recipient() {
    let (d, store, queue, local) = harness().await;
    let sender_mailbox = store.create_node_mailbox("sender").await.unwrap();
    let sender = Address::parse(&format!("sender@{local}")).unwrap();
    let recipient = Address::parse(&format!("purge-{}@outside.example", Uuid::now_v7())).unwrap();
    let raw = msg(
        &sender.as_string(),
        &recipient.as_string(),
        "privacy",
        "body",
    );
    d.submit(&raw, &sender, &[recipient.clone()], Some(sender_mailbox.id))
        .await
        .unwrap();
    let (id,): (Uuid,) = sqlx::query_as("SELECT id FROM outbound_queue WHERE recipient = $1")
        .bind(recipient.as_string())
        .fetch_one(store.pool())
        .await
        .unwrap();
    let token = queue
        .claim_due(10_000)
        .await
        .unwrap()
        .into_iter()
        .find(|row| row.queue_id == id)
        .unwrap()
        .lease_token;
    queue.mark_delivered(id, token).await.unwrap();
    sqlx::query(
        "UPDATE outbound_queue SET completed_at = now() - interval '2 hours' WHERE id = $1",
    )
    .bind(id)
    .execute(store.pool())
    .await
    .unwrap();
    queue
        .purge_finished(chrono::Duration::hours(1), chrono::Duration::days(7))
        .await
        .unwrap();
    let (remaining,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM mail_submission_recipients WHERE recipient = $1")
            .bind(recipient.as_string())
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(
        remaining, 0,
        "queue retention must also forget its durable recipient outcome"
    );
}

#[tokio::test]
async fn erasing_mailbox_copy_forgets_its_local_recipient_outcomes() {
    let (d, store, _, local) = harness().await;
    let sender_mailbox = store.create_node_mailbox("sender").await.unwrap();
    let receiver = store.create_node_mailbox("receiver").await.unwrap();
    let sender = Address::parse(&format!("sender@{local}")).unwrap();
    let first = Address::parse(&format!("first@{local}")).unwrap();
    let second = Address::parse(&format!("second@{local}")).unwrap();
    store.add_address(receiver.id, &first, true).await.unwrap();
    store
        .add_address(receiver.id, &second, false)
        .await
        .unwrap();
    let raw = msg(&sender.as_string(), "local", "privacy", "body");
    d.submit(&raw, &sender, &[first, second], Some(sender_mailbox.id))
        .await
        .unwrap();
    let (message,): (Uuid,) =
        sqlx::query_as("SELECT id FROM messages WHERE content_hash = encode(sha256($1), 'hex')")
            .bind(&raw[..])
            .fetch_one(store.pool())
            .await
            .unwrap();
    store.erase_membership(receiver.id, message).await.unwrap();
    let (messages, local_outcomes): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM mail_submission_recipients r
                 JOIN mail_submissions s ON s.id = r.submission_id
                 WHERE s.message_id = $1 AND r.route = 'local')",
    ).bind(message).fetch_one(store.pool()).await.unwrap();
    assert_eq!((messages, local_outcomes), (1, 0));
}

#[tokio::test]
async fn a_local_message_arrives_in_the_recipients_inbox() {
    let (d, store, _, local) = harness().await;

    let alice_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Alice").await.unwrap();
    let bob_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob").await.unwrap();
    let alice = Address::parse(&format!("alice@{local}")).unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    store.add_address(alice_mb.id, &alice, true).await.unwrap();
    store.add_address(bob_mb.id, &bob, true).await.unwrap();

    let raw = msg(&alice.as_string(), &bob.as_string(), "Hello Bob", "First message.");
    let outcomes = d.submit(&raw, &alice, &[bob.clone()], Some(alice_mb.id)).await.unwrap();

    assert_eq!(outcomes.len(), 1);
    assert_eq!(outcomes[0].disposition, Disposition::DeliveredLocally);
    assert_eq!(store.unseen_count(bob_mb.id).await.unwrap(), 1);
}

#[tokio::test]
async fn the_sender_keeps_a_copy_without_the_message_being_stored_twice() {
    let (d, store, _, local) = harness().await;

    let alice_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Alice").await.unwrap();
    let bob_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob").await.unwrap();
    let alice = Address::parse(&format!("alice@{local}")).unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    store.add_address(alice_mb.id, &alice, true).await.unwrap();
    store.add_address(bob_mb.id, &bob, true).await.unwrap();

    let raw = msg(&alice.as_string(), &bob.as_string(), "Copy check", "body");
    d.submit(&raw, &alice, &[bob], Some(alice_mb.id)).await.unwrap();

    let sent = store.folder(alice_mb.id, FolderKind::Sent).await.unwrap();
    let in_sent: (i64,) = sqlx::query_as(
        "SELECT count(*) FROM mailbox_messages WHERE mailbox_id = $1 AND folder_id = $2",
    )
    .bind(alice_mb.id)
    .bind(sent)
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!(in_sent.0, 1, "sender should have a Sent copy");

    // One stored message, two memberships. A sent message is not a second copy
    // of itself.
    let stored: (i64,) = sqlx::query_as(
        "SELECT count(*) FROM messages WHERE content_hash = encode(sha256($1), 'hex')",
    )
    .bind(&raw[..])
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!(stored.0, 1);
}

#[tokio::test]
async fn one_bad_address_does_not_stop_the_others() {
    // A message to several people where one address does not exist must still
    // reach everyone else, and say precisely which one failed.
    let (d, store, _, local) = harness().await;

    let alice_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Alice").await.unwrap();
    let bob_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob").await.unwrap();
    let alice = Address::parse(&format!("alice@{local}")).unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    let ghost = Address::parse(&format!("nobody@{local}")).unwrap();
    store.add_address(alice_mb.id, &alice, true).await.unwrap();
    store.add_address(bob_mb.id, &bob, true).await.unwrap();

    let raw = msg(&alice.as_string(), "many", "Fan out", "body");
    let outcomes = d.submit(&raw, &alice, &[bob, ghost], Some(alice_mb.id)).await.unwrap();

    assert_eq!(outcomes[0].disposition, Disposition::DeliveredLocally);
    match &outcomes[1].disposition {
        Disposition::Rejected(reason) => assert!(reason.contains("no such mailbox")),
        other => panic!("expected a rejection, got {other:?}"),
    }
    assert_eq!(store.unseen_count(bob_mb.id).await.unwrap(), 1);
    let dispositions: Vec<(String,)> = sqlx::query_as(
        "SELECT r.state FROM mail_submission_recipients r
         JOIN mail_submissions s ON s.id = r.submission_id
         JOIN messages m ON m.id = s.message_id
         WHERE m.content_hash = encode(sha256($1), 'hex') ORDER BY r.state",
    )
    .bind(&raw[..])
    .fetch_all(store.pool())
    .await
    .unwrap();
    assert_eq!(
        dispositions,
        vec![("delivered".to_string(),), ("failed".to_string(),)]
    );
}

#[tokio::test]
async fn a_federated_recipient_is_queued_not_delivered_locally() {
    let (d, store, _, local) = harness().await;

    let alice_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Alice").await.unwrap();
    let alice = Address::parse(&format!("alice@{local}")).unwrap();
    store.add_address(alice_mb.id, &alice, true).await.unwrap();

    // A recipient unique to this test: the queue is shared with every other
    // test running in parallel, so a global pending count is not this test's.
    let peer = Address::parse(&format!("bob-{}@peer.example", Uuid::now_v7().simple())).unwrap();
    let raw = msg(&alice.as_string(), &peer.as_string(), "Across nodes", "hi");
    let outcomes = d.submit(&raw, &alice, &[peer.clone()], Some(alice_mb.id)).await.unwrap();

    assert_eq!(outcomes[0].disposition, Disposition::Queued);
    let (route, state): (String, String) =
        sqlx::query_as("SELECT route, state FROM outbound_queue WHERE recipient = $1")
            .bind(peer.as_string())
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!((route.as_str(), state.as_str()), ("federated", "pending"));
}

#[tokio::test]
async fn a_message_from_a_peer_lands_in_the_local_inbox() {
    // The receiving half of federation.
    let (d, store, _, local) = harness().await;

    let bob_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob").await.unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    store.add_address(bob_mb.id, &bob, true).await.unwrap();

    let remote = Address::parse("carol@peer.example").unwrap();
    let raw = msg(&remote.as_string(), &bob.as_string(), "From another node", "hello");
    d.accept_federated(&raw, &remote, &bob).await.unwrap();

    assert_eq!(store.unseen_count(bob_mb.id).await.unwrap(), 1);

    // Provenance is recorded: this arrived over the node channel, not SMTP.
    let transport: (String,) = sqlx::query_as(
        "SELECT transport FROM messages WHERE content_hash = encode(sha256($1), 'hex')",
    )
    .bind(&raw[..])
    .fetch_one(store.pool())
    .await
    .unwrap();
    assert_eq!(transport.0, "federated");
}

#[tokio::test]
async fn we_refuse_to_relay_for_a_domain_we_do_not_serve() {
    // Accepting this would make the node an open relay for the federation.
    let (d, _, _, _) = harness().await;
    let from = Address::parse("carol@peer.example").unwrap();
    let elsewhere = Address::parse("victim@somewhere-else.test").unwrap();
    let raw = msg(&from.as_string(), &elsewhere.as_string(), "relay attempt", "x");

    assert!(d.accept_federated(&raw, &from, &elsewhere).await.is_err());
}

#[tokio::test]
async fn a_reply_lands_in_the_same_thread_as_the_message_it_answers() {
    let (d, store, _, local) = harness().await;

    let alice_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Alice").await.unwrap();
    let bob_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob").await.unwrap();
    let alice = Address::parse(&format!("alice@{local}")).unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    store.add_address(alice_mb.id, &alice, true).await.unwrap();
    store.add_address(bob_mb.id, &bob, true).await.unwrap();

    let parent_id = format!("<parent-{}@test>", Uuid::now_v7().simple());
    let original = MessageBuilder::new(alice.as_string(), &parent_id)
        .to(bob.as_string())
        .subject("Design review")
        .text("thoughts?")
        .build();
    d.submit(&original, &alice, &[bob.clone()], Some(alice_mb.id)).await.unwrap();

    let reply_id = format!("<reply-{}@test>", Uuid::now_v7().simple());
    let reply = MessageBuilder::new(bob.as_string(), &reply_id)
        .to(alice.as_string())
        .subject("Re: Design review")
        .in_reply_to(&parent_id)
        .text("agreed")
        .build();
    d.submit(&reply, &bob, &[alice], Some(bob_mb.id)).await.unwrap();

    // Both messages must exist AND share one thread. Counting distinct threads
    // alone would pass if the reply had never been stored at all.
    let (found, threads): (i64, i64) = sqlx::query_as(
        "SELECT count(*), count(DISTINCT thread_id) FROM messages
         WHERE rfc822_msg_id IN ($1, $2)",
    )
    .bind(&parent_id)
    .bind(&reply_id)
    .fetch_one(store.pool())
    .await
    .unwrap();

    assert_eq!(found, 2, "both the original and the reply must be stored");
    assert_eq!(threads, 1, "the reply must join its parent's thread");
}

#[tokio::test]
async fn a_delivered_message_becomes_searchable_by_its_body() {
    // Search has to reach the body, not just the subject — searching for a
    // phrase you remember from a message is the whole point.
    let (d, store, _, local) = harness().await;

    let alice_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Alice").await.unwrap();
    let bob_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob").await.unwrap();
    let alice = Address::parse(&format!("alice@{local}")).unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    store.add_address(alice_mb.id, &alice, true).await.unwrap();
    store.add_address(bob_mb.id, &bob, true).await.unwrap();

    let needle = format!("zarquon{}", Uuid::now_v7().simple());
    let raw = msg(&alice.as_string(), &bob.as_string(), "Nothing distinctive", &format!("the word is {needle} ok"));
    d.submit(&raw, &alice, &[bob], Some(alice_mb.id)).await.unwrap();

    let bob_inbox = store.folder(bob_mb.id, FolderKind::Inbox).await.unwrap();
    let hits = store
        .search(bob_mb.id, &needle, &[bob_inbox], None, 10)
        .await
        .unwrap();
    assert_eq!(hits.len(), 1, "the body text should be searchable");

    // And it must not leak across mailboxes: Alice holds only her Sent copy,
    // which is the same message row — so scoping search by membership matters.
    let alice_sent = store.folder(alice_mb.id, FolderKind::Sent).await.unwrap();
    let sender_hits = store
        .search(alice_mb.id, &needle, &[alice_sent], None, 10)
        .await
        .unwrap();
    assert_eq!(sender_hits.len(), 1, "the sender's own copy is hers to find");
}

#[tokio::test]
async fn search_does_not_return_another_mailboxs_mail() {
    // The same message row is shared between mailboxes. Searching `messages`
    // directly rather than through membership would leak across them.
    let (d, store, _, local) = harness().await;

    let alice_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Alice").await.unwrap();
    let bob_mb = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob").await.unwrap();
    let outsider = store.create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Outsider").await.unwrap();
    let alice = Address::parse(&format!("alice@{local}")).unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    store.add_address(alice_mb.id, &alice, true).await.unwrap();
    store.add_address(bob_mb.id, &bob, true).await.unwrap();

    let needle = format!("secret{}", Uuid::now_v7().simple());
    let raw = msg(&alice.as_string(), &bob.as_string(), "private", &format!("contains {needle}"));
    d.submit(&raw, &alice, &[bob], Some(alice_mb.id)).await.unwrap();

    let outsider_inbox = store.folder(outsider.id, FolderKind::Inbox).await.unwrap();
    assert!(
        store
            .search(outsider.id, &needle, &[outsider_inbox], None, 10)
            .await
            .unwrap()
            .is_empty(),
        "a third party must not find mail they do not hold"
    );
}

#[tokio::test]
async fn a_relayed_message_is_stored_once_and_queued_for_each_outside_recipient() {
    let (d, store, _, _) = harness().await;
    let tag = Uuid::now_v7().simple().to_string();
    let from = Address::parse("info@peer.example").unwrap();
    let r1 = Address::parse(&format!("one-{tag}@outside.example")).unwrap();
    let r2 = Address::parse(&format!("two-{tag}@elsewhere.example")).unwrap();
    let raw = msg(&from.as_string(), &r1.as_string(), "Relay me", "via a peer");

    let message_id = d.relay_for_peer(&raw, &from, &[r1.clone(), r2.clone()]).await.unwrap();

    let rows: Vec<(Uuid, String, String, String)> = sqlx::query_as(
        "SELECT message_id, recipient, destination, route FROM outbound_queue
          WHERE recipient IN ($1, $2) ORDER BY recipient",
    )
    .bind(r1.as_string())
    .bind(r2.as_string())
    .fetch_all(store.pool())
    .await
    .unwrap();
    assert_eq!(
        rows,
        vec![
            (message_id, r1.as_string(), "outside.example".into(), "smtp".into()),
            (message_id, r2.as_string(), "elsewhere.example".into(), "smtp".into()),
        ]
    );

    let (transport,): (String,) = sqlx::query_as("SELECT transport::text FROM messages WHERE id = $1")
        .bind(message_id)
        .fetch_one(store.pool())
        .await
        .unwrap();
    assert_eq!(transport, "federated", "provenance must record that a peer handed it over");
}

#[tokio::test]
async fn relaying_refuses_recipients_this_node_or_a_peer_serves() {
    // Those are delivered, not relayed; queueing them for SMTP would send our
    // own users' mail out to the internet and back.
    let (d, _, _, local) = harness().await;
    let from = Address::parse("info@peer.example").unwrap();
    for rcpt in [format!("someone@{local}"), "someone@peer.example".to_string()] {
        let rcpt = Address::parse(&rcpt).unwrap();
        let raw = msg(&from.as_string(), &rcpt.as_string(), "no", "no");
        assert!(d.relay_for_peer(&raw, &from, &[rcpt]).await.is_err());
    }
}

#[tokio::test]
async fn delivery_errors_display_without_the_recipient_but_keep_it_for_the_sender() {
    let (d, _, _, _) = harness().await;
    let foreign = Address::parse(&format!("someone{}@foreign.example", Uuid::now_v7().simple())).unwrap();
    let from = Address::parse("a@origin.test").unwrap();
    let err = d.accept_smtp(b"Subject: x\r\n\r\nb\r\n", &from, &foreign).await.unwrap_err();

    // Display is what every `{e}` in a log line prints.
    let shown = err.to_string();
    assert!(!shown.contains(&foreign.localpart), "{shown}");
    assert!(!shown.contains("foreign.example"), "{shown}");
    // The explicit method still names the recipient, for replying to the sender.
    assert!(err.detail_for_user().contains(&foreign.as_string()));
}
