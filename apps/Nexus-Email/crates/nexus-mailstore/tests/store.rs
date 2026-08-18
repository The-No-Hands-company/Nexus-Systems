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

use nexus_mailstore::{Address, FolderKind, MailStore, Transport};
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
    let alice = s.create_identity_mailbox(&unique("usr"), "Alice").await.unwrap();
    let info = s.create_node_mailbox("Role: info").await.unwrap();

    let raw = b"From: a@tnhc.dev\r\nSubject: shared\r\n\r\nbody";
    let from = Address::parse("a@tnhc.dev").unwrap();
    let thread = s.thread_for(None, &[], Some("shared")).await.unwrap();
    let msg = s
        .store_message(raw, thread, &from, Some("shared"), None, None, &[], Transport::Internal, None)
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
        .store_message(raw.as_bytes(), thread, &from, Some("dedupe"), None, None, &[], Transport::Smtp, None)
        .await
        .unwrap();
    let second = s
        .store_message(raw.as_bytes(), thread, &from, Some("dedupe"), None, None, &[], Transport::Smtp, None)
        .await
        .unwrap();

    assert_eq!(first, second, "same bytes must resolve to the same stored message");
}

#[tokio::test]
async fn an_alias_and_the_primary_address_reach_the_same_mailbox() {
    // Aliases are ordinary, not a special case: several addresses route to one
    // mailbox.
    let s = store().await;
    let mb = s.create_identity_mailbox(&unique("usr"), "Alice").await.unwrap();
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
    let alice = s.create_identity_mailbox(&unique("usr"), "Alice").await.unwrap();
    let bob = s.create_identity_mailbox(&unique("usr"), "Bob").await.unwrap();

    let from = Address::parse("a@tnhc.dev").unwrap();
    let thread = s.thread_for(None, &[], Some("x")).await.unwrap();
    let msg = s
        .store_message(unique("m").as_bytes(), thread, &from, Some("x"), None, None, &[], Transport::Internal, None)
        .await
        .unwrap();

    let bobs_inbox = s.folder(bob.id, FolderKind::Inbox).await.unwrap();
    let err = s.deliver(alice.id, msg, bobs_inbox).await;
    assert!(err.is_err(), "the database must refuse a cross-mailbox folder");
}

#[tokio::test]
async fn a_reply_joins_its_parents_thread() {
    // Header references win over subject matching.
    let s = store().await;
    let from = Address::parse("a@tnhc.dev").unwrap();
    let parent_msgid = format!("<{}@tnhc.dev>", unique("p"));

    let thread = s.thread_for(None, &[], Some("Design review")).await.unwrap();
    s.store_message(
        unique("parent").as_bytes(), thread, &from, Some("Design review"),
        Some(&parent_msgid), None, &[], Transport::Internal, None,
    )
    .await
    .unwrap();

    let reply_thread = s
        .thread_for(Some(&parent_msgid), &[], Some("Re: Design review"))
        .await
        .unwrap();
    assert_eq!(reply_thread, thread, "the reply must join the parent's thread");
}

#[tokio::test]
async fn unrelated_messages_sharing_a_subject_do_not_merge() {
    // Subject grouping is a fallback, and must not drag strangers into one
    // conversation when the headers say nothing.
    let s = store().await;
    let a = s.thread_for(None, &[], Some(&unique("Subject"))).await.unwrap();
    let b = s.thread_for(None, &[], Some(&unique("Subject"))).await.unwrap();
    assert_ne!(a, b);
}
