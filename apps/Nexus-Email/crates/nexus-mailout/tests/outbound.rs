//! Outbound delivery. The retry contract is the subject of most of these:
//! mistaking a temporary failure for a permanent one loses mail outright.

use std::sync::{Arc, Mutex};

use nexus_mailout::{classify, code_of, deliver, is_final_line, Attempt, Disposition};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::TcpListener;

#[test]
fn reply_codes_map_to_the_right_retry_behaviour() {
    assert_eq!(classify(250), Disposition::Ok);
    assert_eq!(classify(354), Disposition::Ok);
    assert_eq!(classify(421), Disposition::Temporary);
    assert_eq!(classify(451), Disposition::Temporary);
    assert_eq!(classify(550), Disposition::Permanent);
    assert_eq!(classify(552), Disposition::Permanent);
}

#[test]
fn an_unrecognised_code_is_treated_as_temporary() {
    // Guessing "permanent" on a reply we cannot make sense of throws mail away
    // on the strength of that guess. Deferring costs a few days of retries.
    assert_eq!(classify(199), Disposition::Temporary);
    assert_eq!(classify(600), Disposition::Temporary);
    assert_eq!(classify(0), Disposition::Temporary);
}

#[test]
fn multi_line_replies_are_recognised() {
    // Every line but the last has a hyphen in the fourth position. Ignoring
    // that puts a client one reply behind for the rest of the conversation.
    assert!(!is_final_line("250-STARTTLS"));
    assert!(is_final_line("250 OK"));
    assert_eq!(code_of("550 no such user"), Some(550));
    assert_eq!(code_of("nonsense"), None);
}

// ── Against a real server ───────────────────────────────────────────────────

/// A scripted SMTP server: replies from a list, and records what it was told.
async fn scripted(replies: Vec<&'static str>) -> (String, Arc<Mutex<Vec<String>>>) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    let seen = Arc::new(Mutex::new(Vec::new()));
    let recorded = Arc::clone(&seen);

    tokio::spawn(async move {
        let (stream, _) = listener.accept().await.unwrap();
        let (r, mut w) = stream.into_split();
        let mut reader = BufReader::new(r);
        let mut replies = replies.into_iter();

        // Greeting.
        let _ = w.write_all(replies.next().unwrap_or("220 ready\r\n").as_bytes()).await;

        let mut in_data = false;
        loop {
            let mut line = String::new();
            if reader.read_line(&mut line).await.unwrap_or(0) == 0 {
                return;
            }
            recorded.lock().unwrap().push(line.trim_end().to_string());

            if in_data {
                if line.trim_end() == "." {
                    in_data = false;
                    let _ = w.write_all(replies.next().unwrap_or("250 ok\r\n").as_bytes()).await;
                }
                continue;
            }
            if line.to_uppercase().starts_with("DATA") {
                in_data = true;
            }
            if line.to_uppercase().starts_with("QUIT") {
                return;
            }
            let _ = w.write_all(replies.next().unwrap_or("250 ok\r\n").as_bytes()).await;
        }
    });

    (addr.to_string(), seen)
}

fn split(addr: &str) -> (String, u16) {
    let (h, p) = addr.rsplit_once(':').unwrap();
    (h.to_string(), p.parse().unwrap())
}

#[tokio::test]
async fn a_message_is_delivered_and_the_conversation_is_correct() {
    let (addr, seen) = scripted(vec![
        "220 mx.example ready\r\n",
        "250-mx.example\r\n250 SIZE 100000\r\n", // multi-line EHLO
        "250 sender ok\r\n",
        "250 recipient ok\r\n",
        "354 go ahead\r\n",
        "250 2.0.0 accepted\r\n",
    ])
    .await;
    let (host, port) = split(&addr);

    let out = deliver(&host, port, "mail.tnhc.dev", "a@tnhc.dev", "b@example.test",
                      b"Subject: hi\r\n\r\nbody\r\n").await;
    assert_eq!(out, Attempt::Delivered);

    let said = seen.lock().unwrap().clone();
    assert!(said.iter().any(|l| l.starts_with("EHLO mail.tnhc.dev")));
    assert!(said.iter().any(|l| l == "MAIL FROM:<a@tnhc.dev>"));
    assert!(said.iter().any(|l| l == "RCPT TO:<b@example.test>"));
    assert!(said.iter().any(|l| l == "body"));
}

#[tokio::test]
async fn a_5xx_refusal_is_permanent() {
    let (addr, _) = scripted(vec![
        "220 ready\r\n",
        "250 ok\r\n",
        "250 ok\r\n",
        "550 5.1.1 no such user\r\n",
    ])
    .await;
    let (host, port) = split(&addr);

    match deliver(&host, port, "me", "a@b.test", "ghost@example.test", b"x").await {
        Attempt::Rejected(r) => assert!(r.contains("550")),
        other => panic!("expected a permanent rejection, got {other:?}"),
    }
}

#[tokio::test]
async fn a_4xx_refusal_is_temporary() {
    let (addr, _) = scripted(vec![
        "220 ready\r\n",
        "250 ok\r\n",
        "450 4.2.0 mailbox busy\r\n",
    ])
    .await;
    let (host, port) = split(&addr);

    match deliver(&host, port, "me", "a@b.test", "busy@example.test", b"x").await {
        Attempt::Deferred(r) => assert!(r.contains("450")),
        other => panic!("expected a deferral, got {other:?}"),
    }
}

