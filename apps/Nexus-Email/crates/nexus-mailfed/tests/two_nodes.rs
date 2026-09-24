//! Federated egress end to end, with two real nodes.
//!
//! Node A cannot reach anyone's MX. It hands outside mail to node B, which
//! relays it from its own address to a fake MX. Each node has its own freshly
//! migrated database: sharing one would let each worker claim the other's
//! queue rows, which is not what two nodes are.
//!
//! Needs NEXUS_EMAIL_TEST_DATABASE_URL pointing at a role that may CREATE
//! DATABASE; the two databases are dropped afterwards.

use std::sync::{Arc, Mutex};
use std::time::{SystemTime, UNIX_EPOCH};

use nexus_mailauth::{generate, public_key_record, verify_with_key, Canon, DkimSigner};
use nexus_maildelivery::{Deliverer, Disposition, Queue, Router};
use nexus_mailfed::client::HttpTransport;
use nexus_mailfed::ingest::{self, IngestState};
use nexus_mailfed::{NodeKey, Peer, PeerDirectory};
use nexus_mailout::{DeliveryWorker, Egress, WorkerConfig};
use nexus_mailstore::{Address, MailStore};
use sqlx::postgres::PgPoolOptions;
use sqlx::{Executor, PgPool};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpListener;
use uuid::Uuid;

fn now() -> i64 {
    SystemTime::now().duration_since(UNIX_EPOCH).unwrap().as_secs() as i64
}

/// A fresh database with every mailstore migration applied.
struct ScratchDb {
    admin: PgPool,
    name: String,
    pool: PgPool,
}

impl ScratchDb {
    async fn create(label: &str) -> Self {
        let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
            .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
        let admin = PgPoolOptions::new().max_connections(1).connect(&url).await.unwrap();
        Self::sweep(&admin).await;
        let name = format!("mail_e2e_{label}_{}", Uuid::now_v7().simple());
        admin.execute(format!("CREATE DATABASE {name}").as_str()).await.unwrap();

        let (base, _) = url.rsplit_once('/').unwrap();
        let pool = PgPoolOptions::new().max_connections(4).connect(&format!("{base}/{name}")).await.unwrap();

        let dir = concat!(env!("CARGO_MANIFEST_DIR"), "/../nexus-mailstore/migrations");
        let mut files: Vec<_> = std::fs::read_dir(dir).unwrap().map(|e| e.unwrap().path()).collect();
        files.sort();
        for f in files {
            pool.execute(std::fs::read_to_string(&f).unwrap().as_str()).await.unwrap();
        }
        Self { admin, name, pool }
    }

    /// A failed run panics before `drop`, leaving its databases behind. Clear
    /// any that nothing is connected to; a concurrent run's are in use and are
    /// left alone.
    async fn sweep(admin: &PgPool) {
        let stale: Vec<(String,)> = sqlx::query_as(
            "SELECT d.datname FROM pg_database d
              WHERE d.datname LIKE 'mail\\_e2e\\_%'
                AND NOT EXISTS (SELECT 1 FROM pg_stat_activity a WHERE a.datname = d.datname)",
        )
        .fetch_all(admin)
        .await
        .unwrap();
        for (name,) in stale {
            let _ = admin.execute(format!("DROP DATABASE IF EXISTS {name}").as_str()).await;
        }
    }

    async fn drop(self) {
        self.pool.close().await;
        self.admin.execute(format!("DROP DATABASE {}", self.name).as_str()).await.unwrap();
    }
}

