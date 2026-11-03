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
async fn cleanup_deletes_recorded_version_without_touching_a_newer_version() {
    isolation::with_store(|store| async move {
        let start = chrono::Utc::now();
        let reservation = store.reserve_external_body(start).await.unwrap();
        store
            .record_external_body_upload(
                &reservation,
                "v1",
                &reservation.generation_id.to_string(),
                start,
            )
            .await
            .unwrap();
        store
            .abandon_external_body_reservation(&reservation, start)
            .await
            .unwrap();

        let versions = Arc::new(Mutex::new(HashSet::from([
            (reservation.object_key.clone(), "v1".to_string()),
            (reservation.object_key.clone(), "v2".to_string()),
        ])));
        let objects = Arc::clone(&versions);
        let pass = run_object_deletion_pass(
            &store,
            reservation.expires_at + Duration::minutes(15),
            10,
            move |body| {
                let mut objects = objects.lock().unwrap();
                objects.remove(&(body.object_key, body.version_id));
                future::ready(Ok(()))
            },
        )
        .await
        .unwrap();
        assert_eq!(pass.deleted, 1);
        let objects = versions.lock().unwrap();
        assert!(!objects.contains(&(reservation.object_key.clone(), "v1".to_string())));
        assert!(objects.contains(&(reservation.object_key.clone(), "v2".to_string())));
    })
    .await;
}

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
            (expired.object_key.clone(), "expired-version".to_string()),
            (unverified.object_key.clone(), "unknown".to_string()),
            (live.object_key.clone(), "live-version".to_string()),
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
            let pass = run_object_deletion_pass(&store, now, 1_000, move |body| {
                let identity = (body.object_key, body.version_id);
                callback_deleted.lock().unwrap().push(identity.clone());
                let removed = callback_objects.lock().unwrap().remove(&identity);
                future::ready(if removed { Ok(()) } else { Err(()) })
            })
            .await
            .unwrap();
            let eligible = now == expired.expires_at + Duration::minutes(15);
            assert_eq!(pass.deleted, u64::from(eligible));
            assert_eq!(pass.failed, 0);
        }

        assert_eq!(
            *deleted.lock().unwrap(),
            vec![(expired.object_key.clone(), "expired-version".to_string())]
        );
        {
            let objects = objects.lock().unwrap();
            assert!(!objects.contains(&(expired.object_key.clone(), "expired-version".to_string())));
            assert!(objects.contains(&(unverified.object_key.clone(), "unknown".to_string())));
            assert!(objects.contains(&(live.object_key.clone(), "live-version".to_string())));
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
