use std::collections::HashMap;

use axum::{extract::Query, http::StatusCode, routing::get, Router};
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
                "old"
            } else {
                "new"
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

    assert_eq!(gateway.read_version(&record).await.unwrap(), b"old");
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