/// Just enough SMTP server to accept one message and keep it.
async fn fake_mx() -> (u16, Arc<Mutex<Vec<Vec<u8>>>>) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let port = listener.local_addr().unwrap().port();
    let received = Arc::new(Mutex::new(Vec::new()));
    let store = Arc::clone(&received);
    tokio::spawn(async move {
        loop {
            let (sock, _) = listener.accept().await.unwrap();
            let store = Arc::clone(&store);
            tokio::spawn(async move {
                let (r, mut w) = sock.into_split();
                let mut r = BufReader::new(r);
                w.write_all(b"220 fake-mx ready\r\n").await.unwrap();
                let mut line = String::new();
                loop {
                    line.clear();
                    if r.read_line(&mut line).await.unwrap() == 0 {
                        return;
                    }
                    let verb = line.get(..4).unwrap_or("").to_ascii_uppercase();
                    match verb.as_str() {
                        "EHLO" | "HELO" => w.write_all(b"250 fake-mx\r\n").await.unwrap(),
                        "MAIL" | "RCPT" => w.write_all(b"250 ok\r\n").await.unwrap(),
                        "DATA" => {
                            w.write_all(b"354 go ahead\r\n").await.unwrap();
                            // Read to the lone dot, undoing dot-stuffing, and
                            // keep the message exactly as a real MX would.
                            let mut msg = Vec::new();
                            loop {
                                let mut l = Vec::new();
                                r.read_until(b'\n', &mut l).await.unwrap();
                                if l == b".\r\n" {
                                    break;
                                }
                                let l = if l.starts_with(b"..") { &l[1..] } else { &l[..] };
                                msg.extend_from_slice(l);
                            }
                            store.lock().unwrap().push(msg);
                            w.write_all(b"250 queued\r\n").await.unwrap();
                        }
                        "QUIT" => {
                            let _ = w.write_all(b"221 bye\r\n").await;
                            return;
                        }
                        _ => w.write_all(b"502 no\r\n").await.unwrap(),
                    }
                }
            });
        }
    });
    (port, received)
}

async fn queue_states(pool: &PgPool) -> Vec<(String, String, String)> {
    sqlx::query_as("SELECT recipient, route, state FROM outbound_queue ORDER BY created_at")
        .fetch_all(pool)
        .await
        .unwrap()
}

