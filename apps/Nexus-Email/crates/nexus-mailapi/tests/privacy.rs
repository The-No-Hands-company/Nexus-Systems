//! Zero-retention at the API: what a signed-in user's send puts into the
//! message, and what the API writes to its log. Needs Postgres.

use std::cell::RefCell;
use std::sync::{Arc, Once};

use nexus_mailapi::{router, AppState};
use nexus_maildelivery::{Deliverer, Queue, Router};
use nexus_mailstore::{Address, MailStore};
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
            let sub = tracing_subscriber::fmt().with_writer(Sink).with_max_level(tracing::Level::TRACE).finish();
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
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    let store = MailStore::new(pool.clone());
    let local = format!("t{}.test", Uuid::now_v7().simple());
    let deliverer = Deliverer::new(store.clone(), Queue::new(pool), Router::new().with_local_domain(&local));
    let state = Arc::new(AppState {
        store: store.clone(),
        deliverer,
        primary_domain: local.clone(),
        cloudflare_ingress_token: token.map(str::to_string),
    });
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    tokio::spawn(async move {
        let _ = axum::serve(listener, router(state)).await;
    });
    Api { addr, store, local }
}

/// A bare HTTP/1.1 POST; returns the status line and body.
async fn post(addr: std::net::SocketAddr, path: &str, headers: &[(&str, &str)], body: &[u8]) -> String {
    let mut s = TcpStream::connect(addr).await.unwrap();
    let mut req = format!("POST {path} HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: {}\r\n", body.len());
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

#[tokio::test]
async fn a_message_sent_through_the_api_carries_no_client_address_or_received_line() {
    let api = start(None).await;
    let subject = format!("usr-{}", Uuid::now_v7().simple());
    let mb = api.store.create_identity_mailbox(&subject, "Sender").await.unwrap();
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
        assert!(!l.starts_with("x-originating-ip"), "X-Originating-IP line: {line}");
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
    assert!(log.contains("Cloudflare inbound delivery"), "failure must still be logged: {log:?}");
    assert!(!log.contains(&rcpt), "recipient leaked: {log:?}");
    assert!(!log.contains(&api.local), "recipient domain leaked: {log:?}");
    assert!(!log.contains("stranger@sender.example"), "sender leaked: {log:?}");
}
