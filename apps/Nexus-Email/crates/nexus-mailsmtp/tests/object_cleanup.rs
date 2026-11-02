use std::{
    collections::HashSet,
    future,
    sync::{Arc, Mutex},
};

use nexus_mailsmtp::object_cleanup::run_object_deletion_pass;
use nexus_mailstore::{Address, FolderKind, MailStore, MailStoreError, Transport};
use sqlx::postgres::PgPoolOptions;
use uuid::Uuid;

async fn store() -> MailStore {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new()
        .max_connections(4)
        .connect(&url)
        .await
        .expect("connect to the test database");
    MailStore::new(pool)
}

fn unique(prefix: &str) -> String {
    format!("{prefix}-{}", Uuid::now_v7().simple())
}

#[tokio::test]
async fn deletion_pass_only_removes_the_queued_body_when_a_new_external_message_arrives() {
    let store = store().await;
    let mailbox = store
        .create_identity_mailbox(&unique("usr"), "Alice")
        .await
        .unwrap();
    let inbox = store.folder(mailbox.id, FolderKind::Inbox).await.unwrap();
    let from = Address::parse("sender@example.test").unwrap();
    let old_key = unique("mail-bodies/old");
    let new_key = unique("mail-bodies/new");
    let old_thread = store.thread_for(None, &[], Some("old")).await.unwrap();
    let old_message = store
        .store_message(
            b"old externally stored body",
            old_thread,
            &from,
            Some("old"),
            None,
            None,
            &[],
            Transport::Internal,
            Some(&old_key),
        )
        .await
        .unwrap();
    store.deliver(mailbox.id, old_message, inbox).await.unwrap();
    store
        .erase_membership(mailbox.id, old_message)
        .await
        .unwrap();

    let reuse_thread = store.thread_for(None, &[], Some("reuse")).await.unwrap();
    let reused = store
        .store_message(
            b"replacement body for the old object key",
            reuse_thread,
            &from,
            Some("reuse"),
            None,
            None,
            &[],
            Transport::Internal,
            Some(&old_key),
        )
        .await;
    assert!(matches!(
        reused,
        Err(MailStoreError::ObjectKeyAlreadyRegistered)
    ));

    let new_thread = store.thread_for(None, &[], Some("new")).await.unwrap();
    let new_message = store
        .store_message(
            b"new externally stored body",
            new_thread,
            &from,
            Some("new"),
            None,
            None,
            &[],
            Transport::Internal,
            Some(&new_key),
        )
        .await
        .unwrap();
    store.deliver(mailbox.id, new_message, inbox).await.unwrap();

    let now = chrono::Utc::now();
    sqlx::query(
        "UPDATE object_deletion_queue q
         SET next_attempt_at = $1
         FROM object_generations g
         WHERE q.generation_id = g.id AND g.object_key = $2",
    )
    .bind(now - chrono::Duration::days(36_500))
    .bind(&old_key)
    .execute(store.pool())
    .await
    .unwrap();

    let objects = Arc::new(Mutex::new(HashSet::from([
        old_key.clone(),
        new_key.clone(),
    ])));
    let deleted = Arc::new(Mutex::new(Vec::new()));
    let callback_objects = Arc::clone(&objects);
    let callback_deleted = Arc::clone(&deleted);
    let pass = run_object_deletion_pass(&store, now, 1, move |key| {
        callback_deleted.lock().unwrap().push(key.clone());
        let removed = callback_objects.lock().unwrap().remove(&key);
        future::ready(if removed { Ok(()) } else { Err(()) })
    })
    .await
    .unwrap();

    assert_eq!(pass.deleted, 1);
    assert_eq!(*deleted.lock().unwrap(), vec![old_key]);
    assert!(
        objects.lock().unwrap().contains(&new_key),
        "the newly referenced external body must remain readable"
    );
    assert_eq!(
        store
            .message_folder(mailbox.id, new_message)
            .await
            .unwrap()
            .unwrap()
            .id,
        inbox,
        "the new message remains accessible through its mailbox"
    );
}
