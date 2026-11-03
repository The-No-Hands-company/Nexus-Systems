use std::{
    collections::HashMap,
    io::{self, Write},
    net::SocketAddr,
    path::{Path, PathBuf},
    process::Stdio,
    sync::{Arc, Mutex},
    time::Duration,
};

use axum::{
    extract::{ConnectInfo, Query},
    http::StatusCode,
    routing::get,
    Extension, Router,
};
use futures_util::StreamExt;
use nexus_mailsmtp::{
    object_gateway::S3BodyGateway,
    object_service::{self, ObjectServiceConfig},
};
use nexus_mailstore::{Address, MailStore, RecordedExternalBody, Transport};
use tokio::{
    io::{AsyncBufReadExt, AsyncReadExt, AsyncWriteExt, BufReader},
    net::TcpListener,
    process::Command,
    sync::{mpsc, watch},
    task::JoinHandle,
};
use tracing::instrument::WithSubscriber;
use uuid::Uuid;

#[path = "../../nexus-mailstore/tests/support/isolation.rs"]
mod isolation;

const API: &str = "api-test-secret";
const IMAP: &str = "imap-test-secret";
const OUTBOUND: &str = "outbound-test-secret";
const VERSION: &str = "sensitive-version-canary";
const BODY: &str = "sensitive-body-canary";

fn config() -> ObjectServiceConfig {
    ObjectServiceConfig {
        bind: "127.0.0.1:0".parse().unwrap(),
        api_token: API.to_string().into(),
        imap_token: IMAP.to_string().into(),
        outbound_token: OUTBOUND.to_string().into(),
    }
}

