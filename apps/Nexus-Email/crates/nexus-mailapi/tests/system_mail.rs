//! The system-mail endpoint end to end: what it refuses, and exactly what it
//! queues when it accepts. Needs Postgres.

use std::sync::Arc;

use nexus_mailapi::system_mail::SystemMailBudget;
use nexus_mailapi::{router, AppState};
use nexus_maildelivery::{Deliverer, Queue, Router};
use nexus_mailstore::MailStore;
use sqlx::postgres::PgPoolOptions;
use sqlx::PgPool;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};
use uuid::Uuid;

const TOKEN: &str = "test-system-mail-token"; // pragma: allowlist secret
const CODE: &str = "0123456789abcdef0123456789abcdef";  // pragma: allowlist secret (fake test code)

struct Api {
    addr: std::net::SocketAddr,
    pool: PgPool,
    domain: String,
}

async fn start(token: Option<&str>, daily_limit: u32) -> Api {
    let url = std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL")
        .expect("NEXUS_EMAIL_TEST_DATABASE_URL must be set");
    let pool = PgPoolOptions::new().max_connections(4).connect(&url).await.unwrap();
    let store = MailStore::new(pool.clone());
    let domain = format!("t{}.test", Uuid::now_v7().simple());
    let deliverer = Deliverer::new(store.clone(), Queue::new(pool.clone()), Router::new().with_local_domain(&domain));
    let state = Arc::new(AppState {
        store,
        deliverer,
        primary_domain: domain.clone(),
        cloudflare_ingress_token: None,
        system_mail_token: token.map(str::to_string),
        system_mail_budget: SystemMailBudget::new(daily_limit),
    });
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let addr = listener.local_addr().unwrap();
    tokio::spawn(async move {
        let _ = axum::serve(listener, router(state)).await;
    });
    Api { addr, pool, domain }
}

async fn post(addr: std::net::SocketAddr, token: Option<&str>, body: &str) -> (u16, String) {
    let mut s = TcpStream::connect(addr).await.unwrap();
    let mut req = format!(
        "POST /internal/v1/system-mail HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Type: application/json\r\nContent-Length: {}\r\n",
        body.len()
    );
    if let Some(t) = token {
        req.push_str(&format!("Authorization: Bearer {t}\r\n"));
    }
    req.push_str("\r\n");
    s.write_all(req.as_bytes()).await.unwrap();
    s.write_all(body.as_bytes()).await.unwrap();
    let mut out = Vec::new();
    s.read_to_end(&mut out).await.unwrap();
    let text = String::from_utf8_lossy(&out).into_owned();
    let status = text.split_whitespace().nth(1).and_then(|c| c.parse().ok()).unwrap_or(0);
    (status, text)
}

fn invite(domain: &str, to: &str) -> String {
    format!(r#"{{"template":"invite","to":"{to}","link":"https://auth.{domain}/join?code={CODE}","expiresAt":"2026-10-20T12:00:00Z"}}"#)
}

#[tokio::test]
async fn test_system_mail_when_not_configured_should_answer_503() {
    let api = start(None, 50).await;
    let (status, _) = post(api.addr, Some(TOKEN), &invite(&api.domain, "ada@elsewhere.test")).await;
    assert_eq!(status, 503);
}

#[tokio::test]
async fn test_system_mail_when_token_is_missing_or_wrong_should_answer_401_before_reading_the_body() {
    let api = start(Some(TOKEN), 50).await;
    assert_eq!(post(api.addr, None, &invite(&api.domain, "ada@elsewhere.test")).await.0, 401);
    assert_eq!(post(api.addr, Some("wrong"), &invite(&api.domain, "ada@elsewhere.test")).await.0, 401);
    // Garbage with no token is still 401, not a parse error that describes the shape.
    assert_eq!(post(api.addr, None, "not json").await.0, 401);
}

#[tokio::test]
async fn test_system_mail_when_link_points_elsewhere_should_answer_400() {
    let api = start(Some(TOKEN), 50).await;
    let body = format!(r#"{{"template":"invite","to":"ada@elsewhere.test","link":"https://evil.example/join?code={CODE}"}}"#);
    assert_eq!(post(api.addr, Some(TOKEN), &body).await.0, 400);
}

#[tokio::test]
async fn test_system_mail_when_valid_should_queue_one_message_from_noreply_with_no_sent_copy() {
    let api = start(Some(TOKEN), 50).await;
    let to = format!("u-{}@elsewhere.test", Uuid::now_v7().simple());
    let (status, body) = post(api.addr, Some(TOKEN), &invite(&api.domain, &to)).await;
    assert_eq!(status, 202, "{body}");
    assert!(body.contains(r#""status":"sent""#));

    let rows: Vec<(String, Uuid)> = sqlx::query_as("SELECT envelope_from, message_id FROM outbound_queue WHERE recipient = $1")
        .bind(&to)
        .fetch_all(&api.pool)
        .await
        .unwrap();
    assert_eq!(rows.len(), 1);
    assert_eq!(rows[0].0, format!("noreply@{}", api.domain));
    let filed: (i64,) = sqlx::query_as("SELECT count(*) FROM mailbox_messages WHERE message_id = $1")
        .bind(rows[0].1)
        .fetch_one(&api.pool)
        .await
        .unwrap();
    assert_eq!(filed.0, 0, "system mail must not be filed in any mailbox");
}

#[tokio::test]
async fn test_system_mail_when_daily_limit_is_reached_should_answer_429() {
    let api = start(Some(TOKEN), 1).await;
    let first = post(api.addr, Some(TOKEN), &invite(&api.domain, &format!("a-{}@elsewhere.test", Uuid::now_v7().simple()))).await;
    assert_eq!(first.0, 202, "{}", first.1);
    let second = post(api.addr, Some(TOKEN), &invite(&api.domain, &format!("b-{}@elsewhere.test", Uuid::now_v7().simple()))).await;
    assert_eq!(second.0, 429);
}
