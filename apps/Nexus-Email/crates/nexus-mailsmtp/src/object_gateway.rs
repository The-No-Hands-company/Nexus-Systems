use std::{fmt, pin::Pin};

use futures_util::{stream, Stream};
use nexus_mailstore::RecordedExternalBody;
use tokio::io::{AsyncRead, AsyncReadExt};
use tracing::instrument::WithSubscriber;

pub const MAX_EXTERNAL_BODY_BYTES: usize = 25 * 1024 * 1024;
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
            chunk.truncate(read);
            Ok(Some((chunk, body)))
        })
    }
}

pub struct S3BodyGateway {
    client: aws_sdk_s3::Client,
    bucket: String,
}

impl S3BodyGateway {
    pub fn new(client: aws_sdk_s3::Client, bucket: String) -> Self {
        Self { client, bucket }
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
                .client
                .get_object()
                .bucket(&self.bucket)
                .key(&body.object_key)
                .version_id(&body.version_id)
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
            Ok(VersionedBody {
                reader: Box::pin(response.body.into_async_read()),
                length,
            })
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
