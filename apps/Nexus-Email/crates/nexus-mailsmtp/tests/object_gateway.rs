use std::{
    collections::HashMap,
    panic::AssertUnwindSafe,
    sync::{
        atomic::{AtomicBool, Ordering},
        Arc,
    },
};

use aws_smithy_runtime_api::client::{
    http::{http_client_fn, HttpConnector, HttpConnectorFuture, SharedHttpConnector},
    orchestrator::{HttpRequest, HttpResponse},
};
use axum::{
    body::Bytes,
    extract::{Path, Query},
    http::{HeaderMap, StatusCode},
    routing::get,
    Router,
};
use futures_util::{stream, FutureExt, StreamExt};
use http_body_util::{BodyExt, Full};
use nexus_mailsmtp::object_gateway::S3BodyGateway;
use nexus_mailstore::RecordedExternalBody;
use tokio::net::TcpListener;
use uuid::Uuid;

#[tokio::test]
async fn gateway_reads_only_the_recorded_version_for_a_message() {
    let app = Router::new().route(
        "/mail/key",
        get(|Query(query): Query<HashMap<String, String>>| async move {
            if query.get("versionId").map(String::as_str) == Some("v1") {
                ([("x-amz-version-id", "v1")], "old")
            } else {
                ([("x-amz-version-id", "v2")], "new")
            }
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    let record = RecordedExternalBody {
        generation_id: Uuid::new_v4(),
        object_key: "key".into(),
        version_id: "v1".into(),
    };

    let body = gateway.read_version(&record).await.unwrap();
    let body = axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024)
        .await
        .unwrap();
    assert_eq!(body, "old");
    server.abort();
}

#[tokio::test]
async fn gateway_rejects_missing_or_mismatched_response_version() {
    let app = Router::new()
        .route("/mail/missing", get(|| async { "wrong body" }))
        .route(
            "/mail/mismatched",
            get(|| async { ([("x-amz-version-id", "other")], "wrong body") }),
        );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    for key in ["missing", "mismatched"] {
        let record = RecordedExternalBody {
            generation_id: Uuid::new_v4(),
            object_key: key.into(),
            version_id: "recorded-version-canary".into(),
        };
        let error = gateway
            .read_version(&record)
            .await
            .expect_err("must reject");
        let output = format!("{error} {error:?}");
        assert!(!output.contains(key));
        assert!(!output.contains("recorded-version-canary"));
        assert!(!output.contains("wrong body"));
    }
    server.abort();
}

#[tokio::test]
async fn gateway_checks_storage_checksum_after_final_body_chunk() {
    let app = Router::new().route(
        "/mail/key-canary",
        get(|| async {
            (
                [
                    ("x-amz-version-id", "v1"),
                    ("x-amz-checksum-crc32", "AQIDBA=="),
                ],
                "checksum-body-canary",
            )
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    let record = RecordedExternalBody {
        generation_id: Uuid::new_v4(),
        object_key: "key-canary".into(),
        version_id: "v1".into(),
    };
    let body = gateway.read_version(&record).await.unwrap();
    let result =
        axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024).await;
    assert!(
        result.is_err(),
        "checksum mismatch must not complete successfully"
    );
    let error = result.unwrap_err().to_string();
    assert!(!error.contains("checksum-body-canary"));
    assert!(!error.contains("key-canary"));
    server.abort();
}

#[tokio::test]
async fn gateway_enables_validation_even_when_client_config_says_when_required() {
    let enabled = Arc::new(AtomicBool::new(false));
    let observed = enabled.clone();
    let app = Router::new()
        .route(
            "/mail/key-canary",
            get(move |headers: HeaderMap| {
                let observed = observed.clone();
                async move {
                    observed.store(
                        headers
                            .get("x-amz-checksum-mode")
                            .is_some_and(|value| value == "ENABLED"),
                        Ordering::SeqCst,
                    );
                    (
                        [
                            ("x-amz-version-id", "v1"),
                            ("x-amz-checksum-crc32", "AQIDBA=="),
                        ],
                        "checksum-body-canary",
                    )
                }
            }),
        )
        .route(
            "/mail/valid",
            get(|| async {
                (
                    [
                        ("x-amz-version-id", "v1"),
                        ("x-amz-checksum-type", "FULL_OBJECT"),
                        ("x-amz-checksum-crc32", "o40Npg=="),
                    ],
                    "checksum-body-canary",
                )
            }),
        );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .response_checksum_validation(aws_sdk_s3::config::ResponseChecksumValidation::WhenRequired)
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    let record = RecordedExternalBody {
        generation_id: Uuid::new_v4(),
        object_key: "key-canary".into(),
        version_id: "v1".into(),
    };
    let body = gateway.read_version(&record).await.unwrap();
    let result =
        axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024).await;
    assert!(result.is_err(), "corrupt checksum was ignored");
    assert!(
        enabled.load(Ordering::SeqCst),
        "checksum mode was not enabled"
    );
    let valid = RecordedExternalBody {
        object_key: "valid".into(),
        ..record
    };
    let body = gateway.read_version(&valid).await.unwrap();
    assert_eq!(
        axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024)
            .await
            .unwrap(),
        "checksum-body-canary"
    );
    server.abort();
}

#[tokio::test]
async fn gateway_rejects_unverifiable_checksum_evidence() {
    let app = Router::new().route(
        "/mail/*key",
        get(|Path(key): Path<String>| async move {
            let mut headers = HeaderMap::new();
            headers.insert("x-amz-version-id", "v1".parse().unwrap());
            match key.as_str() {
                "composite-suffix" => {
                    headers.insert("x-amz-checksum-crc32", "o40Npg==-2".parse().unwrap());
                }
                "composite-type" => {
                    headers.insert("x-amz-checksum-type", "COMPOSITE".parse().unwrap());
                    headers.insert("x-amz-checksum-crc32", "o40Npg==".parse().unwrap());
                }
                "unknown-type" => {
                    headers.insert("x-amz-checksum-type", "FUTURE".parse().unwrap());
                    headers.insert("x-amz-checksum-crc32", "o40Npg==".parse().unwrap());
                }
                "malformed" => {
                    headers.insert("x-amz-checksum-crc32", "not-base64".parse().unwrap());
                }
                "unsupported" => {
                    headers.insert("x-amz-checksum-sha512", "AQIDBA==".parse().unwrap());
                }
                "unknown-algorithm" => {
                    headers.insert("x-amz-checksum-future", "AQIDBA==".parse().unwrap());
                }
                "multiple-algorithms" => {
                    headers.insert("x-amz-checksum-crc32", "o40Npg==".parse().unwrap());
                    headers.insert(
                        "x-amz-checksum-sha256",
                        "fBDkEvy4Cayb3AQ2moKv9WK6T62xsD2J7+1C5BVmQwU="
                            .parse()
                            .unwrap(),
                    );
                }
                "duplicate-algorithm" => {
                    headers.append("x-amz-checksum-crc32", "o40Npg==".parse().unwrap());
                    headers.append("x-amz-checksum-crc32", "AQIDBA==".parse().unwrap());
                }
                "checksum-trailer" => {
                    headers.insert("trailer", "x-amz-checksum-crc32".parse().unwrap());
                }
                "content-md5" => {
                    headers.insert("content-md5", "AQIDBA==".parse().unwrap());
                }
                _ => unreachable!(),
            }
            (headers, "checksum-body-canary")
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    let mut accepted = Vec::new();
    for key in [
        "composite-suffix",
        "composite-type",
        "unknown-type",
        "malformed",
        "unsupported",
        "unknown-algorithm",
        "multiple-algorithms",
        "duplicate-algorithm",
        "checksum-trailer",
        "content-md5",
    ] {
        let record = RecordedExternalBody {
            generation_id: Uuid::new_v4(),
            object_key: key.into(),
            version_id: "v1".into(),
        };
        let result = match gateway.read_version(&record).await {
            Ok(body) => {
                axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024)
                    .await
                    .map(|_| ())
                    .map_err(|error| error.to_string())
            }
            Err(error) => Err(error.to_string()),
        };
        match result {
            Ok(()) => accepted.push(key),
            Err(error) => {
                assert!(!error.contains(key));
                assert!(!error.contains("checksum-body-canary"));
            }
        }
    }
    assert!(
        accepted.is_empty(),
        "unverifiable checksums accepted: {accepted:?}"
    );
    server.abort();
}

#[tokio::test]
async fn gateway_rejects_non_utf8_checksum_evidence() {
    let app = Router::new().route(
        "/mail/*key",
        get(|Path(key): Path<String>| async move {
            let mut headers = HeaderMap::new();
            headers.insert("x-amz-version-id", "v1".parse().unwrap());
            let invalid = axum::http::HeaderValue::from_bytes(b"invalid-\xff-canary").unwrap();
            let name = match key.as_str() {
                "checksum-type" => "x-amz-checksum-type",
                "checksum-crc32" | "duplicate-checksum" => "x-amz-checksum-crc32",
                "unknown-checksum" => "x-amz-checksum-future",
                "content-md5" => "content-md5",
                "digest" => "digest",
                "content-digest" => "content-digest",
                "repr-digest" => "repr-digest",
                "trailer" => "trailer",
                "x-amz-trailer" => "x-amz-trailer",
                "unrelated" => "x-unrelated",
                _ => unreachable!(),
            };
            if key == "duplicate-checksum" {
                headers.append(name, "o40Npg==".parse().unwrap());
            }
            headers.append(name, invalid);
            (headers, "checksum-body-canary")
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    let mut accepted = Vec::new();
    let mut served = Vec::new();
    for key in [
        "checksum-type",
        "checksum-crc32",
        "duplicate-checksum",
        "unknown-checksum",
        "content-md5",
        "digest",
        "content-digest",
        "repr-digest",
        "trailer",
        "x-amz-trailer",
    ] {
        let record = RecordedExternalBody {
            generation_id: Uuid::new_v4(),
            object_key: key.into(),
            version_id: "v1".into(),
        };
        match gateway.read_version(&record).await {
            Ok(body) => {
                accepted.push(key);
                if axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024)
                    .await
                    .is_ok_and(|bytes| bytes == "checksum-body-canary")
                {
                    served.push(key);
                }
            }
            Err(error) => {
                let output = format!("{error} {error:?}");
                assert!(!output.contains(key));
                assert!(!output.contains("invalid-"));
                assert!(!output.contains("checksum-body-canary"));
            }
        }
    }
    let unrelated = RecordedExternalBody {
        generation_id: Uuid::new_v4(),
        object_key: "unrelated".into(),
        version_id: "v1".into(),
    };
    let body = gateway.read_version(&unrelated).await.unwrap();
    assert_eq!(
        axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024)
            .await
            .unwrap(),
        "checksum-body-canary"
    );
    assert!(
        accepted.is_empty(),
        "non-UTF-8 checksum evidence accepted: {accepted:?}; bodies served: {served:?}"
    );
    server.abort();
}

#[derive(Debug)]
struct TrailerConnector {
    empty: bool,
    checksum_header: bool,
}

impl HttpConnector for TrailerConnector {
    fn call(&self, _request: HttpRequest) -> HttpConnectorFuture {
        let empty = self.empty;
        let checksum_header = self.checksum_header;
        HttpConnectorFuture::new(async move {
            let contents = if empty {
                Bytes::new()
            } else {
                Bytes::from_static(b"checksum-body-canary")
            };
            let mut trailers = HeaderMap::new();
            trailers.insert("x-amz-checksum-crc32", "AQIDBA==".parse().unwrap());
            let source =
                Full::new(contents.clone()).with_trailers(async move { Some(Ok(trailers)) });
            let mut response = axum::http::Response::builder()
                .status(StatusCode::OK)
                .header("x-amz-version-id", "v1")
                .header("content-length", contents.len());
            if checksum_header {
                response = response.header(
                    "x-amz-checksum-crc32",
                    if empty { "AAAAAA==" } else { "o40Npg==" },
                );
            }
            let response = response
                .body(aws_sdk_s3::primitives::SdkBody::from_body_1_x(source))
                .unwrap();
            let response: HttpResponse = response.try_into().unwrap();
            Ok(response)
        })
    }
}

#[tokio::test]
async fn gateway_rejects_unannounced_checksum_trailers_without_panicking() {
    let mut accepted = Vec::new();
    let mut panicked = Vec::new();
    for (empty, checksum_header) in [(false, false), (false, true), (true, false), (true, true)] {
        let http_client = http_client_fn(move |_, _| {
            SharedHttpConnector::new(TrailerConnector {
                empty,
                checksum_header,
            })
        });
        let shared = aws_config::defaults(aws_config::BehaviorVersion::latest())
            .region(aws_config::Region::new("us-east-1"))
            .credentials_provider(aws_credential_types::Credentials::new(
                "test", "test", None, None, "test",
            ))
            .endpoint_url("http://localhost")
            .load()
            .await;
        let config = aws_sdk_s3::config::Builder::from(&shared)
            .http_client(http_client)
            .force_path_style(true)
            .build();
        let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
        let record = RecordedExternalBody {
            generation_id: Uuid::new_v4(),
            object_key: "key-canary".into(),
            version_id: "v1".into(),
        };
        let outcome = AssertUnwindSafe(async {
            match gateway.read_version(&record).await {
                Ok(body) => {
                    axum::body::to_bytes(axum::body::Body::from_stream(body.into_stream()), 1024)
                        .await
                        .map(|_| ())
                        .map_err(|error| error.to_string())
                }
                Err(error) => Err(error.to_string()),
            }
        })
        .catch_unwind()
        .await;
        match outcome {
            Ok(Ok(())) => accepted.push((empty, checksum_header)),
            Ok(Err(error)) => {
                assert!(!error.contains("key-canary"));
                assert!(!error.contains("checksum-body-canary"));
            }
            Err(_) => panicked.push((empty, checksum_header)),
        }
    }
    assert!(
        accepted.is_empty(),
        "unvalidated trailers accepted: {accepted:?}"
    );
    assert!(
        panicked.is_empty(),
        "SDK panicked on trailers: {panicked:?}"
    );
}

#[tokio::test]
async fn gateway_streams_a_maximum_size_body_in_bounded_chunks() {
    let app = Router::new().route(
        "/mail/key",
        get(|| async {
            let chunks = stream::iter((0..416).map(|_| Ok::<_, std::io::Error>(vec![7u8; 65536])));
            (
                [("x-amz-version-id", "v1"), ("content-length", "27262976")],
                axum::body::Body::from_stream(chunks),
            )
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    let record = RecordedExternalBody {
        generation_id: Uuid::new_v4(),
        object_key: "key".into(),
        version_id: "v1".into(),
    };
    let body = gateway.read_version(&record).await.unwrap();
    assert_eq!(body.len(), 26 * 1024 * 1024);
    let mut stream = Box::pin(body.into_stream());
    let mut total = 0;
    while let Some(chunk) = stream.next().await {
        let chunk = chunk.unwrap();
        assert!(chunk.len() <= 65536);
        assert!(chunk.iter().all(|byte| *byte == 7));
        total += chunk.len();
    }
    assert_eq!(total, 26 * 1024 * 1024);
    server.abort();
}

#[tokio::test]
async fn gateway_deletes_only_the_recorded_version() {
    let app = Router::new().route(
        "/mail/key",
        axum::routing::delete(|Query(query): Query<HashMap<String, String>>| async move {
            if query.get("versionId").map(String::as_str) == Some("v1") {
                StatusCode::NO_CONTENT
            } else {
                StatusCode::BAD_REQUEST
            }
        }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    let record = RecordedExternalBody {
        generation_id: Uuid::new_v4(),
        object_key: "key".into(),
        version_id: "v1".into(),
    };

    gateway.delete_version(&record).await.unwrap();
    server.abort();
}

#[tokio::test]
async fn gateway_rejects_unversioned_object_reads_and_deletes() {
    let app = Router::new().route(
        "/mail/key",
        get(|| async { "mutable" }).delete(|| async { StatusCode::NO_CONTENT }),
    );
    let listener = TcpListener::bind("127.0.0.1:0").await.unwrap();
    let address = listener.local_addr().unwrap();
    let server = tokio::spawn(async move { axum::serve(listener, app).await.unwrap() });
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
        .build();
    let gateway = S3BodyGateway::new(aws_sdk_s3::Client::from_conf(config), "mail".into());
    for version_id in ["", "  ", "null"] {
        let record = RecordedExternalBody {
            generation_id: Uuid::new_v4(),
            object_key: "key".into(),
            version_id: version_id.into(),
        };
        assert!(gateway.read_version(&record).await.is_err());
        assert!(gateway.delete_version(&record).await.is_err());
    }
    server.abort();
}
