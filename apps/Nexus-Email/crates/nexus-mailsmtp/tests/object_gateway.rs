use std::collections::HashMap;

use axum::{extract::Query, http::StatusCode, routing::get, Router};
use futures_util::{stream, StreamExt};
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
async fn gateway_streams_a_maximum_size_body_in_bounded_chunks() {
    let app = Router::new().route(
        "/mail/key",
        get(|| async {
            let chunks = stream::iter((0..400).map(|_| Ok::<_, std::io::Error>(vec![7u8; 65536])));
            (
                [("x-amz-version-id", "v1"), ("content-length", "26214400")],
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
    assert_eq!(body.len(), 25 * 1024 * 1024);
    let mut stream = Box::pin(body.into_stream());
    let mut total = 0;
    while let Some(chunk) = stream.next().await {
        let chunk = chunk.unwrap();
        assert!(chunk.len() <= 65536);
        assert!(chunk.iter().all(|byte| *byte == 7));
        total += chunk.len();
    }
    assert_eq!(total, 25 * 1024 * 1024);
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
