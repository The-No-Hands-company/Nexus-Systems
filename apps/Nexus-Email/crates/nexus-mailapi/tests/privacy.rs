//! Zero-retention at the API: what a signed-in user's send puts into the
//! message, and what the API writes to its log. Needs Postgres.

use std::cell::RefCell;
use std::sync::{Arc, Once};

use nexus_mailapi::{router, AppState};
use nexus_maildelivery::{Deliverer, Queue, Router};
use nexus_mailstore::{Address, MailStore, MessageReferenceKind};
use sqlx::postgres::PgPoolOptions;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use uuid::Uuid;

/// One global subscriber writing to a per-thread buffer (a scoped subscriber
/// per test flaps when sibling tests log with none installed).
mod capture {
    use super::*;
    thread_local! { static BUF: RefCell<Vec<u8>> = const { RefCell::new(Vec::new()) }; }

    #[derive(Clone, Copy)]
    struct Sink;
    impl std::io::Write for Sink {
        fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
            BUF.with(|b| b.borrow_mut().extend_from_slice(buf));
            Ok(buf.len())
        }
        fn flush(&mut self) -> std::io::Result<()> {
            Ok(())
        }
    }
    impl<'a> tracing_subscriber::fmt::MakeWriter<'a> for Sink {
        type Writer = Sink;
        fn make_writer(&'a self) -> Sink {
            *self
        }
    }
    pub fn start() {
        static ONCE: Once = Once::new();
        ONCE.call_once(|| {
            let sub = tracing_subscriber::fmt()
                .with_writer(Sink)
                .with_max_level(tracing::Level::TRACE)
                .finish();
            tracing::subscriber::set_global_default(sub).unwrap();
        });
        BUF.with(|b| b.borrow_mut().clear());
    }
    pub fn text() -> String {
        BUF.with(|b| String::from_utf8(b.borrow().clone()).unwrap())
    }
}

struct Api {
    addr: std::net::SocketAddr,
    store: MailStore,
    local: String,
}

async fn start(token: Option<&str>) -> Api {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new()
        .max_connections(4)
        .connect(&url)
        .await
        .unwrap();
    let store = MailStore::new(pool.clone());
    let local = format!("t{}.test", Uuid::now_v7().simple());
    let deliverer = Deliverer::new(
        store.clone(),
        Queue::new(pool),
        Router::new().with_local_domain(&local),
    );
    let state = Arc::new(AppState {
        store: store.clone(),
        deliverer,
        primary_domain: local.clone(),
        cloudflare_ingress_token: token.map(str::to_string),
        system_mail_token: None,
        system_mail_budget: nexus_mailapi::system_mail::SystemMailBudget::new(50),
    });
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    tokio::spawn(async move {
        let _ = axum::serve(listener, router(state)).await;
    });
    Api { addr, store, local }
}

/// A bare HTTP/1.1 POST; returns the status line and body.
async fn post(
    addr: std::net::SocketAddr,
    path: &str,
    headers: &[(&str, &str)],
    body: &[u8],
) -> String {
    let mut s = TcpStream::connect(addr).await.unwrap();
    let mut req = format!(
        "POST {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: {}\r\n",
        body.len()
    );
    for (k, v) in headers {
        req.push_str(&format!("{k}: {v}\r\n"));
    }
    req.push_str("\r\n");
    s.write_all(req.as_bytes()).await.unwrap();
    s.write_all(body).await.unwrap();
    let mut out = Vec::new();
    s.read_to_end(&mut out).await.unwrap();
    String::from_utf8_lossy(&out).into_owned()
}

async fn get(addr: std::net::SocketAddr, path: &str, headers: &[(&str, &str)]) -> String {
    let mut s = TcpStream::connect(addr).await.unwrap();
    let mut req = format!("GET {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n");
    for (k, v) in headers {
        req.push_str(&format!("{k}: {v}\r\n"));
    }
    req.push_str("\r\n");
    s.write_all(req.as_bytes()).await.unwrap();
    let mut out = Vec::new();
    s.read_to_end(&mut out).await.unwrap();
    String::from_utf8_lossy(&out).into_owned()
}

