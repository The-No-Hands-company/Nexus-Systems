use std::{
    collections::HashSet,
    future,
    sync::{Arc, Mutex},
};

use chrono::Duration;
use nexus_mailsmtp::object_cleanup::run_object_deletion_pass;
use nexus_mailstore::MailStore;
use sqlx::postgres::PgPoolOptions;

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

#[tokio::test]
async fn deletion_pass_cleans_only_the_expired_reserved_generation() {
    let store = store().await;
    let expired = store
        .reserve_external_body(chrono::Utc::now() - Duration::minutes(16))
        .await
        .unwrap();
    let live = store
        .reserve_external_body(chrono::Utc::now())
        .await
        .unwrap();

    let objects = Arc::new(Mutex::new(HashSet::from([
        expired.object_key.clone(),
        live.object_key.clone(),
    ])));
    let deleted = Arc::new(Mutex::new(Vec::new()));
    let callback_objects = Arc::clone(&objects);
    let callback_deleted = Arc::clone(&deleted);
    let now = chrono::Utc::now();
    let pass = run_object_deletion_pass(&store, now, 1_000, move |key| {
        callback_deleted.lock().unwrap().push(key.clone());
        let removed = callback_objects.lock().unwrap().remove(&key);
        future::ready(if removed { Ok(()) } else { Err(()) })
    })
    .await
    .unwrap();

    assert!(
        pass.deleted >= 1,
        "at least the expired reservation should be deleted"
    );
    assert!(deleted.lock().unwrap().contains(&expired.object_key));
    assert!(!deleted.lock().unwrap().contains(&live.object_key));
    assert!(
        objects.lock().unwrap().contains(&live.object_key),
        "a live reservation must not be deleted"
    );
}
