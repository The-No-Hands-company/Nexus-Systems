use std::pin::Pin;
use std::sync::Arc;

use nexus_mailimap::{Authenticator, ImapServer};
use nexus_mailstore::object_client::{ObjectBodyClient, ObjectBodyClientError};
use nexus_mailstore::{Address, FolderKind, Transport, INLINE_BODY_LIMIT};
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::net::{TcpListener, TcpStream};
use uuid::Uuid;

#[path = "../../nexus-mailstore/tests/support/isolation.rs"]
mod isolation;

struct TestAuth(String);

impl Authenticator for TestAuth {
    async fn verify(&self, _user: &str, _password: &str) -> Option<String> {
        Some(self.0.clone())
    }
}

struct FixedBodyClient(Vec<u8>);

impl ObjectBodyClient for FixedBodyClient {
    fn read_message(
        &self,
        _message_id: Uuid,
    ) -> Pin<Box<dyn std::future::Future<Output = Result<Vec<u8>, ObjectBodyClientError>> + Send + '_>>
    {
        let body = self.0.clone();
        Box::pin(async move { Ok(body) })
    }
}

struct FailingBodyClient;

impl ObjectBodyClient for FailingBodyClient {
    fn read_message(
        &self,
        _message_id: Uuid,
    ) -> Pin<Box<dyn std::future::Future<Output = Result<Vec<u8>, ObjectBodyClientError>> + Send + '_>>
    {
        Box::pin(async move { Err(ObjectBodyClientError) })
    }
}

async fn reply(stream: &mut BufReader<TcpStream>, tag: &str) -> String {
    let mut result = String::new();
    loop {
        let mut line = String::new();
        assert_ne!(stream.read_line(&mut line).await.unwrap(), 0);
        result.push_str(&line);
        if line.starts_with(tag) {
            return result;
        }
    }
}

async fn deliver_external_message(
    store: &nexus_mailstore::MailStore,
    subject: &str,
) -> (nexus_mailstore::Mailbox, uuid::Uuid) {
    let mailbox = store
        .create_identity_mailbox(subject, "Owner")
        .await
        .unwrap();
    let reservation = store
        .reserve_external_body(chrono::Utc::now())
        .await
        .unwrap();
    store
        .record_external_body_upload(
            &reservation,
            "v1",
            &reservation.generation_id.to_string(),
            chrono::Utc::now(),
        )
        .await
        .unwrap();
    let thread = store.thread_for(None, &[], None).await.unwrap();
    let message = store
        .store_message(
            &vec![b'x'; INLINE_BODY_LIMIT + 1],
            thread,
            &Address::parse("sender@example.test").unwrap(),
            None,
            None,
            None,
            &[],
            Transport::Smtp,
            Some(&reservation),
        )
        .await
        .unwrap();
    let inbox = store.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    store.deliver(mailbox.id, message, inbox).await.unwrap();
    (mailbox, message)
}

async fn imap_fetch_body(
    server: Arc<ImapServer<TestAuth>>,
    subject: &str,
) -> String {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    let task = tokio::spawn(server.serve(listener));
    let mut stream = BufReader::new(TcpStream::connect(addr).await.unwrap());
    let mut greeting = String::new();
    stream.read_line(&mut greeting).await.unwrap();
    stream
        .get_mut()
        .write_all(format!("a1 LOGIN {subject} pass\r\n").as_bytes())
        .await
        .unwrap();
    assert!(reply(&mut stream, "a1 ").await.contains("a1 OK"));
    stream
        .get_mut()
        .write_all(b"a2 SELECT INBOX\r\n")
        .await
        .unwrap();
    assert!(reply(&mut stream, "a2 ").await.contains("a2 OK"));
    stream
        .get_mut()
        .write_all(b"a3 UID FETCH 1 (BODY[])\r\n")
        .await
        .unwrap();
    let result = reply(&mut stream, "a3 ").await;
    task.abort();
    result
}

#[tokio::test]
async fn unavailable_external_body_returns_no_instead_of_success_with_nil() {
    isolation::with_store(|store| async move {
        let subject = Uuid::new_v4().to_string();
        let (mailbox, _) = deliver_external_message(&store, &subject).await;
        let _ = mailbox;

        let server = Arc::new(ImapServer {
            hostname: "localhost".into(),
            store: store.clone(),
            auth: Arc::new(TestAuth(subject.clone())),
            object_client: None,
        });
        let fetched = imap_fetch_body(server, &subject).await;
        assert!(fetched.contains("a3 NO"), "{fetched}");
        assert!(!fetched.contains("BODY[] NIL"), "{fetched}");
    })
    .await;
}

#[tokio::test]
async fn object_service_failure_returns_no_unavailable() {
    isolation::with_store(|store| async move {
        let subject = Uuid::new_v4().to_string();
        let (mailbox, _) = deliver_external_message(&store, &subject).await;
        let _ = mailbox;

        let server = Arc::new(ImapServer {
            hostname: "localhost".into(),
            store: store.clone(),
            auth: Arc::new(TestAuth(subject.clone())),
            object_client: Some(Arc::new(FailingBodyClient)),
        });
        let fetched = imap_fetch_body(server, &subject).await;
        assert!(fetched.contains("a3 NO"), "{fetched}");
        assert!(fetched.contains("[UNAVAILABLE]"), "{fetched}");
    })
    .await;
}

#[tokio::test]
async fn object_service_success_returns_body_bytes() {
    isolation::with_store(|store| async move {
        let subject = Uuid::new_v4().to_string();
        let (mailbox, _) = deliver_external_message(&store, &subject).await;
        let _ = mailbox;

        let body = b"From: sender@example.test\r\nSubject: hi\r\n\r\nHello from object store\r\n";
        let server = Arc::new(ImapServer {
            hostname: "localhost".into(),
            store: store.clone(),
            auth: Arc::new(TestAuth(subject.clone())),
            object_client: Some(Arc::new(FixedBodyClient(body.to_vec()))),
        });
        let fetched = imap_fetch_body(server, &subject).await;
        assert!(fetched.contains("a3 OK"), "{fetched}");
        assert!(fetched.contains("Hello from object store"), "{fetched}");
    })
    .await;
}