async fn object_server() -> (Arc<S3BodyGateway>, JoinHandle<()>) {
    let app = Router::new().route(
        "/mail/*key",
        get(|Query(query): Query<HashMap<String, String>>| async move {
            if query.get("versionId").map(String::as_str) == Some(VERSION) {
                (
                    StatusCode::OK,
                    [("x-amz-version-id", VERSION)],
                    "recorded-v1",
                )
            } else {
                (
                    StatusCode::OK,
                    [("x-amz-version-id", "wrong")],
                    "wrong-version",
                )
            }
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let task = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
    let shared = aws_config::defaults(aws_config::BehaviorVersion::latest())
        .region(aws_config::Region::new("us-east-1"))
        .credentials_provider(aws_credential_types::Credentials::new(
            "test", "test", None, None, "test",
        ))
        .endpoint_url(format!("http://{address}"))
        .load()
        .await;
    let s3 = aws_sdk_s3::config::Builder::from(&shared)
        .force_path_style(true)
        .build();
    (
        Arc::new(S3BodyGateway::new(
            aws_sdk_s3::Client::from_conf(s3),
            "mail".into(),
        )),
        task,
    )
}

async fn checksum_storage(body: &'static str) -> (Arc<S3BodyGateway>, JoinHandle<()>) {
    let app = Router::new().route(
        "/mail/*key",
        get(move || async move {
            (
                [
                    ("x-amz-version-id", VERSION),
                    ("x-amz-checksum-crc32", "AQIDBA=="),
                ],
                body,
            )
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let task = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
    let shared = aws_config::defaults(aws_config::BehaviorVersion::latest())
        .region(aws_config::Region::new("us-east-1"))
        .credentials_provider(aws_credential_types::Credentials::new(
            "test", "test", None, None, "test",
        ))
        .endpoint_url(format!("http://{address}"))
        .load()
        .await;
    let s3 = aws_sdk_s3::config::Builder::from(&shared)
        .force_path_style(true)
        .build();
    (
        Arc::new(S3BodyGateway::new(
            aws_sdk_s3::Client::from_conf(s3),
            "mail".into(),
        )),
        task,
    )
}

async fn storage_for_app(app: Router) -> (Arc<S3BodyGateway>, JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let task = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
    let shared = aws_config::defaults(aws_config::BehaviorVersion::latest())
        .region(aws_config::Region::new("us-east-1"))
        .credentials_provider(aws_credential_types::Credentials::new(
            "test", "test", None, None, "test",
        ))
        .endpoint_url(format!("http://{address}"))
        .load()
        .await;
    let s3 = aws_sdk_s3::config::Builder::from(&shared)
        .force_path_style(true)
        .build();
    (
        Arc::new(S3BodyGateway::new(
            aws_sdk_s3::Client::from_conf(s3),
            "mail".into(),
        )),
        task,
    )
}

async fn delayed_storage(
    length: Option<usize>,
) -> (Arc<S3BodyGateway>, watch::Sender<bool>, JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let (release, receiver) = watch::channel(false);
    let task = tokio::spawn(async move {
        loop {
            let (mut socket, _) = listener.accept().await.unwrap();
            let mut receiver = receiver.clone();
            tokio::spawn(async move {
                let mut request = Vec::new();
                let mut buffer = [0; 1024];
                loop {
                    let n = socket.read(&mut buffer).await.unwrap();
                    if n == 0 {
                        return;
                    }
                    request.extend_from_slice(&buffer[..n]);
                    if request.windows(4).any(|window| window == b"\r\n\r\n") {
                        break;
                    }
                }
                let framing = match length {
                    Some(length) => format!("Content-Length: {length}\r\n"),
                    None => "Transfer-Encoding: chunked\r\n".to_string(),
                };
                let headers = format!(
                    "HTTP/1.1 200 OK\r\n{framing}\
                     x-amz-version-id: {VERSION}\r\nConnection: close\r\n\r\n"
                );
                if socket.write_all(headers.as_bytes()).await.is_err() {
                    return;
                }
                while !*receiver.borrow_and_update() {
                    if receiver.changed().await.is_err() {
                        return;
                    }
                }
                if matches!(length, Some(1 | 2)) {
                    let _ = socket.write_all(b"x").await;
                }
            });
        }
    });
    let shared = aws_config::defaults(aws_config::BehaviorVersion::latest())
        .region(aws_config::Region::new("us-east-1"))
        .credentials_provider(aws_credential_types::Credentials::new(
            "test", "test", None, None, "test",
        ))
        .endpoint_url(format!("http://{address}"))
        .load()
        .await;
    let config = aws_sdk_s3::config::Builder::from(&shared)
        .force_path_style(true)
        .stalled_stream_protection(aws_sdk_s3::config::StalledStreamProtectionConfig::disabled())
        .build();
    (
        Arc::new(S3BodyGateway::new(
            aws_sdk_s3::Client::from_conf(config),
            "mail".into(),
        )),
        release,
        task,
    )
}

async fn recorded_message(store: &MailStore) -> (Uuid, String, String) {
    let now = chrono::Utc::now();
    let reservation = store.reserve_external_body(now).await.unwrap();
    store
        .record_external_body_upload(
            &reservation,
            VERSION,
            &reservation.generation_id.to_string(),
            now,
        )
        .await
        .unwrap();
    let sender = Address::parse("sender@example.test").unwrap();
    let thread = store.thread_for(None, &[], None).await.unwrap();
    let message = store
        .store_message(
            format!("{BODY}-{}", Uuid::new_v4()).as_bytes(),
            thread,
            &sender,
            None,
            None,
            None,
            &[],
            Transport::Internal,
            Some(&reservation),
        )
        .await
        .unwrap();
    (
        message,
        reservation.object_key,
        reservation.generation_id.to_string(),
    )
}

async fn serve(app: Router, supplied_peer: Option<SocketAddr>) -> (String, JoinHandle<()>) {
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let task = match supplied_peer {
        Some(peer) => tokio::spawn(async move {
            axum::serve(listener, app.layer(Extension(ConnectInfo(peer))))
                .await
                .unwrap()
        }),
        None => tokio::spawn(async move {
            axum::serve(
                listener,
                app.into_make_service_with_connect_info::<SocketAddr>(),
            )
            .await
            .unwrap()
        }),
    };
    (format!("http://{address}"), task)
}

#[tokio::test]
async fn authenticated_service_streams_only_the_message_recorded_version() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (gateway, object_task) = object_server().await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let client = reqwest::Client::new();
        for token in [API, IMAP, OUTBOUND] {
            let response = client
                .get(format!("{url}/internal/object/messages/{message}"))
                .bearer_auth(token)
                .send()
                .await
                .unwrap();
            assert_eq!(response.status(), StatusCode::OK);
            assert_eq!(
                response.headers().get("content-type").unwrap(),
                "application/octet-stream"
            );
            assert_eq!(response.bytes().await.unwrap(), "recorded-v1");
        }
        let missing = client
            .get(format!("{url}/internal/object/messages/{}", Uuid::new_v4()))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(missing.status(), StatusCode::NOT_FOUND);
        let error = missing.text().await.unwrap();
        for secret in [&key, &generation, VERSION, BODY] {
            assert!(!error.contains(secret));
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn missing_or_inline_body_is_not_found_but_invalid_evidence_is_unavailable() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let sender = Address::parse("sender@example.test").unwrap();
        let thread = store.thread_for(None, &[], None).await.unwrap();
        let inline = store
            .store_message(
                b"inline-only",
                thread,
                &sender,
                None,
                None,
                None,
                &[],
                Transport::Internal,
                None,
            )
            .await
            .unwrap();
        let (gateway, object_task) = object_server().await;
        let (url, service_task) = serve(
            object_service::router(store.clone(), gateway, config()),
            None,
        )
        .await;
        let client = reqwest::Client::new();
        for id in [Uuid::new_v4(), inline] {
            let response = client
                .get(format!("{url}/internal/object/messages/{id}"))
                .bearer_auth(API)
                .send()
                .await
                .unwrap();
            assert_eq!(response.status(), StatusCode::NOT_FOUND);
        }
        sqlx::query("UPDATE object_generations SET uploaded_version_id = NULL WHERE id = $1")
            .bind(Uuid::parse_str(&generation).unwrap())
            .execute(store.pool())
            .await
            .unwrap();
        let response = client
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        let text = response.text().await.unwrap();
        for secret in [&key, &generation, VERSION, BODY] {
            assert!(!text.contains(secret));
        }
        sqlx::query(
            "UPDATE messages SET body_inline = NULL, body_object_key = $2, \
             body_object_generation_id = NULL WHERE id = $1",
        )
        .bind(inline)
        .bind(&key)
        .execute(store.pool())
        .await
        .unwrap();
        let response = client
            .get(format!("{url}/internal/object/messages/{inline}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        assert!(response.text().await.unwrap().is_empty());
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn oversized_recorded_body_is_unavailable_without_deletion_or_truncation() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        sqlx::query("UPDATE messages SET size_bytes = $2 WHERE id = $1")
            .bind(message)
            .bind((26 * 1024 * 1024 + 1) as i64)
            .execute(store.pool())
            .await
            .unwrap();
        let (gateway, _release, object_task) = delayed_storage(Some(26 * 1024 * 1024 + 1)).await;
        let (url, service_task) = serve(
            object_service::router(store.clone(), gateway, config()),
            None,
        )
        .await;
        let response = tokio::time::timeout(
            Duration::from_secs(3),
            reqwest::Client::new()
                .get(format!("{url}/internal/object/messages/{message}"))
                .bearer_auth(API)
                .send(),
        )
        .await
        .expect("oversized response was buffered instead of rejected")
        .unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        assert!(response.text().await.unwrap().is_empty());
        let record = store.external_body_for_message(message).await.unwrap();
        assert_eq!(record.object_key, key);
        assert_eq!(record.generation_id.to_string(), generation);
        assert_eq!(record.version_id, VERSION);
        let state: (String, i64) = sqlx::query_as(
            "SELECT g.lifecycle_state, m.size_bytes FROM messages m
             JOIN object_generations g ON g.id = m.body_object_generation_id WHERE m.id = $1",
        )
        .bind(message)
        .fetch_one(store.pool())
        .await
        .unwrap();
        assert_eq!(state, ("referenced".into(), 26 * 1024 * 1024 + 1));
        let queued: (i64,) =
            sqlx::query_as("SELECT count(*) FROM object_deletion_queue WHERE generation_id = $1")
                .bind(record.generation_id)
                .fetch_one(store.pool())
                .await
                .unwrap();
        assert_eq!(queued.0, 0);
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn active_streams_are_bounded_and_hold_capacity_until_completion() {
    isolation::with_store(|store| async move {
        let (message, _, _) = recorded_message(&store).await;
        let (gateway, release, object_task) = delayed_storage(Some(1)).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let client = reqwest::Client::new();
        let path = format!("{url}/internal/object/messages/{message}");
        let mut active = Vec::new();
        for _ in 0..4 {
            let response = tokio::time::timeout(
                Duration::from_secs(3),
                client.get(&path).bearer_auth(API).send(),
            )
            .await
            .expect("response waited for the whole body")
            .unwrap();
            assert_eq!(response.status(), StatusCode::OK);
            active.push(response);
        }
        let response = client.get(&path).bearer_auth(API).send().await.unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        drop(active.pop());
        let replacement = tokio::time::timeout(Duration::from_secs(5), async {
            loop {
                let response = client.get(&path).bearer_auth(API).send().await.unwrap();
                if response.status() == StatusCode::OK {
                    break response;
                }
                assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
                tokio::time::sleep(Duration::from_millis(50)).await;
            }
        })
        .await
        .expect("disconnected stream did not release its capacity");
        active.push(replacement);
        release.send(true).unwrap();
        for response in active {
            assert_eq!(response.bytes().await.unwrap(), "x");
        }
        let response = client.get(&path).bearer_auth(API).send().await.unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        assert_eq!(response.bytes().await.unwrap(), "x");
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn stalled_storage_headers_timeout_and_release_all_capacity() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (arrived, mut arrivals) = mpsc::unbounded_channel();
        let (release, receiver) = watch::channel(false);
        let app = Router::new().route(
            "/mail/*key",
            get(move || {
                let arrived = arrived.clone();
                let mut receiver = receiver.clone();
                async move {
                    arrived.send(()).unwrap();
                    while !*receiver.borrow_and_update() {
                        if receiver.changed().await.is_err() {
                            break;
                        }
                    }
                    ([("x-amz-version-id", VERSION)], "x")
                }
            }),
        );
        let (gateway, object_task) = storage_for_app(app).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let client = reqwest::Client::new();
        let path = format!("{url}/internal/object/messages/{message}");
        let mut requests = Vec::new();
        for _ in 0..4 {
            let request = client.get(&path).bearer_auth(API);
            requests.push(tokio::spawn(async move { request.send().await.unwrap() }));
        }
        for _ in 0..4 {
            tokio::time::timeout(Duration::from_secs(5), arrivals.recv())
                .await
                .expect("request did not reach storage")
                .expect("storage stopped");
        }
        let full = client.get(&path).bearer_auth(API).send().await.unwrap();
        assert_eq!(full.status(), StatusCode::SERVICE_UNAVAILABLE);
        let responses = tokio::time::timeout(
            Duration::from_secs(35),
            futures_util::future::join_all(requests),
        )
        .await
        .expect("stalled requests retained capacity indefinitely");
        for response in responses {
            let response = response.unwrap();
            assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
            let text = response.text().await.unwrap();
            assert!(text.is_empty());
            for value in [&key, &generation, VERSION, BODY, API] {
                assert!(!text.contains(value));
            }
        }
        release.send(true).unwrap();
        let response = tokio::time::timeout(
            Duration::from_secs(5),
            client.get(&path).bearer_auth(API).send(),
        )
        .await
        .expect("capacity was not released")
        .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        assert_eq!(response.bytes().await.unwrap(), "x");
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn stalled_storage_body_times_out_as_incomplete() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (gateway, _release, object_task) = delayed_storage(Some(1)).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let response = reqwest::Client::new()
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        let result = tokio::time::timeout(Duration::from_secs(35), response.bytes())
            .await
            .expect("stalled body was not timed out");
        let error = result
            .expect_err("stalled body was reported complete")
            .to_string();
        for value in [&key, &generation, VERSION, BODY, API] {
            assert!(!error.contains(value));
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn unconsumed_responses_do_not_hold_read_capacity_after_deadline() {
    isolation::with_store(|store| async move {
        let (message, _, _) = recorded_message(&store).await;
        let app = Router::new().route(
            "/mail/*key",
            get(|| async {
                let chunks = futures_util::stream::iter(
                    (0..416).map(|_| Ok::<_, io::Error>(vec![7u8; 65536])),
                );
                (
                    [
                        ("x-amz-version-id", VERSION),
                        ("content-length", "27262976"),
                    ],
                    axum::body::Body::from_stream(chunks),
                )
            }),
        );
        let (gateway, object_task) = storage_for_app(app).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let client = reqwest::Client::new();
        let path = format!("{url}/internal/object/messages/{message}");
        let mut active = Vec::new();
        for _ in 0..4 {
            let response = client.get(&path).bearer_auth(API).send().await.unwrap();
            assert_eq!(response.status(), StatusCode::OK);
            active.push(response);
        }
        let full = client.get(&path).bearer_auth(API).send().await.unwrap();
        assert_eq!(full.status(), StatusCode::SERVICE_UNAVAILABLE);
        tokio::time::sleep(Duration::from_secs(32)).await;
        let response = client.get(&path).bearer_auth(API).send().await.unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        drop(response);
        for response in active {
            let result = tokio::time::timeout(Duration::from_secs(5), response.bytes())
                .await
                .expect("expired response did not terminate");
            assert!(result.is_err(), "expired response was reported complete");
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test(flavor = "multi_thread", worker_threads = 4)]
async fn concurrent_large_streams_preserve_every_byte_in_order() {
    isolation::with_store(|store| async move {
        let (message, _, _) = recorded_message(&store).await;
        let app = Router::new().route(
            "/mail/*key",
            get(|| async {
                let chunks = futures_util::stream::iter(
                    (0..256).map(|i| Ok::<_, io::Error>(vec![(i % 251) as u8; 65536])),
                );
                (
                    [
                        ("x-amz-version-id", VERSION),
                        ("content-length", "16777216"),
                    ],
                    axum::body::Body::from_stream(chunks),
                )
            }),
        );
        let (gateway, object_task) = storage_for_app(app).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let client = reqwest::Client::new();
        let path = format!("{url}/internal/object/messages/{message}");
        let mut tasks = Vec::new();
        for _ in 0..4 {
            let request = client.get(&path).bearer_auth(API);
            tasks.push(tokio::spawn(async move {
                let mut response = request.send().await.unwrap();
                assert_eq!(response.status(), StatusCode::OK);
                let mut offset = 0usize;
                while let Some(chunk) = response.chunk().await.unwrap() {
                    for byte in chunk {
                        assert_eq!(byte, ((offset / 65536) % 251) as u8);
                        offset += 1;
                    }
                }
                assert_eq!(offset, 256 * 65536);
            }));
        }
        for task in tasks {
            task.await.unwrap();
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn unknown_storage_body_size_is_rejected_without_buffering() {
    isolation::with_store(|store| async move {
        let (message, _, _) = recorded_message(&store).await;
        let (gateway, _release, object_task) = delayed_storage(None).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let response = tokio::time::timeout(
            Duration::from_secs(3),
            reqwest::Client::new()
                .get(format!("{url}/internal/object/messages/{message}"))
                .bearer_auth(API)
                .send(),
        )
        .await
        .expect("unknown-size response was buffered")
        .unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        assert!(response.text().await.unwrap().is_empty());
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn checksum_failure_after_body_is_detectably_incomplete() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (gateway, object_task) = checksum_storage(BODY).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let response = reqwest::Client::new()
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await;
        let error = match response {
            Ok(response) => {
                assert_eq!(response.status(), StatusCode::OK);
                let result = response.bytes().await;
                assert!(result.is_err(), "corrupt body was reported complete");
                result.unwrap_err().to_string()
            }
            Err(error) => error.to_string(),
        };
        for value in [&key, &generation, VERSION, BODY, API] {
            assert!(!error.contains(value));
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn checksum_failure_after_headers_truncates_multi_chunk_response() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (release, receiver) = watch::channel(false);
        let app = Router::new().route(
            "/mail/*key",
            get(move || {
                let mut receiver = receiver.clone();
                async move {
                    let first =
                        futures_util::stream::once(async { Ok::<_, io::Error>(vec![b'x'; 65536]) });
                    let last = futures_util::stream::once(async move {
                        while !*receiver.borrow_and_update() {
                            if receiver.changed().await.is_err() {
                                return Err(io::Error::other("storage stopped"));
                            }
                        }
                        Ok(vec![b'y'])
                    });
                    (
                        [
                            ("x-amz-version-id", VERSION),
                            ("x-amz-checksum-crc32", "AQIDBA=="),
                            ("content-length", "65537"),
                        ],
                        axum::body::Body::from_stream(first.chain(last)),
                    )
                }
            }),
        );
        let (gateway, object_task) = storage_for_app(app).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let mut response = reqwest::Client::new()
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        let first = response.chunk().await.unwrap().unwrap();
        assert!(!first.is_empty());
        assert!(first.iter().all(|byte| *byte == b'x'));
        release.send(true).unwrap();
        let error = response.bytes().await.unwrap_err().to_string();
        for value in [&key, &generation, VERSION, BODY, API] {
            assert!(!error.contains(value));
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn checksum_failure_for_empty_body_returns_unavailable() {
    isolation::with_store(|store| async move {
        let (message, _, _) = recorded_message(&store).await;
        let (gateway, object_task) = checksum_storage("").await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let response = reqwest::Client::new()
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        assert!(response.text().await.unwrap().is_empty());
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn composite_checksum_returns_sanitized_unavailable() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let app = Router::new().route(
            "/mail/*key",
            get(|| async {
                (
                    [
                        ("x-amz-version-id", VERSION),
                        ("x-amz-checksum-type", "COMPOSITE"),
                        ("x-amz-checksum-crc32", "AQIDBA==-2"),
                    ],
                    BODY,
                )
            }),
        );
        let (gateway, object_task) = storage_for_app(app).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let response = reqwest::Client::new()
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        let error = response.text().await.unwrap();
        assert!(error.is_empty());
        for value in [&key, &generation, VERSION, BODY, API] {
            assert!(!error.contains(value));
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn non_utf8_checksum_evidence_returns_sanitized_unavailable() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let app = Router::new().route(
            "/mail/*key",
            get(|| async {
                let mut headers = axum::http::HeaderMap::new();
                headers.insert("x-amz-version-id", VERSION.parse().unwrap());
                headers.insert(
                    "content-md5",
                    axum::http::HeaderValue::from_bytes(b"invalid-\xff-canary").unwrap(),
                );
                (headers, BODY)
            }),
        );
        let (gateway, object_task) = storage_for_app(app).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let response = reqwest::Client::new()
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        let error = response.text().await.unwrap();
        assert!(error.is_empty());
        for value in [&key, &generation, VERSION, BODY, API, "invalid-"] {
            assert!(!error.contains(value));
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn truncated_storage_stream_aborts_the_response_without_exposing_evidence() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (gateway, release, object_task) = delayed_storage(Some(2)).await;
        let (url, service_task) =
            serve(object_service::router(store, gateway, config()), None).await;
        let response = reqwest::Client::new()
            .get(format!("{url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        release.send(true).unwrap();
        let error = response.bytes().await.unwrap_err().to_string();
        for value in [&key, &generation, VERSION, BODY, API] {
            assert!(!error.contains(value));
        }
        service_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn service_rejects_bad_credentials_and_untrusted_inputs_without_evidence() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (gateway, object_task) = object_server().await;
        let app = object_service::router(store, gateway, config());
        let (url, service_task) = serve(app.clone(), None).await;
        let client = reqwest::Client::new();
        let path = format!("{url}/internal/object/messages/{message}");
        for auth in [None, Some("Bearer wrong"), Some("Basic api-test-secret")] {
            let mut request = client.get(&path);
            if let Some(auth) = auth {
                request = request.header("authorization", auth);
            }
            let response = request.send().await.unwrap();
            assert_eq!(response.status(), StatusCode::UNAUTHORIZED);
            let error = response.text().await.unwrap();
            for secret in [&key, &generation, VERSION, BODY, API] {
                assert!(!error.contains(secret));
            }
        }
        let response = client
            .get(&path)
            .header("authorization", format!("Bearer {API}"))
            .header("authorization", format!("Bearer {IMAP}"))
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::UNAUTHORIZED);
        for request in [
            client.get(format!("{path}?key={key}")).bearer_auth(API),
            client.get(&path).bearer_auth(API).body(BODY),
            client
                .get(format!("{url}/internal/object/messages/{VERSION}"))
                .bearer_auth(API),
        ] {
            let response = request.send().await.unwrap();
            assert_eq!(response.status(), StatusCode::BAD_REQUEST);
            let error = response.text().await.unwrap();
            for secret in [&key, &generation, VERSION, BODY, API] {
                assert!(!error.contains(secret));
            }
        }
        let (remote_url, remote_task) =
            serve(app.clone(), Some("192.0.2.1:1234".parse().unwrap())).await;
        let response = client
            .get(format!("{remote_url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::FORBIDDEN);
        let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let no_peer_url = format!("http://{}", listener.local_addr().unwrap());
        let no_peer_task = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
        let response = client
            .get(format!("{no_peer_url}/internal/object/messages/{message}"))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::FORBIDDEN);
        service_task.abort();
        remote_task.abort();
        no_peer_task.abort();
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn listener_binds_only_after_validating_loopback_and_all_secrets() {
    let listener = object_service::bind(&config()).await.unwrap();
    assert!(listener.local_addr().unwrap().ip().is_loopback());
    let mut invalid = config();
    invalid.bind = "0.0.0.0:0".parse().unwrap();
    assert!(object_service::bind(&invalid).await.is_err());
    invalid.bind = "127.0.0.1:0".parse().unwrap();
    invalid.outbound_token = String::new().into();
    assert!(object_service::bind(&invalid).await.is_err());
}

#[derive(Clone)]
struct Capture(Arc<Mutex<Vec<u8>>>);

impl Write for Capture {
    fn write(&mut self, data: &[u8]) -> io::Result<usize> {
        self.0.lock().unwrap().extend_from_slice(data);
        Ok(data.len())
    }

    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

#[tokio::test]
async fn gateway_never_logs_object_evidence_even_when_trace_is_enabled() {
    let (gateway, object_task) = object_server().await;
    let generation = Uuid::new_v4();
    let record = RecordedExternalBody {
        generation_id: generation,
        object_key: "sensitive-key-canary".into(),
        version_id: VERSION.into(),
    };
    let output = Capture(Arc::new(Mutex::new(Vec::new())));
    let writer = output.clone();
    let subscriber = tracing_subscriber::fmt()
        .with_max_level(tracing::Level::TRACE)
        .with_ansi(false)
        .with_writer(move || writer.clone())
        .finish();
    async {
        tracing::info!("capture active");
        let body = gateway.read_version(&record).await.unwrap();
        let body = axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024)
            .await
            .unwrap();
        assert_eq!(body, "recorded-v1");
    }
    .with_subscriber(subscriber)
    .await;
    let output = String::from_utf8(output.0.lock().unwrap().clone()).unwrap();
    assert!(output.contains("capture active"));
    for (name, value) in [
        ("key", record.object_key.as_str()),
        ("version", VERSION),
        ("generation", &generation.to_string()),
        ("body", "recorded-v1"),
    ] {
        assert!(!output.contains(value), "{name} leaked in trace output");
    }
    object_task.abort();
}

#[tokio::test]
async fn gateway_never_logs_object_deletion_evidence_on_storage_failure() {
    let (gateway, object_task) = object_server().await;
    let record = RecordedExternalBody {
        generation_id: Uuid::new_v4(),
        object_key: "sensitive-delete-key-canary".into(),
        version_id: VERSION.into(),
    };
    let output = Capture(Arc::new(Mutex::new(Vec::new())));
    let writer = output.clone();
    let subscriber = tracing_subscriber::fmt()
        .with_max_level(tracing::Level::TRACE)
        .with_ansi(false)
        .with_writer(move || writer.clone())
        .finish();
    async {
        tracing::info!("capture active");
        assert!(gateway.delete_version(&record).await.is_err());
    }
    .with_subscriber(subscriber)
    .await;
    let output = String::from_utf8(output.0.lock().unwrap().clone()).unwrap();
    assert!(output.contains("capture active"));
    assert!(
        !output.contains(&record.object_key),
        "key leaked in trace output"
    );
    assert!(!output.contains(VERSION), "version leaked in trace output");
    object_task.abort();
}

struct NodeKeyFile(PathBuf);

impl Drop for NodeKeyFile {
    fn drop(&mut self) {
        let _ = std::fs::remove_file(&self.0);
    }
}

fn daemon_command(
    db_url: &str,
    key_path: &Path,
    object_address: SocketAddr,
    service_bind: &str,
    mx_bind: &str,
) -> Command {
    let mut command = Command::new(env!("CARGO_BIN_EXE_nexus-mailsmtpd"));
    command
        .env_clear()
        .env("TMPDIR", std::env::current_dir().unwrap())
        .env("NEXUS_EMAIL_DATABASE_URL", db_url)
        .env("NEXUS_EMAIL_NODE_KEY_PATH", key_path)
        .env("NEXUS_EMAIL_MX_BIND", mx_bind)
        .env("NEXUS_EMAIL_SUBMISSION_BIND", "127.0.0.1:0")
        .env("NEXUS_EMAIL_IMAP_BIND", "127.0.0.1:0")
        .env("NEXUS_EMAIL_FEDERATION_BIND", "127.0.0.1:0")
        .env("NEXUS_EMAIL_EGRESS", "direct")
        .env("NEXUS_EMAIL_OBJECT_SERVICE_BIND", service_bind)
        .env("NEXUS_EMAIL_OBJECT_SERVICE_API_TOKEN", API)
        .env("NEXUS_EMAIL_OBJECT_SERVICE_IMAP_TOKEN", IMAP)
        .env("NEXUS_EMAIL_OBJECT_SERVICE_OUTBOUND_TOKEN", OUTBOUND)
        .env("S3_ENDPOINT", format!("http://{object_address}"))
        .env("S3_ACCESS_KEY", "test")
        .env("S3_SECRET_KEY", "test")
        .env("S3_BUCKET", "mail")
        .env("RUST_LOG", "trace")
        .kill_on_drop(true);
    command
}

async fn wait_for_listener(address: SocketAddr, daemon: &mut tokio::process::Child) {
    tokio::time::timeout(Duration::from_secs(30), async {
        loop {
            if tokio::net::TcpStream::connect(address).await.is_ok() {
                break;
            }
            assert!(
                daemon.try_wait().unwrap().is_none(),
                "daemon exited before startup"
            );
            tokio::time::sleep(Duration::from_millis(50)).await;
        }
    })
    .await
    .expect("listener did not start");
}

#[tokio::test]
async fn daemon_hosts_service_without_leaking_evidence_in_trace_logs() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let schema: String = sqlx::query_scalar("SELECT current_schema()")
            .fetch_one(store.pool())
            .await
            .unwrap();
        let mut db_url =
            reqwest::Url::parse(&std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL").unwrap()).unwrap();
        db_url
            .query_pairs_mut()
            .append_pair("options", &format!("-csearch_path={schema}"));
        let object_app = Router::new().route(
            "/mail/*key",
            get(|Query(query): Query<HashMap<String, String>>| async move {
                if query.get("versionId").map(String::as_str) == Some(VERSION) {
                    (StatusCode::OK, [("x-amz-version-id", VERSION)], BODY)
                } else {
                    (StatusCode::NOT_FOUND, [("x-amz-version-id", "wrong")], "")
                }
            }),
        );
        let object_listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let object_address = object_listener.local_addr().unwrap();
        let object_task =
            tokio::spawn(async move { axum::serve(object_listener, object_app).await.unwrap() });

        let reserved = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let service_address = reserved.local_addr().unwrap();
        drop(reserved);
        let node_key = NodeKeyFile(PathBuf::from(format!(".test-node-key-{}", Uuid::new_v4())));
        let mut daemon = daemon_command(
            db_url.as_str(),
            &node_key.0,
            object_address,
            &service_address.to_string(),
            "127.0.0.1:0",
        )
        .env(
            "S3_ENDPOINT",
            format!("http://localhost:{}", object_address.port()),
        )
        .stdout(Stdio::piped())
        .stderr(Stdio::piped())
        .spawn()
        .unwrap();
        let mut stdout = daemon.stdout.take().unwrap();
        let mut stderr = daemon.stderr.take().unwrap();
        let logs = tokio::spawn(async move {
            let (mut out, mut err) = (Vec::new(), Vec::new());
            let (out_result, err_result) =
                tokio::join!(stdout.read_to_end(&mut out), stderr.read_to_end(&mut err));
            out_result.unwrap();
            err_result.unwrap();
            out.extend(err);
            String::from_utf8_lossy(&out).into_owned()
        });
        wait_for_listener(service_address, &mut daemon).await;
        let client = reqwest::Client::new();
        let response = client
            .get(format!(
                "http://{service_address}/internal/object/messages/{message}"
            ))
            .bearer_auth(API)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::OK);
        assert_eq!(response.bytes().await.unwrap(), BODY);
        let response = client
            .get(format!(
                "http://{service_address}/internal/object/messages/{message}"
            ))
            .query(&[("key", &key), ("generation", &generation)])
            .bearer_auth(API)
            .body(BODY)
            .send()
            .await
            .unwrap();
        assert_eq!(response.status(), StatusCode::BAD_REQUEST);
        daemon.kill().await.unwrap();
        daemon.wait().await.unwrap();
        let logs = logs.await.unwrap();
        assert!(logs.contains("nexus-mailsmtpd listening"));
        for (name, value) in [
            ("key", key.as_str()),
            ("generation", generation.as_str()),
            ("version", VERSION),
            ("body", BODY),
            ("API token", API),
            ("IMAP token", IMAP),
            ("outbound token", OUTBOUND),
        ] {
            assert!(!logs.contains(value), "{name} leaked in daemon logs");
        }
        object_task.abort();
    })
    .await;
}

#[tokio::test]
async fn daemon_leaves_object_service_disabled_when_configuration_is_unsafe() {
    isolation::with_store(|store| async move {
        let schema: String = sqlx::query_scalar("SELECT current_schema()")
            .fetch_one(store.pool())
            .await
            .unwrap();
        let mut db_url =
            reqwest::Url::parse(&std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL").unwrap()).unwrap();
        db_url
            .query_pairs_mut()
            .append_pair("options", &format!("-csearch_path={schema}"));
        for case in ["missing-secret", "non-loopback", "missing-storage"] {
            let reserve_service = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let service_address = reserve_service.local_addr().unwrap();
            let reserve_mx = TcpListener::bind("127.0.0.1:0").await.unwrap();
            let mx_address = reserve_mx.local_addr().unwrap();
            drop((reserve_service, reserve_mx));
            let bind = if case == "non-loopback" {
                format!("0.0.0.0:{}", service_address.port())
            } else {
                service_address.to_string()
            };
            let node_key = NodeKeyFile(PathBuf::from(format!(".test-node-key-{}", Uuid::new_v4())));
            let mut command = daemon_command(
                db_url.as_str(),
                &node_key.0,
                "127.0.0.1:1".parse().unwrap(),
                &bind,
                &mx_address.to_string(),
            );
            match case {
                "missing-secret" => {
                    command.env_remove("NEXUS_EMAIL_OBJECT_SERVICE_IMAP_TOKEN");
                }
                "missing-storage" => {
                    command.env_remove("S3_SECRET_KEY");
                }
                _ => {}
            }
            let mut daemon = command
                .env("RUST_LOG", "info")
                .stdout(Stdio::piped())
                .stderr(Stdio::null())
                .spawn()
                .unwrap();
            wait_for_listener(mx_address, &mut daemon).await;
            let mut lines = BufReader::new(daemon.stdout.take().unwrap()).lines();
            let ready = tokio::time::timeout(Duration::from_secs(30), async {
                while let Some(line) = lines.next_line().await.unwrap() {
                    if line.contains("nexus-mailsmtpd listening") {
                        return true;
                    }
                }
                false
            })
            .await
            .unwrap();
            assert!(ready, "daemon exited before completing startup");
            assert!(
                tokio::net::TcpStream::connect(service_address)
                    .await
                    .is_err(),
                "{case} unexpectedly enabled the object service"
            );
            daemon.kill().await.unwrap();
            daemon.wait().await.unwrap();
        }
    })
    .await;
}

#[tokio::test]
async fn occupied_object_listener_does_not_stop_smtp_or_imap() {
    isolation::with_store(|store| async move {
        let schema: String = sqlx::query_scalar("SELECT current_schema()")
            .fetch_one(store.pool())
            .await
            .unwrap();
        let mut db_url =
            reqwest::Url::parse(&std::env::var("NEXUS_EMAIL_TEST_DATABASE_URL").unwrap()).unwrap();
        db_url
            .query_pairs_mut()
            .append_pair("options", &format!("-csearch_path={schema}"));
        let occupied = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let object_bind = occupied.local_addr().unwrap();
        let mx = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let mx_address = mx.local_addr().unwrap();
        let imap = TcpListener::bind("127.0.0.1:0").await.unwrap();
        let imap_address = imap.local_addr().unwrap();
        drop((mx, imap));
        let node_key = NodeKeyFile(PathBuf::from(format!(".test-node-key-{}", Uuid::new_v4())));
        let mut daemon = daemon_command(
            db_url.as_str(),
            &node_key.0,
            "127.0.0.1:1".parse().unwrap(),
            &object_bind.to_string(),
            &mx_address.to_string(),
        )
        .env("NEXUS_EMAIL_IMAP_BIND", imap_address.to_string())
        .env("RUST_LOG", "info")
        .stdout(Stdio::piped())
        .stderr(Stdio::null())
        .spawn()
        .unwrap();
        wait_for_listener(mx_address, &mut daemon).await;
        wait_for_listener(imap_address, &mut daemon).await;
        let mut lines = BufReader::new(daemon.stdout.take().unwrap()).lines();
        let mut logs = String::new();
        tokio::time::timeout(Duration::from_secs(30), async {
            while let Some(line) = lines.next_line().await.unwrap() {
                logs.push_str(&line);
                if line.contains("nexus-mailsmtpd listening") {
                    return;
                }
            }
            panic!("daemon exited during startup");
        })
        .await
        .unwrap();
        assert!(logs.contains("object service disabled"));
        for secret in [API, IMAP, OUTBOUND] {
            assert!(!logs.contains(secret));
        }
        for (address, prefix) in [(mx_address, "220"), (imap_address, "* OK")] {
            let socket = tokio::net::TcpStream::connect(address).await.unwrap();
            let mut reader = BufReader::new(socket);
            let mut greeting = String::new();
            reader.read_line(&mut greeting).await.unwrap();
            assert!(greeting.starts_with(prefix));
        }
        daemon.kill().await.unwrap();
        daemon.wait().await.unwrap();
        drop(occupied);
    })
    .await;
}

#[tokio::test]
async fn storage_and_database_failures_return_unavailable_without_evidence() {
    isolation::with_store(|store| async move {
        let (message, key, generation) = recorded_message(&store).await;
        let (gateway, object_task) = object_server().await;
        let (url, service_task) = serve(
            object_service::router(store.clone(), gateway, config()),
            None,
        )
        .await;
        object_task.abort();
        let client = reqwest::Client::new();
        let path = format!("{url}/internal/object/messages/{message}");
        let response = client.get(&path).bearer_auth(API).send().await.unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        let error = response.text().await.unwrap();
        for secret in [&key, &generation, VERSION, BODY, API] {
            assert!(!error.contains(secret));
        }
        store.pool().close().await;
        let response = client.get(&path).bearer_auth(API).send().await.unwrap();
        assert_eq!(response.status(), StatusCode::SERVICE_UNAVAILABLE);
        assert!(response.text().await.unwrap().is_empty());
        service_task.abort();
    })
    .await;
}

#[test]
fn service_configuration_fails_closed_for_partial_invalid_or_nonlocal_settings() {
    assert!(ObjectServiceConfig::from_settings(None, None, None, None)
        .unwrap()
        .is_none());
    for (bind, api, imap, outbound) in [
        (Some("127.0.0.1:3000"), Some(API), None, Some(OUTBOUND)),
        (Some("0.0.0.0:3000"), Some(API), Some(IMAP), Some(OUTBOUND)),
        (Some("[::]:3000"), Some(API), Some(IMAP), Some(OUTBOUND)),
        (Some("127.0.0.1:3000"), Some(API), Some(API), Some(OUTBOUND)),
        (Some("127.0.0.1:3000"), Some(API), Some(""), Some(OUTBOUND)),
        (
            Some("127.0.0.1:3000"),
            Some(API),
            Some("contains space"),
            Some(OUTBOUND),
        ),
    ] {
        assert!(ObjectServiceConfig::from_settings(bind, api, imap, outbound).is_err());
    }
    let valid = ObjectServiceConfig::from_settings(
        Some("[::1]:3000"),
        Some(API),
        Some(IMAP),
        Some(OUTBOUND),
    )
    .unwrap()
    .unwrap();
    let debug = format!("{valid:?}");
    for secret in [API, IMAP, OUTBOUND] {
        assert!(!debug.contains(secret));
    }
}
