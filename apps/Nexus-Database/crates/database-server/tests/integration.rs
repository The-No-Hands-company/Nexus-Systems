//! Integration tests for the Nexus-Database server.
//!
//! Spins up a real HTTP server with in-memory state (no control-plane,
//! no auth) and drives all endpoints through HTTP requests.

use axum::body::Body;
use axum::http::{header, Request, StatusCode};
use axum::Router;
use serde_json::Value;
use tower::ServiceExt; // for `oneshot`

// Reuse the server's route assembly, but with test-specific state.
use database_server::*;

// ── Helpers ──────────────────────────────────────────────────────────

/// Build a test app with no persistence and no auth.
async fn test_app() -> Router {
    let persistence = persistence::Persistence::new(None).await;
    let state = state::AppState::new(persistence).await;

    let routes = Router::new()
        .route("/health", axum::routing::get(routes::health))
        .route("/studio", axum::routing::get(routes::studio))
        .route("/databases", axum::routing::get(routes::list_databases))
        .route("/databases", axum::routing::post(routes::provision_database))
        .route(
            "/databases/:id",
            axum::routing::get(routes::get_database),
        )
        .route(
            "/databases/:id",
            axum::routing::delete(routes::delete_database),
        )
        .route(
            "/databases/:id/query",
            axum::routing::post(routes::query_database),
        )
        .route(
            "/databases/:id/snapshot",
            axum::routing::get(routes::export_snapshot),
        )
        .route(
            "/databases/:id/snapshot/restore",
            axum::routing::post(routes::restore_snapshot),
        );

    routes.with_state(state)
}

async fn json_body(body: Body) -> Value {
    let bytes = axum::body::to_bytes(body, usize::MAX).await.unwrap();
    serde_json::from_slice(&bytes).unwrap()
}

// ── Health ───────────────────────────────────────────────────────────

#[tokio::test]
async fn health_returns_ok() {
    let app = test_app().await;
    let resp = app
        .oneshot(Request::get("/health").body(Body::empty()).unwrap())
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["status"], "ok");
    assert_eq!(body["managed_databases"], 0);
    assert_eq!(body["persistence"], false);
    assert!(body["uptime_seconds"].as_f64().unwrap() >= 0.0);
}

// ── Provision ────────────────────────────────────────────────────────

#[tokio::test]
async fn provision_postgres_returns_created() {
    let app = test_app().await;
    let resp = app
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"mydb","engine":"postgres"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::CREATED);
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["name"], "mydb");
    assert_eq!(body["engine"], "postgres");
    assert_eq!(body["status"], "provisioning");
    assert!(body["id"].as_str().unwrap().len() > 0);
}

#[tokio::test]
async fn provision_sqlite_returns_created() {
    let app = test_app().await;
    let resp = app
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"sqlitedb","engine":"sqlite"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::CREATED);
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["name"], "sqlitedb");
    assert_eq!(body["engine"], "sqlite");
}

#[tokio::test]
async fn provision_duplicate_name_fails() {
    let app = test_app().await;
    // First provision
    app.clone()
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"unique","engine":"postgres"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    // Second with same name
    let resp = app
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"unique","engine":"sqlite"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::from_u16(500).unwrap());
}

#[tokio::test]
async fn provision_empty_name_fails() {
    let app = test_app().await;
    let resp = app
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(r#"{"name":"","engine":"postgres"}"#))
                .unwrap(),
        )
        .await
        .unwrap();

    assert!(!resp.status().is_success());
}

#[tokio::test]
async fn provision_bad_engine_fails() {
    let app = test_app().await;
    let resp = app
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"test","engine":"mysql"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    assert!(!resp.status().is_success());
}

// ── List ─────────────────────────────────────────────────────────────

#[tokio::test]
async fn list_empty_returns_empty_array() {
    let app = test_app().await;
    let resp = app
        .oneshot(Request::get("/databases").body(Body::empty()).unwrap())
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let body = json_body(resp.into_body()).await;
    assert!(body.as_array().unwrap().is_empty());
}

#[tokio::test]
async fn list_after_provision_shows_databases() {
    let app = test_app().await;
    // Add two databases
    for (name, engine) in &[("a", "postgres"), ("b", "sqlite")] {
        app.clone()
            .oneshot(
                Request::post("/databases")
                    .header(header::CONTENT_TYPE, "application/json")
                    .body(Body::from(
                        serde_json::json!({"name": name, "engine": engine}).to_string(),
                    ))
                    .unwrap(),
            )
            .await
            .unwrap();
    }

    let resp = app
        .oneshot(Request::get("/databases").body(Body::empty()).unwrap())
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let body = json_body(resp.into_body()).await;
    let arr = body.as_array().unwrap();
    assert_eq!(arr.len(), 2);
}

// ── Get by ID ────────────────────────────────────────────────────────