#[tokio::test]
async fn a_blocked_or_refused_port_defers_and_never_bounces() {
    // The case that matters on this node: outbound 25 is filtered by the ISP,
    // so every direct attempt fails at connect. If that read as permanent, the
    // queue would bounce perfectly good mail the moment an egress path
    // appeared — and the sender would have no copy.
    let out = deliver("127.0.0.1", 9, "me", "a@b.test", "c@d.test", b"x").await;
    match out {
        Attempt::Deferred(reason) => assert!(reason.contains("connect")),
        other => panic!("a dead port must defer, not bounce: {other:?}"),
    }
}

#[tokio::test]
async fn a_body_line_starting_with_a_dot_is_stuffed() {
    // Without stuffing, the receiving server reads that line as the end of the
    // message and silently truncates everything after it.
    let (addr, seen) = scripted(vec![
        "220 ready\r\n", "250 ok\r\n", "250 ok\r\n", "250 ok\r\n", "354 go\r\n", "250 ok\r\n",
    ])
    .await;
    let (host, port) = split(&addr);

    deliver(&host, port, "me", "a@b.test", "c@d.test",
            b"Subject: x\r\n\r\n.hidden line\r\nafter\r\n").await;

    let said = seen.lock().unwrap().clone();
    assert!(said.iter().any(|l| l == "..hidden line"), "dot line must be stuffed: {said:?}");
    assert!(said.iter().any(|l| l == "after"), "content after the dot line must survive");
}

/// The lines the server received between DATA and the terminating dot.
fn data_lines(said: &[String]) -> Vec<String> {
    let start = said.iter().position(|l| l.eq_ignore_ascii_case("DATA")).unwrap() + 1;
    let end = start + said[start..].iter().position(|l| l == ".").unwrap();
    said[start..end].to_vec()
}

#[tokio::test]
async fn a_message_that_ends_in_crlf_gains_no_extra_blank_line() {
    // The line terminator already present must not be read as the start of
    // one more, empty, line: that appends a blank line to every message sent,
    // and a relay would deliver bytes that differ from the origin's.
    let (addr, seen) = scripted(vec![
        "220 ready\r\n", "250 ok\r\n", "250 ok\r\n", "250 ok\r\n", "354 go\r\n", "250 ok\r\n",
    ])
    .await;
    let (host, port) = split(&addr);

    deliver(&host, port, "me", "a@b.test", "c@d.test", b"Subject: x\r\n\r\nlast\r\n").await;

    let said = seen.lock().unwrap().clone();
    assert_eq!(data_lines(&said), vec!["Subject: x", "", "last"]);
}

#[tokio::test]
async fn a_message_without_a_final_crlf_is_terminated_properly() {
    // RFC 5321 needs CRLF before the closing dot; the client supplies it.
    let (addr, seen) = scripted(vec![
        "220 ready\r\n", "250 ok\r\n", "250 ok\r\n", "250 ok\r\n", "354 go\r\n", "250 ok\r\n",
    ])
    .await;
    let (host, port) = split(&addr);

    deliver(&host, port, "me", "a@b.test", "c@d.test", b"Subject: x\r\n\r\nlast").await;

    let said = seen.lock().unwrap().clone();
    assert_eq!(data_lines(&said), vec!["Subject: x", "", "last"]);
}

// ── Logs carry no personal data ─────────────────────────────────────────────

/// One global subscriber that writes into a per-thread buffer. A scoped
/// subscriber per test is unreliable here: other tests in this binary log with
/// none installed, which makes the call sites flap. Each test runs on its own
/// single thread, so its buffer holds exactly what it logged.
mod capture {
    use std::cell::RefCell;
    use std::sync::Once;

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

    /// Install the subscriber (once) and clear this thread's buffer.
    pub fn start() {
        static ONCE: Once = Once::new();
        ONCE.call_once(|| {
            let sub = tracing_subscriber::fmt().with_writer(Sink).with_max_level(tracing::Level::TRACE).finish();
            tracing::subscriber::set_global_default(sub).unwrap();
        });
        BUF.with(|b| b.borrow_mut().clear());
    }

    /// Everything this thread has logged since `start`.
    pub fn text() -> String {
        BUF.with(|b| String::from_utf8(b.borrow().clone()).unwrap())
    }
}

#[tokio::test]
async fn the_delivery_log_never_names_the_recipient() {
    capture::start();

    // The remote's own reply is also kept out of the log: servers routinely
    // echo the recipient back in their acceptance text.
    let (addr, _) = scripted(vec![
        "220 mx.example ready\r\n",
        "250 hello\r\n",
        "250 sender ok\r\n",
        "250 recipient ok\r\n",
        "354 go ahead\r\n",
        "250 2.0.0 queued for someone@example.com\r\n",
    ])
    .await;
    let (host, port) = split(&addr);

    let out = deliver(&host, port, "mail.tnhc.dev", "a@tnhc.dev", "someone@example.com",
                      b"Subject: hi\r\n\r\nbody\r\n").await;
    assert_eq!(out, Attempt::Delivered);

    let log = capture::text();
    assert!(log.contains("delivered"), "the delivery must still be logged: {log:?}");
    assert!(!log.contains("someone@example.com"), "recipient leaked into the log: {log:?}");
    assert!(!log.contains("example.com"), "recipient domain leaked into the log: {log:?}");
}
