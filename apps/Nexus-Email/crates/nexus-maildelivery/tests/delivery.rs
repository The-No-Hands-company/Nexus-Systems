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

    let hits = store.search(bob_mb.id, &needle, 10).await.unwrap();
    assert_eq!(hits.len(), 1, "the body text should be searchable");

    // And it must not leak across mailboxes: Alice holds only her Sent copy,
    // which is the same message row — so scoping search by membership matters.
    let sender_hits = store.search(alice_mb.id, &needle, 10).await.unwrap();
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

    assert!(
        store.search(outsider.id, &needle, 10).await.unwrap().is_empty(),
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
