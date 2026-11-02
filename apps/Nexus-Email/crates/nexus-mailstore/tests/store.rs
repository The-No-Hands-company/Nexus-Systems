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
