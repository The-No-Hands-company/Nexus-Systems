//! Zero-retention: mail accepted over a real socket must not record who
//! connected. The client's address is used for SPF at accept time and then
//! forgotten; it must not reach the stored message (no `Received:` line, no
//! address in an Authentication-Results header). Needs Postgres.
//!
//! The submission listener only differs from this one by requiring AUTH before
//! MAIL; both hand the message to the same `AuthenticatingSink`, which is the
//! code that decides what bytes are stored, so that is what is driven here.

use std::sync::Arc;

use nexus_mailauth::{DnsError, Lookup};
use nexus_maildelivery::{Deliverer, Queue, Router};
use nexus_mailsmtp::{
    AuthenticatingSink, Inbound, Limits, PolicyMode, RelayPolicy, Role, Sink, SmtpServer,
};
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

struct QuarantineDns;
impl Lookup for QuarantineDns {
    async fn txt(&self, name: &str) -> Result<Vec<String>, DnsError> {
        match name.trim_end_matches('.') {
            "bank.test" => Ok(vec!["v=spf1 ip4:203.0.113.1 -all".into()]),
            "_dmarc.bank.test" => Ok(vec!["v=DMARC1; p=quarantine".into()]),
            _ => Err(DnsError::NotFound),
        }
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

#[tokio::test]
async fn smtp_envelope_rolls_back_every_recipient_on_later_membership_failure() {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL").unwrap();
    let pool = PgPoolOptions::new()
        .max_connections(4)
        .connect(&url)
        .await
        .unwrap();
    let store = MailStore::new(pool.clone());
    let local = format!("t{}.test", Uuid::now_v7().simple());
    let router = Router::new().with_local_domain(&local);
    let deliverer = Arc::new(Deliverer::new(store.clone(), Queue::new(pool), router));
    let first = store.create_node_mailbox("first").await.unwrap();
    let second = store.create_node_mailbox("second").await.unwrap();
    let recipients = vec![format!("first@{local}"), format!("second@{local}")];
    store
        .add_address(first.id, &Address::parse(&recipients[0]).unwrap(), true)
        .await
        .unwrap();
    store
        .add_address(second.id, &Address::parse(&recipients[1]).unwrap(), true)
        .await
        .unwrap();
    let suffix = Uuid::now_v7().simple().to_string();
    let function = format!("smtp_fail_{suffix}");
    sqlx::raw_sql(&format!(
        "CREATE FUNCTION {function}() RETURNS trigger LANGUAGE plpgsql AS $$
             BEGIN RAISE EXCEPTION 'injected membership failure'; END $$;
             CREATE TRIGGER {function} BEFORE INSERT ON mailbox_messages
             FOR EACH ROW WHEN (NEW.mailbox_id = '{}') EXECUTE FUNCTION {function}();",
        second.id,
    ))
    .execute(store.pool())
    .await
    .unwrap();
    let sink = AuthenticatingSink {
        store: store.clone(),
        deliverer,
        dns: Arc::new(NoDns),
        receiving_host: format!("mail.{local}"),
        mode: PolicyMode::Observe,
    };
    let result = sink
        .deliver(Inbound {
            from: "sender@example.test",
            recipients: &recipients,
            data: b"From: sender@example.test\r\nSubject: smtp atomic\r\n\r\nbody\r\n",
            client_ip: "127.0.0.1".parse().unwrap(),
            helo: "client.test",
        })
        .await;
    sqlx::raw_sql(&format!(
        "DROP TRIGGER {function} ON mailbox_messages; DROP FUNCTION {function}();"
    ))
    .execute(store.pool())
    .await
    .unwrap();
    assert!(result.is_err());
    let (memberships,): (i64,) =
        sqlx::query_as("SELECT count(*) FROM mailbox_messages WHERE mailbox_id = ANY($1)")
            .bind(vec![first.id, second.id])
            .fetch_one(store.pool())
            .await
            .unwrap();
    assert_eq!(
        memberships, 0,
        "a failed envelope must not deliver to an earlier recipient"
    );
}

#[tokio::test]
async fn smtp_quarantine_places_every_envelope_recipient_in_junk() {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL").unwrap();
    let pool = PgPoolOptions::new()
        .max_connections(4)
        .connect(&url)
        .await
        .unwrap();
    let store = MailStore::new(pool.clone());
    let local = format!("t{}.test", Uuid::now_v7().simple());
    let router = Router::new().with_local_domain(&local);
    let deliverer = Arc::new(Deliverer::new(store.clone(), Queue::new(pool), router));
    let first = store.create_node_mailbox("first").await.unwrap();
    let second = store.create_node_mailbox("second").await.unwrap();
    let recipients = vec![format!("first@{local}"), format!("second@{local}")];
    store
        .add_address(first.id, &Address::parse(&recipients[0]).unwrap(), true)
        .await
        .unwrap();
    store
        .add_address(second.id, &Address::parse(&recipients[1]).unwrap(), true)
        .await
        .unwrap();
    let sink = AuthenticatingSink {
        store: store.clone(),
        deliverer,
        dns: Arc::new(QuarantineDns),
        receiving_host: format!("mail.{local}"),
        mode: PolicyMode::Enforce,
    };
    let result = sink
        .deliver(Inbound {
            from: "attacker@evil.test",
            recipients: &recipients,
            data: b"From: security@bank.test\r\nSubject: spoof\r\n\r\nbody\r\n",
            client_ip: "198.51.100.66".parse().unwrap(),
            helo: "evil.test",
        })
        .await;
    assert!(matches!(result, Ok(None)));
    let memberships: Vec<(Uuid, String, Uuid)> = sqlx::query_as(
        "SELECT mm.mailbox_id, f.name, mm.message_id FROM mailbox_messages mm
             JOIN folders f ON f.id = mm.folder_id WHERE mm.mailbox_id = ANY($1)
             ORDER BY mm.mailbox_id",
    )
    .bind(vec![first.id, second.id])
    .fetch_all(store.pool())
    .await
    .unwrap();
    assert_eq!(memberships.len(), 2);
    assert!(memberships.iter().all(|(_, name, _)| name == "Junk"));
    assert_eq!(memberships[0].2, memberships[1].2);
}

#[tokio::test]
async fn smtp_quarantine_moves_an_existing_identical_inbox_copy_to_junk() {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL").unwrap();
    let pool = PgPoolOptions::new()
        .max_connections(4)
        .connect(&url)
        .await
        .unwrap();
    let store = MailStore::new(pool.clone());
    let local = format!("t{}.test", Uuid::now_v7().simple());
    let deliverer = Arc::new(Deliverer::new(
        store.clone(),
        Queue::new(pool),
        Router::new().with_local_domain(&local),
    ));
    let mailbox = store
        .create_node_mailbox("existing quarantine")
        .await
        .unwrap();
    let recipients = vec![format!("user@{local}")];
    store
        .add_address(mailbox.id, &Address::parse(&recipients[0]).unwrap(), true)
        .await
        .unwrap();
    let inbound = Inbound {
        from: "attacker@evil.test",
        recipients: &recipients,
        data: b"From: security@bank.test\r\nSubject: identical spoof\r\n\r\nbody\r\n",
        client_ip: "198.51.100.66".parse().unwrap(),
        helo: "evil.test",
    };
    for mode in [PolicyMode::Observe, PolicyMode::Enforce] {
        let sink = AuthenticatingSink {
            store: store.clone(),
            deliverer: deliverer.clone(),
            dns: Arc::new(QuarantineDns),
            receiving_host: format!("mail.{local}"),
            mode,
        };
        assert!(matches!(sink.deliver(inbound.clone()).await, Ok(None)));
    }
    let folders: Vec<(String,)> = sqlx::query_as(
        "SELECT f.name FROM mailbox_messages mm JOIN folders f ON f.id = mm.folder_id
         WHERE mm.mailbox_id = $1",
    )
    .bind(mailbox.id)
    .fetch_all(store.pool())
    .await
    .unwrap();
    assert_eq!(folders, vec![("Junk".to_string(),)]);
}
