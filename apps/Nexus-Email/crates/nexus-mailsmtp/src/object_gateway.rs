use std::{
    fmt,
    pin::Pin,
    task::{Context, Poll},
};

use aws_sdk_s3::{
    config::{
        interceptors::BeforeDeserializationInterceptorContextMut, ConfigBag, Intercept,
        RuntimeComponents,
    },
    error::BoxError,
    primitives::SdkBody,
    types::ChecksumMode,
};
use axum::body::{Bytes, HttpBody};
use base64::{engine::general_purpose::STANDARD, Engine};
use futures_util::{stream, Stream};
use http_body::Frame;
use nexus_mailstore::RecordedExternalBody;
use tokio::io::{AsyncRead, AsyncReadExt};
use tracing::instrument::WithSubscriber;

pub const MAX_EXTERNAL_BODY_BYTES: usize = nexus_mailstore::MAX_EXTERNAL_BODY_BYTES;
const STREAM_CHUNK_BYTES: usize = 64 * 1024;

#[derive(Debug, thiserror::Error)]
#[error("external body storage unavailable")]
pub struct GatewayError;

pub struct VersionedBody {
    reader: Pin<Box<dyn AsyncRead + Send>>,
    length: usize,
}

impl fmt::Debug for VersionedBody {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("VersionedBody([REDACTED])")
    }
}

impl VersionedBody {
    pub fn len(&self) -> usize {
        self.length
    }

    pub fn is_empty(&self) -> bool {
        self.length == 0
    }

    pub fn into_stream(self) -> impl Stream<Item = Result<Vec<u8>, GatewayError>> + Send {
        stream::try_unfold(self, |mut body| async move {
            if body.length == 0 {
                return Ok(None);
            }
            let mut chunk = vec![0; body.length.min(STREAM_CHUNK_BYTES)];
            let read = body
                .reader
                .read(&mut chunk)
                .with_subscriber(tracing::Dispatch::none())
                .await
                .map_err(|_| GatewayError)?;
            if read == 0 {
                return Err(GatewayError);
            }
            body.length -= read;
            if body.length == 0 {
                verify_eof(&mut body.reader).await?;
            }
            chunk.truncate(read);
            Ok(Some((chunk, body)))
        })
    }
}

async fn verify_eof(reader: &mut Pin<Box<dyn AsyncRead + Send>>) -> Result<(), GatewayError> {
    let mut extra = [0; 1];
    let read = reader
        .read(&mut extra)
        .with_subscriber(tracing::Dispatch::none())
        .await
        .map_err(|_| GatewayError)?;
    if read != 0 {
        return Err(GatewayError);
    }
    Ok(())
}

pub struct S3BodyGateway {
    client: aws_sdk_s3::Client,
    read_client: aws_sdk_s3::Client,
    bucket: String,
}

struct RejectTrailersBody(SdkBody);

impl HttpBody for RejectTrailersBody {
    type Data = Bytes;
    type Error = BoxError;

    fn poll_frame(
        mut self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Result<Frame<Self::Data>, Self::Error>>> {
        match Pin::new(&mut self.0).poll_frame(cx) {
            Poll::Ready(Some(Ok(frame))) if frame.is_trailers() => {
                Poll::Ready(Some(Err(GatewayError.into())))
            }
            other => other,
        }
    }
}

#[derive(Debug)]
struct ChecksumEvidenceGuard;

impl Intercept for ChecksumEvidenceGuard {
    fn name(&self) -> &'static str {
        "ChecksumEvidenceGuard"
    }