async fn delete(addr: std::net::SocketAddr, path: &str, headers: &[(&str, &str)]) -> String {
    let mut s = TcpStream::connect(addr).await.unwrap();
    let mut req = format!("DELETE {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n");
    for (k, v) in headers {
        req.push_str(&format!("{k}: {v}\r\n"));
    }
    req.push_str("\r\n");
    s.write_all(req.as_bytes()).await.unwrap();
    let mut out = Vec::new();
    s.read_to_end(&mut out).await.unwrap();
    String::from_utf8_lossy(&out).into_owned()
}

#[tokio::test]
async fn a_message_sent_through_the_api_carries_no_client_address_or_received_line() {
    let api = start(None).await;
    let subject = format!("usr-{}", Uuid::now_v7().simple());
    let mb = api
        .store
        .create_identity_mailbox(&subject, "Sender")
        .await
        .unwrap();
    let from = Address::parse(&format!("alice@{}", api.local)).unwrap();
    api.store.add_address(mb.id, &from, true).await.unwrap();

    // The API has no client-address input of its own, so the client's address
    // is offered every way a proxy would pass it; none may reach the message.
    let rcpt = format!("u-{}@elsewhere.test", Uuid::now_v7().simple());
    let body = format!(r#"{{"to":["{rcpt}"],"subject":"hello","text":"body text"}}"#);
    let resp = post(
        api.addr,
        "/api/v1/messages",
        &[
            ("Content-Type", "application/json"),
            ("X-Nexus-Subject", &subject),
            ("X-Forwarded-For", "203.0.113.50"),
            ("X-Real-IP", "203.0.113.50"),
            ("CF-Connecting-IP", "203.0.113.50"),
            ("User-Agent", "privacy-test/1.0"),
        ],
        body.as_bytes(),
    )
    .await;
    assert!(resp.starts_with("HTTP/1.1 200"), "{resp}");

    let (raw,): (Vec<u8>,) = sqlx::query_as(
        "SELECT m.body_inline FROM messages m JOIN outbound_queue q ON q.message_id = m.id WHERE q.recipient = $1",
    )
    .bind(&rcpt)
    .fetch_one(api.store.pool())
    .await
    .unwrap();
    let text = String::from_utf8_lossy(&raw).into_owned();
    assert!(text.contains("body text"));
    for needle in ["203.0.113.50", "127.0.0.1", "privacy-test"] {
        assert!(!text.contains(needle), "{needle} stored: {text}");
    }
    for line in text.lines() {
        let l = line.to_ascii_lowercase();
        assert!(!l.starts_with("received:"), "Received line: {line}");
        assert!(
            !l.starts_with("x-originating-ip"),
            "X-Originating-IP line: {line}"
        );
    }
}

#[tokio::test]
async fn a_failed_inbound_delivery_logs_no_recipient_address() {
    capture::start();
    let api = start(Some("tok")).await;
    // info@ on the primary domain passes the ingress check, but no mailbox
    // routes it, which surfaces as DeliveryError::Permanent{recipient}.
    let rcpt = format!("info@{}", api.local);
    let resp = post(
        api.addr,
        "/internal/v1/cloudflare-email",
        &[
            ("Authorization", "Bearer tok"),
            ("X-Nexus-Envelope-From", "stranger@sender.example"),
            ("X-Nexus-Envelope-To", &rcpt),
        ],
        b"From: stranger@sender.example\r\nSubject: x\r\n\r\nhi\r\n",
    )
    .await;
    assert!(resp.starts_with("HTTP/1.1 500"), "{resp}");

    let log = capture::text();
    assert!(
        log.contains("Cloudflare inbound delivery"),
        "failure must still be logged: {log:?}"
    );
    assert!(!log.contains(&rcpt), "recipient leaked: {log:?}");
    assert!(
        !log.contains(&api.local),
        "recipient domain leaked: {log:?}"
    );
    assert!(
        !log.contains("stranger@sender.example"),
        "sender leaked: {log:?}"
    );
}

#[tokio::test]
async fn a_mailbox_owner_can_move_a_message_between_special_folders() {
    let api = start(None).await;
    let subject = format!("usr-{}", Uuid::now_v7().simple());
    let mailbox = api
        .store
        .create_identity_mailbox(&subject, "Alice")
        .await
        .unwrap();
    let from = Address::parse(&format!("sender@{}", api.local)).unwrap();
    let thread = api
        .store
        .thread_for(None, &[], Some("Keep me"))
        .await
        .unwrap();
    let message = api
        .store
        .store_message(
            b"From: sender@example.test\r\nSubject: Keep me\r\n\r\nbody",
            thread,
            &from,
            Some("Keep me"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let inbox = api
        .store
        .folder(mailbox.id, nexus_mailstore::FolderKind::Inbox)
        .await
        .unwrap();
    api.store.deliver(mailbox.id, message, inbox).await.unwrap();

    let missing = post(
        api.addr,
        &format!("/api/v1/messages/{}/folder", Uuid::now_v7()),
        &[
            ("Content-Type", "application/json"),
            ("X-Nexus-Subject", &subject),
        ],
        br#"{"folder":"trash"}"#,
    )
    .await;
    assert!(missing.starts_with("HTTP/1.1 404"), "{missing}");

    let trash = post(
        api.addr,
        &format!("/api/v1/messages/{message}/folder"),
        &[
            ("Content-Type", "application/json"),
            ("X-Nexus-Subject", &subject),
        ],
        br#"{"folder":"trash"}"#,
    )
    .await;
    assert!(trash.starts_with("HTTP/1.1 204"), "{trash}");

    let trash_id = api
        .store
        .folder(mailbox.id, nexus_mailstore::FolderKind::Trash)
        .await
        .unwrap();
    assert_eq!(
        api.store
            .list_folder(mailbox.id, trash_id, 10, 0)
            .await
            .unwrap()
            .len(),
        1
    );
    assert!(api
        .store
        .list_folder(mailbox.id, inbox, 10, 0)
        .await
        .unwrap()
        .is_empty());

    let trash_to_junk = post(
        api.addr,
        &format!("/api/v1/messages/{message}/folder"),
        &[
            ("Content-Type", "application/json"),
            ("X-Nexus-Subject", &subject),
        ],
        br#"{"folder":"junk"}"#,
    )
    .await;
    assert!(trash_to_junk.starts_with("HTTP/1.1 400"), "{trash_to_junk}");

    let restore = post(
        api.addr,
        &format!("/api/v1/messages/{message}/folder"),
        &[
            ("Content-Type", "application/json"),
            ("X-Nexus-Subject", &subject),
        ],
        br#"{"folder":"inbox"}"#,
    )
    .await;
    assert!(restore.starts_with("HTTP/1.1 204"), "{restore}");
    assert_eq!(
        api.store
            .list_folder(mailbox.id, inbox, 10, 0)
            .await
            .unwrap()
            .len(),
        1
    );

    let junk = post(
        api.addr,
        &format!("/api/v1/messages/{message}/folder"),
        &[
            ("Content-Type", "application/json"),
            ("X-Nexus-Subject", &subject),
        ],
        br#"{"folder":"junk"}"#,
    )
    .await;
    assert!(junk.starts_with("HTTP/1.1 204"), "{junk}");
    let junk_id = api
        .store
        .folder_named_or_create(mailbox.id, "Junk", nexus_mailstore::FolderKind::Custom)
        .await
        .unwrap();
    assert_eq!(
        api.store
            .list_folder(mailbox.id, junk_id, 10, 0)
            .await
            .unwrap()
            .len(),
        1
    );
    let message_response = get(
        api.addr,
        &format!("/api/v1/messages/{message}"),
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(
        message_response.contains(r#""folder_kind":"custom""#),
        "{message_response}"
    );
    assert!(
        message_response.contains(r#""folder_name":"Junk""#),
        "{message_response}"
    );

    let not_spam = post(
        api.addr,
        &format!("/api/v1/messages/{message}/folder"),
        &[
            ("Content-Type", "application/json"),
            ("X-Nexus-Subject", &subject),
        ],
        br#"{"folder":"inbox"}"#,
    )
    .await;
    assert!(not_spam.starts_with("HTTP/1.1 204"), "{not_spam}");
    assert_eq!(
        api.store
            .list_folder(mailbox.id, inbox, 10, 0)
            .await
            .unwrap()
            .len(),
        1
    );
}

#[tokio::test]
async fn permanent_delete_and_empty_trash_are_idempotent_and_mailbox_scoped() {
    let api = start(None).await;
    let owner_subject = format!("usr-{}", Uuid::now_v7().simple());
    let owner = api
        .store
        .create_identity_mailbox(&owner_subject, "Owner")
        .await
        .unwrap();
    let owner_trash = api
        .store
        .folder(owner.id, nexus_mailstore::FolderKind::Trash)
        .await
        .unwrap();

    let other_subject = format!("usr-{}", Uuid::now_v7().simple());
    let other = api
        .store
        .create_identity_mailbox(&other_subject, "Other")
        .await
        .unwrap();
    let other_inbox = api
        .store
        .folder(other.id, nexus_mailstore::FolderKind::Inbox)
        .await
        .unwrap();
    let other_trash = api
        .store
        .folder(other.id, nexus_mailstore::FolderKind::Trash)
        .await
        .unwrap();

    let from = Address::parse(&format!("sender@{}", api.local)).unwrap();
    let thread = api
        .store
        .thread_for(None, &[], Some("Shared"))
        .await
        .unwrap();
    let shared_submission = Uuid::now_v7();
    let mut tx = api.store.begin_reference_transaction().await.unwrap();
    let shared = api
        .store
        .store_message_in_transaction(
            &mut tx,
            b"From: sender@example.test\r\nSubject: Shared\r\n\r\nbody",
            thread,
            &from,
            Some("Shared"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
            Some(shared_submission),
        )
        .await
        .unwrap();
    api.store
        .deliver_in_transaction(&mut tx, owner.id, shared, owner_trash)
        .await
        .unwrap();
    api.store
        .deliver_in_transaction(&mut tx, other.id, shared, other_inbox)
        .await
        .unwrap();
    MailStore::release_reference_in_transaction(
        &mut tx,
        shared,
        MessageReferenceKind::Submission,
        shared_submission,
    )
    .await
    .unwrap();
    tx.commit().await.unwrap();

    let owner_only = delete(
        api.addr,
        &format!("/api/v1/messages/{shared}"),
        &[("X-Nexus-Subject", &owner_subject)],
    )
    .await;
    assert!(owner_only.starts_with("HTTP/1.1 204"), "{owner_only}");
    let owner_only_repeat = delete(
        api.addr,
        &format!("/api/v1/messages/{shared}"),
        &[("X-Nexus-Subject", &owner_subject)],
    )
    .await;
    assert!(
        owner_only_repeat.starts_with("HTTP/1.1 204"),
        "{owner_only_repeat}"
    );
    assert_eq!(
        api.store
            .list_folder(owner.id, owner_trash, 10, 0)
            .await
            .unwrap()
            .len(),
        0
    );
    assert_eq!(
        api.store
            .list_folder(other.id, other_inbox, 10, 0)
            .await
            .unwrap()
            .len(),
        1
    );

    let owner_only_other = delete(
        api.addr,
        &format!("/api/v1/messages/{shared}"),
        &[("X-Nexus-Subject", &other_subject)],
    )
    .await;
    assert!(
        owner_only_other.starts_with("HTTP/1.1 204"),
        "{owner_only_other}"
    );
    let (remaining_shared_rows,): (i64,) =
        sqlx::query_as("SELECT COUNT(*) FROM messages WHERE id = $1")
            .bind(shared)
            .fetch_one(api.store.pool())
            .await
            .unwrap();
    assert_eq!(remaining_shared_rows, 0);

    let lonely_thread = api
        .store
        .thread_for(None, &[], Some("Lonely"))
        .await
        .unwrap();
    let lonely_submission = Uuid::now_v7();
    let mut tx = api.store.begin_reference_transaction().await.unwrap();
    let lonely = api
        .store
        .store_message_in_transaction(
            &mut tx,
            b"From: sender@example.test\r\nSubject: Lonely\r\n\r\nbody",
            lonely_thread,
            &from,
            Some("Lonely"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
            Some(lonely_submission),
        )
        .await
        .unwrap();
    api.store
        .deliver_in_transaction(&mut tx, owner.id, lonely, owner_trash)
        .await
        .unwrap();
    api.store
        .deliver_in_transaction(&mut tx, other.id, lonely, other_trash)
        .await
        .unwrap();
    MailStore::release_reference_in_transaction(
        &mut tx,
        lonely,
        MessageReferenceKind::Submission,
        lonely_submission,
    )
    .await
    .unwrap();
    tx.commit().await.unwrap();

    let empty = post(
        api.addr,
        "/api/v1/folders/trash/empty",
        &[("X-Nexus-Subject", &owner_subject)],
        b"",
    )
    .await;
    assert!(empty.starts_with("HTTP/1.1 204"), "{empty}");
    let empty_repeat = post(
        api.addr,
        "/api/v1/folders/trash/empty",
        &[("X-Nexus-Subject", &owner_subject)],
        b"",
    )
    .await;
    assert!(empty_repeat.starts_with("HTTP/1.1 204"), "{empty_repeat}");
    assert_eq!(
        api.store
            .list_folder(owner.id, owner_trash, 10, 0)
            .await
            .unwrap()
            .len(),
        0
    );
    assert_eq!(
        api.store
            .list_folder(other.id, other_trash, 10, 0)
            .await
            .unwrap()
            .len(),
        1
    );
}

#[tokio::test]
async fn search_defaults_exclude_junk_and_trash_and_filters_validate() {
    let api = start(None).await;
    let subject = format!("usr-{}", Uuid::now_v7().simple());
    let mailbox = api
        .store
        .create_identity_mailbox(&subject, "Searcher")
        .await
        .unwrap();
    let inbox = api
        .store
        .folder(mailbox.id, nexus_mailstore::FolderKind::Inbox)
        .await
        .unwrap();
    let trash = api
        .store
        .folder(mailbox.id, nexus_mailstore::FolderKind::Trash)
        .await
        .unwrap();
    let sent = api
        .store
        .folder(mailbox.id, nexus_mailstore::FolderKind::Sent)
        .await
        .unwrap();
    let junk = api
        .store
        .folder_named_or_create(mailbox.id, "Junk", nexus_mailstore::FolderKind::Custom)
        .await
        .unwrap();
    let from = Address::parse(&format!("sender@{}", api.local)).unwrap();
    let thread = api
        .store
        .thread_for(None, &[], Some("Needle"))
        .await
        .unwrap();

    let in_inbox = api
        .store
        .store_message(
            b"From: sender@example.test\r\nSubject: Needle inbox\r\n\r\nNeedle inbox body",
            thread,
            &from,
            Some("Needle inbox"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let in_sent = api
        .store
        .store_message(
            b"From: sender@example.test\r\nSubject: Needle sent\r\n\r\nNeedle sent body",
            thread,
            &from,
            Some("Needle sent"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let in_trash = api
        .store
        .store_message(
            b"From: sender@example.test\r\nSubject: Needle trash\r\n\r\nNeedle trash body",
            thread,
            &from,
            Some("Needle trash"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
        )
        .await
        .unwrap();
    let in_junk = api
        .store
        .store_message(
            b"From: sender@example.test\r\nSubject: Needle junk\r\n\r\nNeedle junk body",
            thread,
            &from,
            Some("Needle junk"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
        )
        .await
        .unwrap();
    api.store
        .deliver(mailbox.id, in_inbox, inbox)
        .await
        .unwrap();
    api.store.deliver(mailbox.id, in_sent, sent).await.unwrap();
    api.store
        .deliver(mailbox.id, in_trash, trash)
        .await
        .unwrap();
    api.store.deliver(mailbox.id, in_junk, junk).await.unwrap();
    api.store.set_seen(mailbox.id, in_sent, true).await.unwrap();

    let default = get(
        api.addr,
        "/api/v1/search?q=Needle",
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(default.starts_with("HTTP/1.1 200"), "{default}");
    assert!(default.contains("Needle inbox"), "{default}");
    assert!(default.contains("Needle sent"), "{default}");
    assert!(!default.contains("Needle trash"), "{default}");
    assert!(!default.contains("Needle junk"), "{default}");

    let trash_only = get(
        api.addr,
        "/api/v1/search?q=Needle&folder=trash",
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(trash_only.starts_with("HTTP/1.1 200"), "{trash_only}");
    assert!(trash_only.contains("Needle trash"), "{trash_only}");
    assert!(!trash_only.contains("Needle inbox"), "{trash_only}");

    let unread = get(
        api.addr,
        "/api/v1/search?q=Needle&unread=true",
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(unread.starts_with("HTTP/1.1 200"), "{unread}");
    assert!(unread.contains("Needle inbox"), "{unread}");
    assert!(!unread.contains("Needle sent"), "{unread}");

    let empty_q = get(
        api.addr,
        "/api/v1/search?q=%20%20",
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(empty_q.starts_with("HTTP/1.1 400"), "{empty_q}");

    let too_long_q = get(
        api.addr,
        &format!("/api/v1/search?q={}", "x".repeat(201)),
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(too_long_q.starts_with("HTTP/1.1 400"), "{too_long_q}");

    let invalid_folder = get(
        api.addr,
        "/api/v1/search?q=Needle&folder=spam",
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(
        invalid_folder.starts_with("HTTP/1.1 400"),
        "{invalid_folder}"
    );

    let crowded_inbox = api
        .store
        .store_message(
            b"From: sender@example.test\r\nSubject: FloodNeedle inbox keeper\r\n\r\nFloodNeedle",
            thread,
            &from,
            Some("FloodNeedle inbox keeper"),
            None,
            None,
            &[],
            nexus_mailstore::Transport::Internal,
            None,
        )
        .await
        .unwrap();
    api.store
        .deliver(mailbox.id, crowded_inbox, inbox)
        .await
        .unwrap();
    for i in 0..220 {
        let noisy = api
            .store
            .store_message(
                format!(
                    "From: sender@example.test\r\nSubject: FloodNeedle trash {i}\r\n\r\nFloodNeedle"
                )
                .as_bytes(),
                thread,
                &from,
                Some(&format!("FloodNeedle trash {i}")),
                None,
                None,
                &[],
                nexus_mailstore::Transport::Internal,
                None,
            )
            .await
            .unwrap();
        api.store.deliver(mailbox.id, noisy, trash).await.unwrap();
    }
    let crowded_default = get(
        api.addr,
        "/api/v1/search?q=FloodNeedle",
        &[("X-Nexus-Subject", &subject)],
    )
    .await;
    assert!(
        crowded_default.starts_with("HTTP/1.1 200"),
        "{crowded_default}"
    );
    assert!(
        crowded_default.contains("FloodNeedle inbox keeper"),
        "{crowded_default}"
    );
    assert!(
        !crowded_default.contains("FloodNeedle trash 219"),
        "{crowded_default}"
    );
}