#[tokio::test]
async fn outside_mail_leaves_through_a_peer_with_the_origins_dkim_signature_intact() {
    let db_a = ScratchDb::create("a").await;
    let db_b = ScratchDb::create("b").await;
    let (a_domain, b_domain) = ("a.test", "b.test");
    let key_a = Arc::new(NodeKey::generate());
    let key_b = Arc::new(NodeKey::generate());

    // --- Node B: the egress peer. Its own port 25 works (the fake MX).
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let b_addr = listener.local_addr().unwrap();
    let store_b = MailStore::new(db_b.pool.clone());
    let peers_b = PeerDirectory::new(db_b.pool.clone());
    peers_b
        .add(&Peer {
            domain: a_domain.into(),
            base_url: "https://mail.a.test".into(),
            public_key: key_a.public_b64(),
            may_relay: true,
        })
        .await
        .unwrap();
    let deliverer_b = Arc::new(Deliverer::new(
        store_b.clone(),
        Queue::new(db_b.pool.clone()),
        Router::new().with_local_domain(b_domain),
    ));
    let ingest_b = ingest::router(Arc::new(IngestState {
        local_domain: b_domain.into(),
        public_host: b_addr.to_string(),
        key: Arc::clone(&key_b),
        peers: peers_b,
        deliverer: deliverer_b,
        clock: Arc::new(now),
    }));
    tokio::spawn(async move { axum::serve(listener, ingest_b).await.unwrap() });
    let (mx_port, received) = fake_mx().await;
    let worker_b = DeliveryWorker::new(
        store_b,
        Queue::new(db_b.pool.clone()),
        WorkerConfig {
            ehlo_name: "mail.b.test".into(),
            port: mx_port,
            smtp_host_override: Some("127.0.0.1".into()),
            ..WorkerConfig::default()
        },
    );

    // --- Node A: port 25 filtered; egress through B, DKIM-signing as a.test.
    let store_a = MailStore::new(db_a.pool.clone());
    let peers_a = PeerDirectory::new(db_a.pool.clone());
    peers_a
        .add(&Peer {
            domain: b_domain.into(),
            base_url: format!("http://{b_addr}"),
            public_key: key_b.public_b64(),
            may_relay: false,
        })
        .await
        .unwrap();
    let deliverer_a = Deliverer::new(
        store_a.clone(),
        Queue::new(db_a.pool.clone()),
        Router::new().with_local_domain(a_domain),
    );
    let dkim_key = generate(2048).unwrap();
    let worker_a = DeliveryWorker::new(
        store_a.clone(),
        Queue::new(db_a.pool.clone()),
        WorkerConfig { egress: Egress::Peer(b_domain.into()), ..WorkerConfig::default() },
    )
    .with_transport(Arc::new(HttpTransport::new(a_domain.into(), key_a, peers_a, Arc::new(now))))
    .with_dkim(Arc::new(DkimSigner {
        domain: a_domain.into(),
        selector: "nexus".into(),
        key: dkim_key.clone(),
        header_canon: Canon::Relaxed,
        body_canon: Canon::Relaxed,
    }));

    // --- Alice on A writes to someone on the internet.
    let alice_mb = store_a.create_node_mailbox("Alice").await.unwrap();
    let alice = Address::parse("alice@a.test").unwrap();
    store_a.add_address(alice_mb.id, &alice, true).await.unwrap();
    let rcpt = Address::parse("someone@outside.example").unwrap();
    // Hand-written rather than built, so it has what real submitted mail has:
    // a final CRLF, and a body line starting with a dot that SMTP must stuff
    // on the way out and the receiver must unstuff.
    let raw = format!(
        "From: {}\r\nTo: {}\r\nSubject: Hello from a node with no port 25\r\n\
         Message-ID: <e2e@a.test>\r\nMIME-Version: 1.0\r\n\
         Content-Type: text/plain; charset=utf-8\r\n\r\n\
         Line one.\r\n.A line that starts with a dot.\r\nLast line.\r\n",
        alice.as_string(),
        rcpt.as_string()
    )
    .into_bytes();
    let outcomes = deliverer_a.submit(&raw, &alice, &[rcpt.clone()], Some(alice_mb.id)).await.unwrap();
    assert_eq!(outcomes[0].disposition, Disposition::Queued);

    // A hands it to B; B relays it to the world.
    assert_eq!(worker_a.tick().await.unwrap(), 1);
    assert_eq!(
        queue_states(&db_a.pool).await,
        vec![(rcpt.as_string(), "smtp".into(), "delivered".into())],
        "A's delivery is done once B has accepted it"
    );
    assert_eq!(
        queue_states(&db_b.pool).await,
        vec![(rcpt.as_string(), "smtp".into(), "pending".into())],
        "B queued it for SMTP"
    );
    assert_eq!(worker_b.tick().await.unwrap(), 1);
    assert_eq!(queue_states(&db_b.pool).await[0].2, "delivered");

    // What the world received is A's message, signed by A, byte for byte.
    let got = received.lock().unwrap().clone();
    assert_eq!(got.len(), 1);
    let delivered = &got[0];
    assert!(delivered.starts_with(b"DKIM-Signature:"), "{}", String::from_utf8_lossy(delivered));
    // Exactly A's bytes, with exactly one header added in front of them.
    assert!(
        delivered.ends_with(&raw),
        "the message must arrive unaltered after the signature:\n{}",
        String::from_utf8_lossy(delivered)
    );
    let added = String::from_utf8_lossy(&delivered[..delivered.len() - raw.len()]).to_string();
    let mut lines = added.split_terminator("\r\n");
    assert!(lines.next().unwrap().starts_with("DKIM-Signature:"));
    assert!(
        lines.all(|l| l.starts_with(' ') || l.starts_with('\t')),
        "only the signature header, folded, may precede A's message: {added:?}"
    );
    let sig = verify_with_key(delivered, &public_key_record(&dkim_key).unwrap())
        .expect("the origin's signature must survive the relay");
    assert_eq!(sig.domain, "a.test");

    db_a.drop().await;
    db_b.drop().await;
}
