use nexus_mailstore::RecordedExternalBody;
use tracing::instrument::WithSubscriber;

#[derive(Debug, thiserror::Error)]
#[error("external body storage unavailable")]
pub struct GatewayError;

pub struct S3BodyGateway {
    client: aws_sdk_s3::Client,
    bucket: String,
}

impl S3BodyGateway {
    pub fn new(client: aws_sdk_s3::Client, bucket: String) -> Self {
        Self { client, bucket }
    }

    /// Call only after authorizing a message and resolving its record in the store.
    pub async fn read_version(&self, body: &RecordedExternalBody) -> Result<Vec<u8>, GatewayError> {
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
            let bytes = response.body.collect().await.map_err(|_| GatewayError)?;
            Ok(bytes.into_bytes().to_vec())
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
