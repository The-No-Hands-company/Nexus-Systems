//! One real conversation over a real socket, to prove the listener and the
//! state machine agree. The rules themselves are covered exhaustively in
//! session.rs, which needs no sockets.

use std::sync::{Arc, Mutex};

use nexus_mailsmtp::{Inbound, Limits, RelayPolicy, Role, Sink, SmtpServer};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpStream};

#[derive(Clone)]
struct Local;
impl RelayPolicy for Local {
    fn is_local(&self, address: &str) -> bool {
        address.ends_with("@tnhc.dev")
    }
}

#[derive(Default)]
struct Collector {
    received: Mutex<Vec<(String, Vec<String>, Vec<u8>)>>,
    fail: bool,
}

impl Sink for Collector {
    async fn deliver(&self, msg: Inbound<'_>) -> Result<Option<String>, String> {
        if self.fail {
            return Err("storage down".into());
        }
        // The client IP is what SPF is evaluated against, so a sink that never
        // receives it could not authenticate anything.
        assert!(!msg.client_ip.is_unspecified(), "the sink must be told who connected");
        self.received.lock().unwrap().push((
            msg.from.to_string(),
            msg.recipients.to_vec(),
            msg.data.to_vec(),
        ));
        Ok(None)
    }
}

async fn start(collector: Arc<Collector>) -> (String, Arc<Collector>) {
    // The server takes Arc<S>, so the test keeps its own clone to inspect what
    // arrived.
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap().to_string();
    let server = Arc::new(SmtpServer {
        hostname: "mail.tnhc.dev".into(),
        role: Role::Mx,
        limits: Limits::default(),
        policy: Arc::new(Local),
        sink: Arc::clone(&collector),
    });
    tokio::spawn(async move { let _ = server.serve(listener).await; });
    (addr, collector)
}

struct Client {
    reader: BufReader<tokio::net::tcp::OwnedReadHalf>,
    writer: tokio::net::tcp::OwnedWriteHalf,
}

impl Client {
    async fn connect(addr: &str) -> Self {
        let (r, w) = TcpStream::connect(addr).await.unwrap().into_split();
        let mut c = Self { reader: BufReader::new(r), writer: w };
        c.read().await; // greeting
        c
    }

    /// Read a whole SMTP reply, not one line.
    ///
    /// EHLO answers with a multi-line reply: every line but the last has a
    /// hyphen after the code. A client that reads a single line falls one
    /// reply behind for the rest of the conversation and blames the server.
    async fn read(&mut self) -> String {
        let mut out = String::new();
        loop {
            let mut line = String::new();
            self.reader.read_line(&mut line).await.unwrap();
            let last = line.as_bytes().get(3) != Some(&b'-');
            out.push_str(&line);
            if last || line.is_empty() {
                return out;
            }
        }
    }

    async fn send(&mut self, line: &str) -> String {
        self.writer.write_all(format!("{line}\r\n").as_bytes()).await.unwrap();
        self.read().await
    }

    /// Send a line expecting no reply (inside DATA).
    async fn feed(&mut self, line: &str) {
        self.writer.write_all(format!("{line}\r\n").as_bytes()).await.unwrap();
    }
}

#[tokio::test]
async fn a_message_travels_from_the_wire_into_the_sink() {
    let (addr, collector) = start(Arc::new(Collector::default())).await;
    let mut c = Client::connect(&addr).await;

    assert!(c.send("EHLO client.test").await.starts_with("250"));
    assert!(c.send("MAIL FROM:<sender@example.test>").await.starts_with("250"));
    assert!(c.send("RCPT TO:<info@tnhc.dev>").await.starts_with("250"));
    assert!(c.send("DATA").await.starts_with("354"));
    c.feed("Subject: over the wire").await;
    c.feed("").await;
    c.feed("hello from a socket").await;
    assert!(c.send(".").await.starts_with("250"));
    assert!(c.send("QUIT").await.starts_with("221"));

    let got = collector.received.lock().unwrap();
    assert_eq!(got.len(), 1);
    assert_eq!(got[0].0, "sender@example.test");
    assert_eq!(got[0].1, vec!["info@tnhc.dev"]);
    assert!(String::from_utf8_lossy(&got[0].2).contains("hello from a socket"));
}

#[tokio::test]
async fn relaying_is_refused_over_the_wire_too() {
    // The guard is in the state machine, but a listener that bypassed it would
    // be an open relay regardless of how well that machine is tested.
    let (addr, collector) = start(Arc::new(Collector::default())).await;
    let mut c = Client::connect(&addr).await;

    c.send("EHLO client.test").await;
    c.send("MAIL FROM:<spammer@evil.test>").await;
    let r = c.send("RCPT TO:<victim@gmail.com>").await;

    assert!(r.starts_with("550"), "got {r}");
    assert!(collector.received.lock().unwrap().is_empty());
}

#[tokio::test]
async fn a_failed_store_is_a_temporary_failure_not_an_acceptance() {
    // Acknowledging before the message is safely stored tells the sender their
    // mail was accepted when it was not, which is how mail is silently lost.
    let collector = Arc::new(Collector { received: Mutex::new(Vec::new()), fail: true });
    let (addr, _) = start(collector).await;
    let mut c = Client::connect(&addr).await;

    c.send("EHLO client.test").await;
    c.send("MAIL FROM:<a@example.test>").await;
    c.send("RCPT TO:<info@tnhc.dev>").await;
    c.send("DATA").await;
    c.feed("Subject: x").await;
    let r = c.send(".").await;

    assert!(r.starts_with("451"), "expected a temporary failure, got {r}");
}
