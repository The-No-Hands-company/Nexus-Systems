//! Zero-retention: mail accepted over a real socket must not record who
//! connected. The client's address is used for SPF at accept time and then
//! forgotten; it must not reach the stored message (no `Received:` line, no
//! address in an Authentication-Results header). Needs Postgres.
//!
//! The submission listener only differs from this one by requiring AUTH before
//! MAIL; both hand the message to the same `AuthenticatingSink`, which is the
//! code that decides what bytes are stored, so that is what is driven here.

use std::sync::Arc;

use nexus_maildelivery::{Deliverer, Queue, Router};
use nexus_mailauth::{DnsError, Lookup};
use nexus_mailsmtp::{AuthenticatingSink, Limits, PolicyMode, RelayPolicy, Role, SmtpServer};
use nexus_mailstore::{Address, MailStore};
use sqlx::postgres::PgPoolOptions;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpStream};
use uuid::Uuid;

/// Every name is absent; SPF resolves to "none", which is all this needs.
struct NoDns;
impl Lookup for NoDns {
    async fn txt(&self, _: &str) -> Result<Vec<String>, DnsError> {
        Err(DnsError::NotFound)
    }
    async fn a(&self, _: &str) -> Result<Vec<std::net::IpAddr>, DnsError> {
        Err(DnsError::NotFound)
    }
    async fn mx(&self, _: &str) -> Result<Vec<String>, DnsError> {
        Err(DnsError::NotFound)
    }
}

#[derive(Clone)]
struct AllLocal;
impl RelayPolicy for AllLocal {
    async fn is_local(&self, _: &str) -> bool {
        true
    }
}

async fn reply(r: &mut BufReader<tokio::net::tcp::OwnedReadHalf>) -> String {
    let mut out = String::new();
    loop {
        let mut line = String::new();
        r.read_line(&mut line).await.unwrap();
        let last = line.as_bytes().get(3) != Some(&b'-');
        out.push_str(&line);
        if last || line.is_empty() {
            return out;
        }
    }
}

#[tokio::test]
async fn a_message_accepted_over_the_wire_stores_no_client_address_and_no_received_line() {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    let store = MailStore::new(pool.clone());
    let local = format!("t{}.test", Uuid::now_v7().simple());
    let router = Router::new().with_local_domain(&local);
    let deliverer = Arc::new(Deliverer::new(store.clone(), Queue::new(pool), router));

    let mailbox = store
        .create_identity_mailbox(&format!("u{}", Uuid::now_v7().simple()), "Bob")
        .await
        .unwrap();
    let bob = Address::parse(&format!("bob@{local}")).unwrap();
    store.add_address(mailbox.id, &bob, true).await.unwrap();

    let sink = Arc::new(AuthenticatingSink {
        store: store.clone(),
        deliverer,
        dns: Arc::new(NoDns),
        receiving_host: format!("mail.{local}"),
        mode: PolicyMode::Observe,
    });
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    let server = Arc::new(SmtpServer {
        hostname: format!("mail.{local}"),
        role: Role::Mx,
        limits: Limits::default(),
        policy: Arc::new(AllLocal),
        sink,
    });
    tokio::spawn(async move {
        let _ = server.serve(listener).await;
    });

    // The connection really does come from 127.0.0.1.
    let (r, mut w) = TcpStream::connect(addr).await.unwrap().into_split();
    let mut r = BufReader::new(r);
    reply(&mut r).await;
    for (line, want) in [
        ("EHLO client.test", "250"),
        ("MAIL FROM:<sender@example.test>", "250"),
        (&format!("RCPT TO:<{}>", bob.as_string()), "250"),
        ("DATA", "354"),
    ] {
        w.write_all(format!("{line}\r\n").as_bytes()).await.unwrap();
        let got = reply(&mut r).await;
        assert!(got.starts_with(want), "{line}: {got}");
    }
    w.write_all(b"From: sender@example.test\r\nSubject: hi\r\n\r\nhello\r\n.\r\n").await.unwrap();
    assert!(reply(&mut r).await.starts_with("250"));
    w.write_all(b"QUIT\r\n").await.unwrap();

    let rows: Vec<(Option<Vec<u8>>,)> = sqlx::query_as(
        "SELECT m.body_inline FROM messages m
           JOIN mailbox_messages mm ON mm.message_id = m.id
          WHERE mm.mailbox_id = $1",
    )
    .bind(mailbox.id)
    .fetch_all(store.pool())
    .await
    .unwrap();
    assert_eq!(rows.len(), 1);
    let stored = String::from_utf8_lossy(rows[0].0.as_ref().unwrap()).into_owned();

    assert!(stored.contains("hello"), "the message itself must be stored: {stored:?}");
    assert!(!stored.contains("127.0.0.1"), "client address stored: {stored:?}");
    assert!(!stored.contains("::1"), "client address stored: {stored:?}");
    assert!(
        !stored.lines().any(|l| l.to_ascii_lowercase().starts_with("received:")),
        "a Received line was stored: {stored:?}"
    );
}
