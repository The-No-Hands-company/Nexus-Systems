//! Tests against a real PostgreSQL. There is no mock: the point of most of
//! these is that the *database* enforces something, which a mock cannot
//! demonstrate.
//!
//! Run with:
//!   NEXUS_EMAIL_TEST_DATABASE_URL=postgres://nexus:...@127.0.0.1:5432/nexus_email_test \
//!     cargo test -p nexus-mailstore
//!
//! Without that variable the tests panic rather than skip. A silently skipped
//! integration test reads as a passing one in CI output, which is how a suite
//! ends up green while proving nothing.

use chrono::Duration;
use nexus_mailstore::{
    Address, ExternalBodyReservation, FolderKind, MailStore, MailStoreError, Transport,
    INLINE_BODY_LIMIT,
};
use sqlx::postgres::PgPoolOptions;
use uuid::Uuid;

async fn store() -> MailStore {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL").expect(
        "NEXUS_EMAIL_TEST_DATABASE_URL must be set; these tests assert database behaviour \
         and cannot run without one",
    );
    let pool = PgPoolOptions::new()
        .max_connections(4)
        .connect(&url)
        .await
        .expect("connect to the test database");
    MailStore::new(pool)
}

/// Unique per test run so tests do not collide in a shared database.
fn unique(prefix: &str) -> String {
    format!("{prefix}-{}", Uuid::now_v7().simple())
}

async fn finish_submission(store: &MailStore, message: Uuid) {
    let mut tx = store.pool().begin().await.unwrap();
    sqlx::query("SELECT id FROM messages WHERE id = $1 FOR UPDATE")
        .bind(message)
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query(
        "UPDATE message_references SET released_at = now()
         WHERE message_id = $1 AND kind = 'submission' AND released_at IS NULL",
    )
    .bind(message)
    .execute(&mut *tx)
    .await
    .unwrap();
    tx.commit().await.unwrap();
}

async fn wait_for_block_or_finish<T>(
    store: &MailStore,
    blocker_pid: i32,
    task: &tokio::task::JoinHandle<T>,
) {
    tokio::time::timeout(std::time::Duration::from_secs(5), async {
        loop {
            let (blocked,): (bool,) = sqlx::query_as(
                "SELECT EXISTS (SELECT 1 FROM pg_stat_activity WHERE $1 = ANY(pg_blocking_pids(pid)))",
            ).bind(blocker_pid).fetch_one(store.pool()).await.unwrap();
            if blocked || task.is_finished() {
                break;
            }
            tokio::time::sleep(std::time::Duration::from_millis(10)).await;
        }
    }).await.expect("concurrent operation neither finished nor blocked");
}

#[tokio::test]
async fn concurrent_final_message_erasures_remove_the_shared_thread() {
    let s = store().await;
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s
        .thread_for(None, &[], Some(&unique("shared-thread")))
        .await
        .unwrap();
    let mut messages = Vec::new();
    for _ in 0..2 {
        let message = s
            .store_message(
                unique("thread-message").as_bytes(),
                thread,
                &from,
                None,
                None,
                None,
                &[],
                Transport::Internal,
                None,
            )
            .await
            .unwrap();
        finish_submission(&s, message).await;
        messages.push(message);
    }

    let mut first = s.pool().begin().await.unwrap();
    let mut second = s.pool().begin().await.unwrap();
    let (first_pid,): (i32,) = sqlx::query_as("SELECT pg_backend_pid()")
        .fetch_one(&mut *first)
        .await
        .unwrap();
    assert!(
        MailStore::erase_orphaned_message_in_transaction(&mut first, messages[0])
            .await
            .unwrap()
    );
    let second_message = messages[1];
    let task = tokio::spawn(async move {
        let result =
            MailStore::erase_orphaned_message_in_transaction(&mut second, second_message).await;
        (second, result)
    });
    wait_for_block_or_finish(&s, first_pid, &task).await;
    first.commit().await.unwrap();
    let (second, result) = task.await.unwrap();
    assert!(result.unwrap());
    second.commit().await.unwrap();

    let (messages_left, thread_left): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE thread_id = $1),
                (SELECT count(*) FROM threads WHERE id = $1)",
    )
    .bind(thread)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(
        (messages_left, thread_left),
        (0, 0),
        "the final erasure must remove the shared thread"
    );
}

