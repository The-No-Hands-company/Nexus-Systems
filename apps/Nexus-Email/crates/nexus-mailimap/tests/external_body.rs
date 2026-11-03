use std::sync::Arc;

use nexus_mailimap::{Authenticator, ImapServer};
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

#[tokio::test]
async fn unavailable_external_body_returns_no_instead_of_success_with_nil() {
    isolation::with_store(|store| async move {
        let subject = Uuid::new_v4().to_string();
        let mailbox = store
            .create_identity_mailbox(&subject, "Owner")
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

        let server = Arc::new(ImapServer {
            hostname: "localhost".into(),
            store: store.clone(),
            auth: Arc::new(TestAuth(subject)),
        });
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let task = tokio::spawn(server.serve(listener));
        let mut stream = BufReader::new(TcpStream::connect(addr).await.unwrap());
        let mut greeting = String::new();
        stream.read_line(&mut greeting).await.unwrap();
        stream
            .get_mut()
            .write_all(b"a1 LOGIN user pass\r\n")
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
        let fetched = reply(&mut stream, "a3 ").await;
        assert!(fetched.contains("a3 NO"), "{fetched}");
        assert!(!fetched.contains("BODY[] NIL"));
        assert!(!fetched.contains(&reservation.object_key));
        task.abort();
    })
    .await;
}
