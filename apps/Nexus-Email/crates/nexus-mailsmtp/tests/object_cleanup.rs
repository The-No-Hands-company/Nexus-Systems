use std::{
    collections::HashSet,
    future,
    sync::{Arc, Mutex},
};

use chrono::Duration;
use nexus_mailsmtp::object_cleanup::run_object_deletion_pass;

#[path = "../../nexus-mailstore/tests/support/isolation.rs"]
mod isolation;

#[tokio::test]
async fn deletion_pass_cleans_only_the_expired_reserved_generation() {
    isolation::with_store(|store| async move {
        let start =
            chrono::DateTime::from_timestamp_micros(chrono::Utc::now().timestamp_micros()).unwrap();
        let expired = store.reserve_external_body(start).await.unwrap();
        let unverified = store.reserve_external_body(start).await.unwrap();
        let live = store
            .reserve_external_body(start + Duration::minutes(16))
            .await
            .unwrap();
        store
            .record_external_body_upload(
                &expired,
                "expired-version",
                &expired.generation_id.to_string(),
                start,
            )
            .await
            .unwrap();
        store
            .record_external_body_upload(
                &live,
                "live-version",
                &live.generation_id.to_string(),
                start + Duration::minutes(16),
            )
            .await
            .unwrap();

        let objects = Arc::new(Mutex::new(HashSet::from([
            expired.object_key.clone(),
            unverified.object_key.clone(),
            live.object_key.clone(),
        ])));
        let deleted = Arc::new(Mutex::new(Vec::new()));
        for now in [
            expired.expires_at - Duration::microseconds(1),
            expired.expires_at,
            expired.expires_at + Duration::minutes(15) - Duration::microseconds(1),
            expired.expires_at + Duration::minutes(15),
        ] {
            let callback_objects = Arc::clone(&objects);
            let callback_deleted = Arc::clone(&deleted);
            let pass = run_object_deletion_pass(&store, now, 1_000, move |key| {
                callback_deleted.lock().unwrap().push(key.clone());
                let removed = callback_objects.lock().unwrap().remove(&key);
                future::ready(if removed { Ok(()) } else { Err(()) })
            })
            .await
            .unwrap();
            let eligible = now == expired.expires_at + Duration::minutes(15);
            assert_eq!(pass.deleted, u64::from(eligible));
            assert_eq!(pass.failed, 0);
        }

        assert_eq!(*deleted.lock().unwrap(), vec![expired.object_key.clone()]);
        {
            let objects = objects.lock().unwrap();
            assert!(!objects.contains(&expired.object_key));
            assert!(objects.contains(&unverified.object_key));
            assert!(objects.contains(&live.object_key));
        }
        let (version,): (Option<String>,) =
            sqlx::query_as("SELECT version_id FROM object_deletion_queue WHERE generation_id = $1")
                .bind(unverified.generation_id)
                .fetch_one(store.pool())
                .await
                .unwrap();
        assert_eq!(
            version, None,
            "unknown versions must remain queued and unclaimed"
        );
    })
    .await;
}