    fn modify_before_deserialization(
        &self,
        context: &mut BeforeDeserializationInterceptorContextMut<'_>,
        _runtime: &RuntimeComponents,
        _config: &mut ConfigBag,
    ) -> Result<(), BoxError> {
        let response = context.response_mut();
        let mut checksum_count = 0;
        let mut checksum_type_count = 0;
        for (name, value) in response.headers().iter_bytes() {
            if name == "x-amz-checksum-type" {
                let value = std::str::from_utf8(value).map_err(|_| GatewayError)?;
                checksum_type_count += 1;
                if checksum_type_count > 1 || value != "FULL_OBJECT" {
                    return Err(GatewayError.into());
                }
            } else if name.starts_with("x-amz-checksum-") {
                let value = std::str::from_utf8(value).map_err(|_| GatewayError)?;
                checksum_count += 1;
                // The SDK validates only one of its supported checksum headers.
                let expected_length: usize = match name {
                    "x-amz-checksum-crc32" | "x-amz-checksum-crc32c" => 4,
                    "x-amz-checksum-crc64nvme" => 8,
                    "x-amz-checksum-sha1" => 20,
                    "x-amz-checksum-sha256" => 32,
                    _ => return Err(GatewayError.into()),
                };
                if checksum_count > 1
                    || value.len() != expected_length.div_ceil(3) * 4
                    || STANDARD
                        .decode(value)
                        .ok()
                        .is_none_or(|decoded| decoded.len() != expected_length)
                {
                    return Err(GatewayError.into());
                }
            } else if matches!(
                name,
                "content-md5"
                    | "digest"
                    | "content-digest"
                    | "repr-digest"
                    | "trailer"
                    | "x-amz-trailer"
            ) {
                return Err(GatewayError.into());
            }
        }
        let body = std::mem::replace(response.body_mut(), SdkBody::taken());
        *response.body_mut() = SdkBody::from_body_1_x(RejectTrailersBody(body));
        Ok(())
    }
}

impl S3BodyGateway {
    pub fn new(client: aws_sdk_s3::Client, bucket: String) -> Self {
        // Client interceptors wrap trailers before the SDK's checksum interceptor.
        let read_config = client
            .config()
            .to_builder()
            .interceptor(ChecksumEvidenceGuard)
            .build();
        let read_client = aws_sdk_s3::Client::from_conf(read_config);
        Self {
            client,
            read_client,
            bucket,
        }
    }

    /// Call only after authorizing a message and resolving its record in the store.
    pub async fn read_version(
        &self,
        body: &RecordedExternalBody,
    ) -> Result<VersionedBody, GatewayError> {
        // The SDK's trace events include the request URI and hence object evidence.
        async {
            validate_version(body)?;
            let response = self
                .read_client
                .get_object()
                .bucket(&self.bucket)
                .key(&body.object_key)
                .version_id(&body.version_id)
                .checksum_mode(ChecksumMode::Enabled)
                .send()
                .await
                .map_err(|_| GatewayError)?;
            if response.version_id() != Some(body.version_id.as_str()) {
                return Err(GatewayError);
            }
            let length = response
                .content_length()
                .and_then(|length| usize::try_from(length).ok())
                .filter(|length| *length <= MAX_EXTERNAL_BODY_BYTES)
                .ok_or(GatewayError)?;
            let mut reader: Pin<Box<dyn AsyncRead + Send>> =
                Box::pin(response.body.into_async_read());
            if length == 0 {
                verify_eof(&mut reader).await?;
            }
            Ok(VersionedBody { reader, length })
        }
        .with_subscriber(tracing::Dispatch::none())
        .await
    }

    pub async fn delete_version(&self, body: &RecordedExternalBody) -> Result<(), GatewayError> {
        async {
            validate_version(body)?;
            self.client
                .delete_object()
                .bucket(&self.bucket)
                .key(&body.object_key)
                .version_id(&body.version_id)
                .send()
                .await
                .map_err(|_| GatewayError)?;
            Ok(())
        }
        .with_subscriber(tracing::Dispatch::none())
        .await
    }
}

fn validate_version(body: &RecordedExternalBody) -> Result<(), GatewayError> {
    if body.version_id.trim().is_empty() || body.version_id == "null" {
        Err(GatewayError)
    } else {
        Ok(())
    }
}