#[tokio::test]
async fn reply_creation_survives_concurrent_final_parent_erasure() {
    let s = store().await;
    let mailbox = s.create_node_mailbox("thread race").await.unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let parent_id = format!("<{}@test>", Uuid::now_v7());
    let thread = s.thread_for(None, &[], Some("thread race")).await.unwrap();
    let parent = s
        .store_message(
            unique("parent").as_bytes(),
            thread,
            &from,
            Some("thread race"),
            Some(&parent_id),
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let inbox = s.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    s.deliver(mailbox.id, parent, inbox).await.unwrap();
    finish_submission(&s, parent).await;

    let mut reply_tx = s.pool().begin().await.unwrap();
    let (reply_pid,): (i32,) = sqlx::query_as("SELECT pg_backend_pid()")
        .fetch_one(&mut *reply_tx)
        .await
        .unwrap();
    let reply_thread = s
        .thread_for_in_transaction(
            &mut reply_tx,
            Some(&parent_id),
            &[],
            Some("Re: thread race"),
        )
        .await
        .unwrap();
    assert_eq!(reply_thread, thread);
    let other = s.clone();
    let erasure = tokio::spawn(async move { other.erase_membership(mailbox.id, parent).await });
    wait_for_block_or_finish(&s, reply_pid, &erasure).await;
    let reply = s
        .store_message_in_transaction(
            &mut reply_tx,
            unique("reply").as_bytes(),
            reply_thread,
            &from,
            Some("Re: thread race"),
            None,
            Some(&parent_id),
            &[],
            Transport::Internal,
            None,
            None,
        )
        .await
        .expect("reply creation must not lose its thread during parent erasure");
    reply_tx.commit().await.unwrap();
    erasure.await.unwrap().unwrap();

    let (reply_exists, thread_exists): (bool, bool) = sqlx::query_as(
        "SELECT EXISTS (SELECT 1 FROM messages WHERE id = $1 AND thread_id = $2),
                EXISTS (SELECT 1 FROM threads WHERE id = $2)",
    )
    .bind(reply)
    .bind(thread)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert!(reply_exists && thread_exists);
}

#[tokio::test]
async fn parent_erasure_rechecks_thread_after_concurrent_reply_commits() {
    let s = store().await;
    let mailbox = s
        .create_node_mailbox("thread race committed reply")
        .await
        .unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let parent_id = format!("<{}@test>", Uuid::now_v7());
    let thread = s.thread_for(None, &[], Some("thread race")).await.unwrap();
    let parent = s
        .store_message(
            unique("parent").as_bytes(),
            thread,
            &from,
            Some("thread race"),
            Some(&parent_id),
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let inbox = s.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    s.deliver(mailbox.id, parent, inbox).await.unwrap();
    finish_submission(&s, parent).await;

    let mut reply_tx = s.pool().begin().await.unwrap();
    let (reply_pid,): (i32,) = sqlx::query_as("SELECT pg_backend_pid()")
        .fetch_one(&mut *reply_tx)
        .await
        .unwrap();
    let reply_thread = s
        .thread_for_in_transaction(
            &mut reply_tx,
            Some(&parent_id),
            &[],
            Some("Re: thread race"),
        )
        .await
        .unwrap();
    let reply = s
        .store_message_in_transaction(
            &mut reply_tx,
            unique("reply").as_bytes(),
            reply_thread,
            &from,
            Some("Re: thread race"),
            None,
            Some(&parent_id),
            &[],
            Transport::Internal,
            None,
            None,
        )
        .await
        .unwrap();
    let other = s.clone();
    let erasure = tokio::spawn(async move { other.erase_membership(mailbox.id, parent).await });
    wait_for_block_or_finish(&s, reply_pid, &erasure).await;
    reply_tx.commit().await.unwrap();
    erasure
        .await
        .unwrap()
        .expect("erasure must recheck references after the thread lock");

    let (reply_exists, thread_exists): (bool, bool) = sqlx::query_as(
        "SELECT EXISTS (SELECT 1 FROM messages WHERE id = $1 AND thread_id = $2),
                EXISTS (SELECT 1 FROM threads WHERE id = $2)",
    )
    .bind(reply)
    .bind(thread)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert!(reply_exists && thread_exists);
}

#[tokio::test]
async fn unfinished_submission_hold_prevents_orphan_cleanup_after_restart() {
    let s = store().await;
    let mailbox = s.create_node_mailbox("unfinished").await.unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s.thread_for(None, &[], None).await.unwrap();
    let msg = s
        .store_message(
            unique("unfinished").as_bytes(),
            thread,
            &from,
            None,
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let inbox = s.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    s.deliver(mailbox.id, msg, inbox).await.unwrap();
    let restarted = MailStore::new(s.pool().clone());
    restarted.erase_membership(mailbox.id, msg).await.unwrap();

    let kinds: Vec<(String,)> = sqlx::query_as(
        "SELECT kind FROM message_references WHERE message_id = $1 AND released_at IS NULL",
    )
    .bind(msg)
    .fetch_all(s.pool())
    .await
    .unwrap();
    assert_eq!(kinds, vec![("submission".to_string(),)]);
    let (count,): (i64,) = sqlx::query_as("SELECT count(*) FROM messages WHERE id = $1")
        .bind(msg)
        .fetch_one(s.pool())
        .await
        .unwrap();
    assert_eq!(count, 1);
}

#[tokio::test]
async fn duplicate_delivery_rejects_a_missing_membership_hold() {
    let s = store().await;
    let mailbox = s.create_node_mailbox("missing hold").await.unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s.thread_for(None, &[], None).await.unwrap();
    let msg = s
        .store_message(
            unique("missing-hold").as_bytes(),
            thread,
            &from,
            None,
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let inbox = s.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    s.deliver(mailbox.id, msg, inbox).await.unwrap();
    sqlx::query("UPDATE message_references SET released_at = now() WHERE message_id = $1 AND kind = 'mailbox'")
        .bind(msg).execute(s.pool()).await.unwrap();
    assert!(s.deliver(mailbox.id, msg, inbox).await.is_err());
}

#[tokio::test]
async fn deduplicated_submissions_have_independent_holds() {
    let s = store().await;
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s.thread_for(None, &[], None).await.unwrap();
    let raw = unique("concurrent-submission");
    let (first, second) = tokio::join!(
        s.store_message(
            raw.as_bytes(),
            thread,
            &from,
            None,
            None,
            None,
            &[],
            Transport::Internal,
            None
        ),
        s.store_message(
            raw.as_bytes(),
            thread,
            &from,
            None,
            None,
            None,
            &[],
            Transport::Internal,
            None
        ),
    );
    let first = first.unwrap();
    let second = second.unwrap();
    assert_eq!(first, second);
    let (active,): (i64,) = sqlx::query_as(
        "SELECT count(*) FROM message_references WHERE message_id = $1 AND kind = 'submission' AND released_at IS NULL",
    ).bind(first).fetch_one(s.pool()).await.unwrap();
    assert_eq!(
        active, 2,
        "one submission must not release another submission's hold"
    );
}

#[tokio::test]
async fn reference_migration_backfills_existing_and_unfinished_messages() {
    let s = store().await;
    let mut tx = s.pool().begin().await.unwrap();
    let schema = format!("reference_test_{}", Uuid::now_v7().simple());
    sqlx::query(&format!("CREATE SCHEMA {schema}"))
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query("SELECT set_config('search_path', $1, true)")
        .bind(&schema)
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::raw_sql(
        "CREATE TABLE messages (id UUID PRIMARY KEY);
         CREATE TABLE mailbox_messages (mailbox_id UUID, message_id UUID REFERENCES messages(id));
         CREATE TABLE outbound_queue (
             id UUID PRIMARY KEY, message_id UUID REFERENCES messages(id) ON DELETE CASCADE,
             state TEXT NOT NULL
         );",
    )
    .execute(&mut *tx)
    .await
    .unwrap();
    let mailbox_message = Uuid::now_v7();
    let queue_message = Uuid::now_v7();
    let orphan = Uuid::now_v7();
    let terminal = Uuid::now_v7();
    for id in [mailbox_message, queue_message, orphan, terminal] {
        sqlx::query("INSERT INTO messages (id) VALUES ($1)")
            .bind(id)
            .execute(&mut *tx)
            .await
            .unwrap();
    }
    sqlx::query("INSERT INTO mailbox_messages (mailbox_id, message_id) VALUES ($1, $2)")
        .bind(Uuid::now_v7())
        .bind(mailbox_message)
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query("INSERT INTO outbound_queue (id, message_id, state) VALUES ($1, $2, 'delivering'), ($3, $4, 'delivered')")
        .bind(Uuid::now_v7()).bind(queue_message).bind(Uuid::now_v7()).bind(terminal)
        .execute(&mut *tx).await.unwrap();
    sqlx::raw_sql(include_str!(
        "../migrations/20261104000010_message_reference_safety.sql"
    ))
    .execute(&mut *tx)
    .await
    .unwrap();
    for (message, kind) in [
        (mailbox_message, "mailbox"),
        (queue_message, "outbound"),
        (orphan, "submission"),
        (terminal, "submission"),
    ] {
        let rows: Vec<(String,)> = sqlx::query_as(
            "SELECT kind FROM message_references WHERE message_id = $1 AND released_at IS NULL",
        )
        .bind(message)
        .fetch_all(&mut *tx)
        .await
        .unwrap();
        assert_eq!(rows, vec![(kind.to_string(),)]);
    }
    tx.rollback().await.unwrap();
}

#[tokio::test]
async fn submission_migration_recovers_legacy_inflight_work_and_unfinished_submissions() {
    let s = store().await;
    let mut tx = s.pool().begin().await.unwrap();
    let schema = format!("intent_migration_{}", Uuid::now_v7().simple());
    sqlx::query(&format!("CREATE SCHEMA {schema}"))
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query("SELECT set_config('search_path', $1, true)")
        .bind(&schema)
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::raw_sql(
        "CREATE TABLE messages (id UUID PRIMARY KEY);
         CREATE TABLE mailbox_messages (id UUID PRIMARY KEY);
         CREATE TABLE outbound_queue (
           id UUID PRIMARY KEY, message_id UUID NOT NULL REFERENCES messages(id),
           recipient TEXT NOT NULL, route TEXT NOT NULL, state TEXT NOT NULL,
           CONSTRAINT queue_no_duplicate_recipient UNIQUE (message_id, recipient)
         );
         CREATE TABLE message_references (
           id UUID PRIMARY KEY, message_id UUID NOT NULL, kind TEXT NOT NULL,
           released_at TIMESTAMPTZ, protected_id UUID
         );
         CREATE TABLE message_submission_dispositions (
           id UUID PRIMARY KEY, message_id UUID NOT NULL, recipient TEXT NOT NULL,
           disposition TEXT NOT NULL, membership_id UUID
         );",
    )
    .execute(&mut *tx)
    .await
    .unwrap();
    let message = Uuid::now_v7();
    let queue = Uuid::now_v7();
    sqlx::query("INSERT INTO messages VALUES ($1)")
        .bind(message)
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query(
        "INSERT INTO outbound_queue VALUES ($1, $2, 'outside@example.test', 'smtp', 'delivering')",
    )
    .bind(queue)
    .bind(message)
    .execute(&mut *tx)
    .await
    .unwrap();
    sqlx::query("INSERT INTO message_references VALUES ($1, $2, 'submission', NULL, $1)")
        .bind(Uuid::now_v7())
        .bind(message)
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query("SAVEPOINT inconsistent_legacy")
        .execute(&mut *tx)
        .await
        .unwrap();
    sqlx::query("INSERT INTO message_references VALUES ($1, $2, 'outbound', NULL, $3)")
        .bind(Uuid::now_v7())
        .bind(message)
        .bind(Uuid::now_v7())
        .execute(&mut *tx)
        .await
        .unwrap();
    let migration = include_str!("../migrations/20261105000013_submission_intents.sql")
        .trim()
        .strip_prefix("BEGIN;")
        .unwrap()
        .strip_suffix("COMMIT;")
        .unwrap();
    assert!(sqlx::raw_sql(migration).execute(&mut *tx).await.is_err());
    sqlx::query("ROLLBACK TO SAVEPOINT inconsistent_legacy")
        .execute(&mut *tx)
        .await
        .unwrap();
    let (created,): (bool,) = sqlx::query_as("SELECT to_regclass('mail_submissions') IS NOT NULL")
        .fetch_one(&mut *tx)
        .await
        .unwrap();
    assert!(
        !created,
        "inconsistent legacy data must fail before schema changes"
    );
    sqlx::raw_sql(migration).execute(&mut *tx).await.unwrap();
    let (state,): (String,) = sqlx::query_as("SELECT state FROM outbound_queue WHERE id = $1")
        .bind(queue)
        .fetch_one(&mut *tx)
        .await
        .unwrap();
    assert_eq!(state, "pending");
    let intents: Vec<(String, String)> =
        sqlx::query_as("SELECT route, state FROM mail_submission_recipients ORDER BY route")
            .fetch_all(&mut *tx)
            .await
            .unwrap();
    assert_eq!(
        intents,
        vec![
            ("local".into(), "local_pending".into()),
            ("smtp".into(), "outbound_pending".into()),
        ]
    );
    let (legacy_refs,): (i64,) = sqlx::query_as("SELECT count(*) FROM message_references")
        .fetch_one(&mut *tx)
        .await
        .unwrap();
    assert_eq!(legacy_refs, 0);
    tx.rollback().await.unwrap();
}

#[tokio::test]
async fn concurrent_final_membership_erasure_deletes_shared_message_once() {
    let s = store().await;
    let first = s.create_node_mailbox("first").await.unwrap();
    let second = s.create_node_mailbox("second").await.unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s.thread_for(None, &[], None).await.unwrap();
    let reservation = s.reserve_external_body(chrono::Utc::now()).await.unwrap();
    let msg = s
        .store_message(
            unique("concurrent-erase").as_bytes(),
            thread,
            &from,
            None,
            None,
            None,
            &[],
            Transport::Internal,
            Some(&reservation),
        )
        .await
        .unwrap();
    for mailbox in [first.id, second.id] {
        let inbox = s.folder(mailbox, FolderKind::Inbox).await.unwrap();
        s.deliver(mailbox, msg, inbox).await.unwrap();
    }
    finish_submission(&s, msg).await;

    let (a, b) = tokio::join!(
        s.erase_membership(first.id, msg),
        s.erase_membership(second.id, msg)
    );
    a.unwrap();
    b.unwrap();
    let (messages, jobs): (i64, i64) = sqlx::query_as(
        "SELECT (SELECT count(*) FROM messages WHERE id = $1),
                (SELECT count(*) FROM object_deletion_queue WHERE generation_id = $2)",
    )
    .bind(msg)
    .bind(reservation.generation_id)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!((messages, jobs), (0, 1));
}

#[tokio::test]
async fn one_message_lands_in_many_mailboxes_without_being_copied() {
    // The central claim of the schema. If this regresses, every message
    // delivered to N recipients costs N copies of the body.
    let s = store().await;
    let alice = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let info = s.create_node_mailbox("Role: info").await.unwrap();

    let raw = b"From: a@tnhc.dev\r\nSubject: shared\r\n\r\nbody";
    let from = Address::parse("a@tnhc.dev").unwrap();
    let thread = s.thread_for(None, &[], Some("shared")).await.unwrap();
    let msg = s
        .store_message(
            raw,
            thread,
            &from,
            Some("shared"),
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();

    for m in [alice.id, info.id] {
        let inbox = s.folder(m, FolderKind::Inbox).await.unwrap();
        s.deliver(m, msg, inbox).await.unwrap();
    }

    // Read state is per mailbox: marking Alice's copy read must not touch the
    // role mailbox's.
    s.set_seen(alice.id, msg, true).await.unwrap();
    assert_eq!(s.unseen_count(alice.id).await.unwrap(), 0);
    assert_eq!(s.unseen_count(info.id).await.unwrap(), 1);

    let rows: (i64,) = sqlx::query_as("SELECT count(*) FROM messages WHERE id = $1")
        .bind(msg)
        .fetch_one(s.pool())
        .await
        .unwrap();
    assert_eq!(rows.0, 1, "the message must be stored exactly once");
}

#[tokio::test]
async fn identical_content_is_stored_once() {
    // Content addressing. A retried delivery must not duplicate the message.
    let s = store().await;
    let from = Address::parse("a@tnhc.dev").unwrap();
    let raw = format!("Subject: dedupe\r\n\r\n{}", unique("body"));
    let thread = s.thread_for(None, &[], Some("dedupe")).await.unwrap();

    let first = s
        .store_message(
            raw.as_bytes(),
            thread,
            &from,
            Some("dedupe"),
            None,
            None,
            &[],
            Transport::Smtp,
            None,
        )
        .await
        .unwrap();
    let second = s
        .store_message(
            raw.as_bytes(),
            thread,
            &from,
            Some("dedupe"),
            None,
            None,
            &[],
            Transport::Smtp,
            None,
        )
        .await
        .unwrap();

    assert_eq!(
        first, second,
        "same bytes must resolve to the same stored message"
    );
}

#[tokio::test]
async fn an_alias_and_the_primary_address_reach_the_same_mailbox() {
    // Aliases are ordinary, not a special case: several addresses route to one
    // mailbox.
    let s = store().await;
    let mb = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let domain = unique("d").to_lowercase();

    let primary = Address::parse(&format!("alice@{domain}")).unwrap();
    let alias = Address::parse(&format!("sales@{domain}")).unwrap();
    s.add_address(mb.id, &primary, true).await.unwrap();
    s.add_address(mb.id, &alias, false).await.unwrap();

    assert_eq!(s.resolve(&primary).await.unwrap(), mb.id);
    assert_eq!(s.resolve(&alias).await.unwrap(), mb.id);
}

#[tokio::test]
async fn an_unrouted_address_is_reported_as_unknown_not_as_an_error() {
    // SMTP must distinguish "no such user here" (550, permanent) from a
    // database failure (451, try again). Collapsing them loses real mail or
    // accepts mail for people who do not exist.
    let s = store().await;
    let nobody = Address::parse(&format!("{}@{}", unique("no"), unique("d"))).unwrap();
    match s.resolve(&nobody).await {
        Err(nexus_mailstore::MailStoreError::NoSuchAddress(_)) => {}
        other => panic!("expected NoSuchAddress, got {other:?}"),
    }
}

#[tokio::test]
async fn a_message_cannot_be_filed_into_another_mailboxs_folder() {
    // Enforced by a composite foreign key, not by application code. Getting
    // this wrong makes one person's mail visible in another person's folder.
    let s = store().await;
    let alice = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let bob = s
        .create_identity_mailbox(&unique("usr"), "Bob")
        .await
        .unwrap();

    let from = Address::parse("a@tnhc.dev").unwrap();
    let thread = s.thread_for(None, &[], Some("x")).await.unwrap();
    let msg = s
        .store_message(
            unique("m").as_bytes(),
            thread,
            &from,
            Some("x"),
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();

    let bobs_inbox = s.folder(bob.id, FolderKind::Inbox).await.unwrap();
    let err = s.deliver(alice.id, msg, bobs_inbox).await;
    assert!(
        err.is_err(),
        "the database must refuse a cross-mailbox folder"
    );
}

#[tokio::test]
async fn a_reply_joins_its_parents_thread() {
    // Header references win over subject matching.
    let s = store().await;
    let from = Address::parse("a@tnhc.dev").unwrap();
    let parent_msgid = format!("<{}@tnhc.dev>", unique("p"));

    let thread = s
        .thread_for(None, &[], Some("Design review"))
        .await
        .unwrap();
    s.store_message(
        unique("parent").as_bytes(),
        thread,
        &from,
        Some("Design review"),
        Some(&parent_msgid),
        None,
        &[],
        Transport::Internal,
        None,
    )
    .await
    .unwrap();

    let reply_thread = s
        .thread_for(Some(&parent_msgid), &[], Some("Re: Design review"))
        .await
        .unwrap();
    assert_eq!(
        reply_thread, thread,
        "the reply must join the parent's thread"
    );
}

#[tokio::test]
async fn unrelated_messages_sharing_a_subject_do_not_merge() {
    // Subject grouping is a fallback, and must not drag strangers into one
    // conversation when the headers say nothing.
    let s = store().await;
    let a = s
        .thread_for(None, &[], Some(&unique("Subject")))
        .await
        .unwrap();
    let b = s
        .thread_for(None, &[], Some(&unique("Subject")))
        .await
        .unwrap();
    assert_ne!(a, b);
}

#[tokio::test]
async fn trash_membership_expires_after_seven_days_and_last_copy_is_erased() {
    let s = store().await;
    let alice = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let bob = s
        .create_identity_mailbox(&unique("usr"), "Bob")
        .await
        .unwrap();
    let from = Address::parse("a@tnhc.dev").unwrap();
    let thread = s.thread_for(None, &[], Some("lifecycle")).await.unwrap();

    let msg = s
        .store_message(
            unique("lifecycle").as_bytes(),
            thread,
            &from,
            Some("lifecycle"),
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();

    let alice_inbox = s.folder(alice.id, FolderKind::Inbox).await.unwrap();
    let bob_inbox = s.folder(bob.id, FolderKind::Inbox).await.unwrap();
    let bob_trash = s.folder(bob.id, FolderKind::Trash).await.unwrap();
    s.deliver(alice.id, msg, alice_inbox).await.unwrap();
    s.deliver(bob.id, msg, bob_inbox).await.unwrap();
    finish_submission(&s, msg).await;

    s.erase_membership(alice.id, msg).await.unwrap();

    let (remaining_after_single_erase,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM mailbox_messages WHERE message_id = $1")
            .bind(msg)
            .fetch_one(s.pool())
            .await
            .unwrap();
    assert_eq!(remaining_after_single_erase, 1);

    let moved_at = chrono::DateTime::parse_from_rfc3339("2000-01-01T00:00:00Z")
        .unwrap()
        .with_timezone(&chrono::Utc);
    s.move_to_folder_at(bob.id, msg, bob_trash, moved_at)
        .await
        .unwrap();

    let (trash_expires_at,): (Option<chrono::DateTime<chrono::Utc>>,) = sqlx::query_as(
        "SELECT trash_expires_at FROM mailbox_messages WHERE mailbox_id = $1 AND message_id = $2",
    )
    .bind(bob.id)
    .bind(msg)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(
        trash_expires_at,
        Some(moved_at + Duration::days(7)),
        "trash expiry must be exactly seven days after move"
    );

    s.move_to_folder_at(bob.id, msg, bob_inbox, moved_at + Duration::days(1))
        .await
        .unwrap();
    let (cleared_expiry,): (Option<chrono::DateTime<chrono::Utc>>,) = sqlx::query_as(
        "SELECT trash_expires_at FROM mailbox_messages WHERE mailbox_id = $1 AND message_id = $2",
    )
    .bind(bob.id)
    .bind(msg)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(
        cleared_expiry, None,
        "moving out of Trash must clear expiry"
    );

    s.move_to_folder_at(bob.id, msg, bob_trash, moved_at + Duration::days(2))
        .await
        .unwrap();

    s.purge_expired_trash(moved_at + Duration::days(8), 64)
        .await
        .unwrap();
    let (memberships_before_expiry,): (i64,) = sqlx::query_as(
        "SELECT count(*) FROM mailbox_messages WHERE mailbox_id = $1 AND message_id = $2",
    )
    .bind(bob.id)
    .bind(msg)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(memberships_before_expiry, 1);

    let purge_at = moved_at + Duration::days(10);
    let (eligible_for_purge,): (i64,) = sqlx::query_as(
        "SELECT count(*)
         FROM mailbox_messages mm
         JOIN folders f ON f.id = mm.folder_id AND f.mailbox_id = mm.mailbox_id
         WHERE mm.mailbox_id = $1
           AND mm.message_id = $2
           AND f.kind = 'trash'
           AND mm.trash_expires_at IS NOT NULL
           AND mm.trash_expires_at <= $3",
    )
    .bind(bob.id)
    .bind(msg)
    .bind(purge_at)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(eligible_for_purge, 1);

    for _ in 0..200 {
        s.purge_expired_trash(purge_at, 10_000).await.unwrap();
        let (remaining,): (i64,) = sqlx::query_as(
            "SELECT count(*) FROM mailbox_messages WHERE mailbox_id = $1 AND message_id = $2",
        )
        .bind(bob.id)
        .bind(msg)
        .fetch_one(s.pool())
        .await
        .unwrap();
        if remaining == 0 {
            break;
        }
    }

    let (memberships_after_purge,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM mailbox_messages WHERE message_id = $1")
            .bind(msg)
            .fetch_one(s.pool())
            .await
            .unwrap();
    let (messages_after_purge,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM messages WHERE id = $1")
            .bind(msg)
            .fetch_one(s.pool())
            .await
            .unwrap();
    assert_eq!(memberships_after_purge, 0);
    assert_eq!(messages_after_purge, 0);
}

#[tokio::test]
async fn purge_only_erases_memberships_still_in_expired_trash() {
    let s = store().await;
    let mailbox = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let from = Address::parse("a@tnhc.dev").unwrap();
    let thread = s.thread_for(None, &[], Some("race-guard")).await.unwrap();
    let message_id = s
        .store_message(
            unique("race-guard").as_bytes(),
            thread,
            &from,
            Some("race-guard"),
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();

    let inbox = s.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    s.deliver(mailbox.id, message_id, inbox).await.unwrap();

    let now = chrono::DateTime::parse_from_rfc3339("2026-11-09T00:00:00Z")
        .unwrap()
        .with_timezone(&chrono::Utc);
    sqlx::query(
        "UPDATE mailbox_messages
         SET trash_expires_at = $1
         WHERE mailbox_id = $2 AND message_id = $3",
    )
    .bind(now - Duration::hours(1))
    .bind(mailbox.id)
    .bind(message_id)
    .execute(s.pool())
    .await
    .unwrap();

    let purged = s.purge_expired_trash(now, 32).await.unwrap();
    assert_eq!(
        purged, 0,
        "memberships not currently in Trash must never be purged"
    );

    let (memberships,): (i64,) = sqlx::query_as(
        "SELECT count(*) FROM mailbox_messages WHERE mailbox_id = $1 AND message_id = $2",
    )
    .bind(mailbox.id)
    .bind(message_id)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(memberships, 1);
}

#[tokio::test]
async fn migration_backfill_keeps_existing_trash_for_at_least_seven_days() {
    let migration = std::fs::read_to_string(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/migrations/20261101000006_mail_lifecycle.sql"
    ))
    .unwrap();
    assert!(
        migration.contains("GREATEST(now(), mm.added_at) + interval '7 days'"),
        "migration backfill must clamp to migration-time floor"
    );

    let s = store().await;
    let mailbox = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let from = Address::parse("a@tnhc.dev").unwrap();
    let thread = s
        .thread_for(None, &[], Some("migration-backfill"))
        .await
        .unwrap();
    let message_id = s
        .store_message(
            unique("migration-backfill").as_bytes(),
            thread,
            &from,
            Some("migration-backfill"),
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();

    let trash = s.folder(mailbox.id, FolderKind::Trash).await.unwrap();
    s.deliver(mailbox.id, message_id, trash).await.unwrap();

    let very_old = chrono::DateTime::parse_from_rfc3339("2019-01-01T00:00:00Z")
        .unwrap()
        .with_timezone(&chrono::Utc);
    sqlx::query(
        "UPDATE mailbox_messages
         SET added_at = $1, trash_expires_at = NULL
         WHERE mailbox_id = $2 AND message_id = $3",
    )
    .bind(very_old)
    .bind(mailbox.id)
    .bind(message_id)
    .execute(s.pool())
    .await
    .unwrap();

    let activation_floor = chrono::Utc::now();
    sqlx::query(
        "UPDATE mailbox_messages mm
         SET trash_expires_at = GREATEST(now(), mm.added_at) + interval '7 days'
         FROM folders f
         WHERE mm.folder_id = f.id
           AND mm.mailbox_id = f.mailbox_id
           AND f.kind = 'trash'
           AND mm.trash_expires_at IS NULL",
    )
    .execute(s.pool())
    .await
    .unwrap();

    let (expires_at,): (chrono::DateTime<chrono::Utc>,) = sqlx::query_as(
        "SELECT trash_expires_at
         FROM mailbox_messages
         WHERE mailbox_id = $1 AND message_id = $2",
    )
    .bind(mailbox.id)
    .bind(message_id)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert!(
        expires_at >= activation_floor + Duration::days(7),
        "backfill must retain existing Trash for at least seven days from migration activation"
    );
}

#[tokio::test]
async fn empty_trash_does_not_erase_a_membership_restored_after_listing() {
    let s = store().await;
    let mailbox = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s
        .thread_for(None, &[], Some("empty-trash-race"))
        .await
        .unwrap();
    let message_id = s
        .store_message(
            unique("empty-trash-race").as_bytes(),
            thread,
            &from,
            Some("empty-trash-race"),
            None,
            None,
            &[],
            Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let inbox = s.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    let trash = s.folder(mailbox.id, FolderKind::Trash).await.unwrap();
    s.deliver(mailbox.id, message_id, trash).await.unwrap();

    let listed = s.list_folder(mailbox.id, trash, 200, 0).await.unwrap();
    assert_eq!(
        listed.len(),
        1,
        "the stale Empty Trash batch must contain it"
    );

    s.move_to_folder(mailbox.id, message_id, inbox)
        .await
        .unwrap();
    assert!(
        !s.erase_trash_membership(mailbox.id, message_id)
            .await
            .unwrap(),
        "the conditional deletion must reject a membership restored after listing"
    );
    assert_eq!(
        s.message_folder(mailbox.id, message_id)
            .await
            .unwrap()
            .unwrap()
            .id,
        inbox
    );
}

#[tokio::test]
async fn final_erasure_durably_queues_external_body_deletion_for_retry() {
    let s = store().await;
    let mailbox = s
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s
        .thread_for(None, &[], Some("external-body-delete"))
        .await
        .unwrap();
    let reservation = s.reserve_external_body(chrono::Utc::now()).await.unwrap();
    let external_key = reservation.object_key.clone();
    let raw = format!("stored externally {}", unique("body"));
    let message_id = s
        .store_message(
            raw.as_bytes(),
            thread,
            &from,
            Some("external-body-delete"),
            None,
            None,
            &[],
            Transport::Internal,
            Some(&reservation),
        )
        .await
        .unwrap();
    let inbox = s.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    s.deliver(mailbox.id, message_id, inbox).await.unwrap();

    finish_submission(&s, message_id).await;
    s.erase_membership(mailbox.id, message_id).await.unwrap();

    let now = chrono::Utc::now();
    sqlx::query(
        "UPDATE object_deletion_queue q
         SET next_attempt_at = $1
         FROM object_generations g
         WHERE q.generation_id = g.id AND g.object_key = $2",
    )
    .bind(now - Duration::days(36_500))
    .bind(&external_key)
    .execute(s.pool())
    .await
    .unwrap();
    let job = s
        .claim_object_deletion_jobs(now, 1)
        .await
        .unwrap()
        .pop()
        .expect("the erased generation is queued");
    assert_eq!(
        job.object_key, external_key,
        "external cleanup must use the exact key already uploaded by the caller"
    );

    s.retry_object_deletion(job.id, now).await.unwrap();
    assert!(
        s.claim_object_deletion_jobs(now, 1)
            .await
            .unwrap()
            .into_iter()
            .all(|candidate| candidate.id != job.id),
        "a failed deletion must remain durably queued until its retry delay"
    );
    let retry = s
        .claim_object_deletion_jobs(now + Duration::minutes(2), 1)
        .await
        .unwrap();
    let retry = retry
        .into_iter()
        .next()
        .expect("the queued external object must become retryable");
    s.complete_object_deletion(retry.id, retry.generation_id)
        .await
        .unwrap();
    assert!(
        s.claim_object_deletion_jobs(now + Duration::minutes(4), 1)
            .await
            .unwrap()
            .into_iter()
            .all(|candidate| candidate.id != job.id),
        "only a successful object-store deletion may remove the durable job"
    );
}

#[tokio::test]
async fn external_body_keeps_the_reserved_object_key() {
    let s = store().await;
    let from = Address::parse("sender@example.test").unwrap();
    let reservation = s.reserve_external_body(chrono::Utc::now()).await.unwrap();
    let external_key = reservation.object_key.clone();
    let raw = format!("externally uploaded content {}", unique("body"));
    let thread = s
        .thread_for(None, &[], Some("external-body"))
        .await
        .unwrap();
    let message = s
        .store_message(
            raw.as_bytes(),
            thread,
            &from,
            Some("external-body"),
            None,
            None,
            &[],
            Transport::Internal,
            Some(&reservation),
        )
        .await
        .unwrap();
    let stored_key: (String,) =
        sqlx::query_as("SELECT body_object_key FROM messages WHERE id = $1")
            .bind(message)
            .fetch_one(s.pool())
            .await
            .unwrap();

    assert_eq!(
        stored_key.0, external_key,
        "store_message must preserve the key of bytes already uploaded by its caller"
    );
}

#[tokio::test]
async fn finalized_reservation_retry_returns_the_original_message_when_metadata_matches() {
    let s = store().await;
    let from = Address::parse("sender@example.test").unwrap();
    let reservation = s.reserve_external_body(chrono::Utc::now()).await.unwrap();
    let thread = s.thread_for(None, &[], Some("retry-match")).await.unwrap();
    let raw = format!(
        "{}{}",
        "x".repeat(INLINE_BODY_LIMIT + 128),
        unique("retry-match-body")
    )
    .into_bytes();
    let references = vec![
        "<first@example.test>".to_string(),
        "<second@example.test>".to_string(),
    ];

    let first = s
        .store_message(
            &raw,
            thread,
            &from,
            Some("retry-match"),
            Some("<retry-match@example.test>"),
            Some("<parent@example.test>"),
            &references,
            Transport::Internal,
            Some(&reservation),
        )
        .await
        .unwrap();

    let retried = s
        .store_message(
            &raw,
            thread,
            &from,
            Some("retry-match"),
            Some("<retry-match@example.test>"),
            Some("<parent@example.test>"),
            &references,
            Transport::Internal,
            Some(&reservation),
        )
        .await
        .unwrap();

    assert_eq!(
        retried, first,
        "a finalized reservation retry with identical payload and metadata must return the original message id"
    );
}

#[tokio::test]
async fn finalized_reservation_retry_rejects_metadata_mismatch() {
    let s = store().await;
    let from = Address::parse("sender@example.test").unwrap();
    let reservation = s.reserve_external_body(chrono::Utc::now()).await.unwrap();
    let thread = s
        .thread_for(None, &[], Some("retry-mismatch"))
        .await
        .unwrap();
    let raw = format!(
        "{}{}",
        "y".repeat(INLINE_BODY_LIMIT + 64),
        unique("retry-mismatch-body")
    )
    .into_bytes();
    let references = vec!["<ref@example.test>".to_string()];

    let _ = s
        .store_message(
            &raw,
            thread,
            &from,
            Some("retry-mismatch"),
            Some("<retry-mismatch@example.test>"),
            None,
            &references,
            Transport::Internal,
            Some(&reservation),
        )
        .await
        .unwrap();

    let mismatch = s
        .store_message(
            &raw,
            thread,
            &from,
            Some("retry-mismatch-updated"),
            Some("<retry-mismatch@example.test>"),
            None,
            &references,
            Transport::Internal,
            Some(&reservation),
        )
        .await;
    assert!(matches!(
        mismatch,
        Err(MailStoreError::ExternalBodyReservationUnavailable)
    ));
}

#[tokio::test]
async fn externally_stored_body_rejects_a_caller_selected_key() {
    let s = store().await;
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s.thread_for(None, &[], Some("reserved")).await.unwrap();
    let reservation = s.reserve_external_body(chrono::Utc::now()).await.unwrap();
    let caller_selected = ExternalBodyReservation {
        generation_id: reservation.generation_id,
        object_key: unique("caller-selected-key"),
        expires_at: reservation.expires_at,
    };
    let result = s
        .store_message(
            &vec![b'x'; INLINE_BODY_LIMIT + 1],
            thread,
            &from,
            Some("reserved"),
            None,
            None,
            &[],
            Transport::Internal,
            Some(&caller_selected),
        )
        .await;
    assert!(matches!(
        result,
        Err(MailStoreError::ExternalBodyReservationUnavailable)
    ));
}

#[tokio::test]
async fn expired_uploaded_reservation_is_queued_without_affecting_live_body() {
    let s = store().await;
    let expired = s
        .reserve_external_body(chrono::Utc::now() - Duration::minutes(16))
        .await
        .unwrap();
    let live = s.reserve_external_body(chrono::Utc::now()).await.unwrap();

    assert_eq!(
        s.queue_expired_external_body_reservations(chrono::Utc::now(), 10)
            .await
            .unwrap(),
        1
    );

    let (expired_jobs,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM object_deletion_queue WHERE generation_id = $1")
            .bind(expired.generation_id)
            .fetch_one(s.pool())
            .await
            .unwrap();
    let (live_jobs,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM object_deletion_queue WHERE generation_id = $1")
            .bind(live.generation_id)
            .fetch_one(s.pool())
            .await
            .unwrap();

    assert_eq!(expired_jobs, 1);
    assert_eq!(live_jobs, 0);
}

#[tokio::test]
async fn abandoning_live_reservation_queues_generation_cleanup() {
    let s = store().await;
    let reservation = s.reserve_external_body(chrono::Utc::now()).await.unwrap();
    let now = chrono::Utc::now();

    s.abandon_external_body_reservation(&reservation, now)
        .await
        .unwrap();

    let (queued_jobs,): (i64,) = sqlx::query_as(
        "SELECT count(*)
         FROM object_deletion_queue
         WHERE generation_id = $1 AND object_key = $2",
    )
    .bind(reservation.generation_id)
    .bind(&reservation.object_key)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(
        queued_jobs, 1,
        "abandonment must queue cleanup for the reserved immutable generation"
    );

    let row: (String, Option<chrono::DateTime<chrono::Utc>>) = sqlx::query_as(
        "SELECT lifecycle_state, reservation_expires_at
         FROM object_generations
         WHERE id = $1",
    )
    .bind(reservation.generation_id)
    .fetch_one(s.pool())
    .await
    .unwrap();
    assert_eq!(row.0, "cleanup_queued");
    assert_eq!(row.1, None);
}

#[tokio::test]
async fn lifecycle_migration_rollback_removes_and_restores_its_column_and_index() {
    let down = std::fs::read_to_string(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../docs/migration-rollbacks/20261101000006_mail_lifecycle.sql"
    ))
    .expect("the lifecycle migration must supply its rollback script");
    assert!(down.contains("DROP INDEX IF EXISTS mailbox_messages_trash_expiry_idx"));
    assert!(down.contains("DROP COLUMN IF EXISTS trash_expires_at"));

    let up = std::fs::read_to_string(concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/migrations/20261101000006_mail_lifecycle.sql"
    ))
    .unwrap();
    let s = store().await;
    let mut tx = s.pool().begin().await.unwrap();
    for statement in down
        .split(';')
        .filter(|statement| !statement.trim().is_empty())
    {
        sqlx::query(statement).execute(&mut *tx).await.unwrap();
    }

    let (column_removed,): (bool,) = sqlx::query_as(
        "SELECT NOT EXISTS (
             SELECT 1
             FROM information_schema.columns
             WHERE table_name = 'mailbox_messages'
               AND column_name = 'trash_expires_at'
         )",
    )
    .fetch_one(&mut *tx)
    .await
    .unwrap();
    let (index_removed,): (bool,) = sqlx::query_as(
        "SELECT NOT EXISTS (
             SELECT 1
             FROM pg_indexes
             WHERE schemaname = current_schema()
               AND indexname = 'mailbox_messages_trash_expiry_idx'
         )",
    )
    .fetch_one(&mut *tx)
    .await
    .unwrap();
    assert!(column_removed && index_removed);

    for statement in up
        .split(';')
        .filter(|statement| !statement.trim().is_empty())
    {
        sqlx::query(statement).execute(&mut *tx).await.unwrap();
    }
    tx.rollback().await.unwrap();
}