#[tokio::test]
async fn get_by_id_returns_database() {
    let app = test_app().await;
    let create_resp = app
        .clone()
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"getme","engine":"postgres"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    let created: Value = json_body(create_resp.into_body()).await;
    let id = created["id"].as_str().unwrap();

    let resp = app
        .oneshot(
            Request::get(&format!("/databases/{id}"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["id"], id);
    assert_eq!(body["name"], "getme");
}

#[tokio::test]
async fn get_nonexistent_returns_404() {
    let app = test_app().await;
    let fake_id = "00000000-0000-0000-0000-000000000000";
    let resp = app
        .oneshot(
            Request::get(&format!("/databases/{fake_id}"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::NOT_FOUND);
}

// ── Delete ───────────────────────────────────────────────────────────

#[tokio::test]
async fn delete_returns_no_content() {
    let app = test_app().await;
    let create_resp = app
        .clone()
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"deleteme","engine":"sqlite"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    let created: Value = json_body(create_resp.into_body()).await;
    let id = created["id"].as_str().unwrap();

    let resp = app
        .clone()
        .oneshot(
            Request::delete(&format!("/databases/{id}"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::NO_CONTENT);

    // Database should no longer be found
    let get_resp = app
        .oneshot(
            Request::get(&format!("/databases/{id}"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(get_resp.status(), StatusCode::NOT_FOUND);
}

#[tokio::test]
async fn delete_nonexistent_returns_404() {
    let app = test_app().await;
    let fake_id = "00000000-0000-0000-0000-000000000000";
    let resp = app
        .oneshot(
            Request::delete(&format!("/databases/{fake_id}"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::NOT_FOUND);
}

// ── Query ────────────────────────────────────────────────────────────

#[tokio::test]
async fn query_read_returns_results() {
    let app = test_app().await;

    // Provision a DB — use a name-based SQLite that creates the file
    let create_resp = app
        .clone()
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"querytest","engine":"sqlite"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    let created: Value = json_body(create_resp.into_body()).await;
    let id = created["id"].as_str().unwrap();

    // Execute a read query
    let resp = app
        .oneshot(
            Request::post(&format!("/databases/{id}/query"))
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"sql":"SELECT 42 AS answer, 'hello' AS greeting","readonly":true}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["row_count"], 1);
    assert!(body["columns"].as_array().unwrap().len() >= 1);
    assert!(body["rows"].as_array().unwrap().len() == 1);
    assert!(body["elapsed_ms"].as_f64().unwrap() >= 0.0);
}

#[tokio::test]
async fn query_write_returns_affected() {
    let app = test_app().await;

    let create_resp = app
        .clone()
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"writetest","engine":"sqlite"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    let created: Value = json_body(create_resp.into_body()).await;
    let id = created["id"].as_str().unwrap();

    // Create a table
    let resp = app
        .clone()
        .oneshot(
            Request::post(&format!("/databases/{id}/query"))
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"sql":"CREATE TABLE items (id INTEGER PRIMARY KEY, label TEXT)","readonly":false}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let body = json_body(resp.into_body()).await;
    // Write response should have rows_affected
    assert_eq!(body["columns"][0], "rows_affected");

    // Insert data
    let resp = app
        .oneshot(
            Request::post(&format!("/databases/{id}/query"))
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"sql":"INSERT INTO items (id, label) VALUES (1, 'test')","readonly":false}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["rows"][0][0], 1); // rows_affected = 1
}

#[tokio::test]
async fn query_on_nonexistent_database_fails() {
    let app = test_app().await;
    let fake_id = "00000000-0000-0000-0000-000000000000";
    let resp = app
        .oneshot(
            Request::post(&format!("/databases/{fake_id}/query"))
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(r#"{"sql":"SELECT 1","readonly":true}"#))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::NOT_FOUND);
}

// ── Health after mutations ───────────────────────────────────────────

#[tokio::test]
async fn health_reflects_database_count() {
    let app = test_app().await;

    // Check initial count
    let resp = app
        .clone()
        .oneshot(Request::get("/health").body(Body::empty()).unwrap())
        .await
        .unwrap();
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["managed_databases"], 0);

    // Add one
    app.clone()
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(
                    r#"{"name":"h1","engine":"sqlite"}"#,
                ))
                .unwrap(),
        )
        .await
        .unwrap();

    let resp = app
        .oneshot(Request::get("/health").body(Body::empty()).unwrap())
        .await
        .unwrap();
    let body = json_body(resp.into_body()).await;
    assert_eq!(body["managed_databases"], 1);
}

// ── Studio & Snapshots ───────────────────────────────────────────────

#[tokio::test]
async fn studio_serves_html() {
    let app = test_app().await;
    let resp = app
        .oneshot(Request::get("/studio").body(Body::empty()).unwrap())
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::OK);
    let bytes = axum::body::to_bytes(resp.into_body(), usize::MAX).await.unwrap();
    let html = String::from_utf8(bytes.to_vec()).unwrap();
    assert!(html.contains("Nexus Database Studio"));
}

#[tokio::test]
async fn export_and_restore_snapshot() {
    let app = test_app().await;

    // 1. Provision a SQLite DB
    let resp = app
        .clone()
        .oneshot(
            Request::post("/databases")
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(r#"{"name":"snap_test","engine":"sqlite"}"#))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(resp.status(), StatusCode::CREATED);
    let body = json_body(resp.into_body()).await;
    let db_id = body["id"].as_str().unwrap();

    // 2. Create a table and insert a row
    let create_resp = app
        .clone()
        .oneshot(
            Request::post(format!("/databases/{db_id}/query"))
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(r#"{"sql":"CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT); INSERT INTO users (name) VALUES ('Alice');","readonly":false}"#))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(create_resp.status(), StatusCode::OK);

    // 3. Export snapshot
    let snap_resp = app
        .clone()
        .oneshot(
            Request::get(format!("/databases/{db_id}/snapshot"))
                .body(Body::empty())
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(snap_resp.status(), StatusCode::OK);
    let snap_body = json_body(snap_resp.into_body()).await;
    assert_eq!(snap_body["name"], "snap_test");
    assert!(!snap_body["tables"].as_array().unwrap().is_empty());

    // 4. Restore snapshot into the database
    let restore_resp = app
        .oneshot(
            Request::post(format!("/databases/{db_id}/snapshot/restore"))
                .header(header::CONTENT_TYPE, "application/json")
                .body(Body::from(snap_body.to_string()))
                .unwrap(),
        )
        .await
        .unwrap();
    assert_eq!(restore_resp.status(), StatusCode::OK);
    let res_body = json_body(restore_resp.into_body()).await;
    assert_eq!(res_body["status"], "ok");
}
