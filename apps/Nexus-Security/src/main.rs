use axum::{http::StatusCode, response::Json, routing::get, Router};
use serde::Serialize;
use std::time::{SystemTime, UNIX_EPOCH};
use tracing_subscriber;

#[derive(Serialize)]
struct HealthResponse {
    service: String,
    status: String,
    version: String,
    uptime_seconds: u64,
}

async fn health() -> (StatusCode, Json<HealthResponse>) {
    let uptime = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0);
    (
        StatusCode::OK,
        Json(HealthResponse {
            service: "nexus-security".into(),
            status: "ok".into(),
            version: "v1".into(),
            uptime_seconds: uptime,
        }),
    )
}

#[derive(Serialize)]
struct StatusResponse {
    service: String,
    status: String,
    capabilities: Vec<String>,
}

async fn api_status() -> Json<StatusResponse> {
    Json(StatusResponse {
        service: "nexus-security".into(),
        status: "ready".into(),
        capabilities: vec![
            "threat-detection".into(),
            "confidential-computing".into(),
            "provenance".into(),
            "crypto".into(),
        ],
    })
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt::init();

    let app = Router::new()
        .route("/health", get(health))
        .route("/api/v1/status", get(api_status));

    let port: u16 = std::env::var("PORT")
        .unwrap_or_else(|_| "3120".into())
        .parse()
        .unwrap_or(3120);

    let addr = format!("0.0.0.0:{}", port);
    tracing::info!("[nexus-security] Listening on port {}", port);

    let listener = tokio::net::TcpListener::bind(&addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}
