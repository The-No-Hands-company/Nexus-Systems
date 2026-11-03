//! Private, message-ID-only access to recorded external message bodies.

use std::{
    fmt,
    net::SocketAddr,
    sync::{Arc, Mutex},
    time::Duration,
};

use axum::{
    body::{to_bytes, Body},
    extract::{connect_info::ConnectInfo, rejection::PathRejection, Path, Request, State},
    http::{header, HeaderMap, StatusCode},
    middleware::{self, Next},
    response::{IntoResponse, Response},
    routing::get,
    Extension, Router,
};
use futures_util::{stream, StreamExt};
use nexus_mailstore::{MailStore, MailStoreError};
use sha2::{Digest, Sha256};
use subtle::ConstantTimeEq;
use tokio::{
    net::TcpListener,
    sync::{mpsc, Semaphore},
    time::{timeout_at, Instant},
};
use uuid::Uuid;

use crate::object_gateway::{GatewayError, S3BodyGateway};

const READ_DEADLINE: Duration = Duration::from_secs(30);
type PendingChunk = Arc<Mutex<Option<Vec<u8>>>>;

#[derive(Clone)]
pub struct SecretString(String);

impl From<String> for SecretString {
    fn from(value: String) -> Self {
        Self(value)
    }
}

impl fmt::Debug for SecretString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str("[REDACTED]")
    }
}

#[derive(Clone, Debug)]
pub struct ObjectServiceConfig {
    pub bind: SocketAddr,
    pub api_token: SecretString,
    pub imap_token: SecretString,
    pub outbound_token: SecretString,
}

#[derive(Debug, thiserror::Error)]
#[error("invalid object service configuration")]
pub struct ConfigError;

#[derive(Debug, thiserror::Error)]
#[error("object service listener unavailable")]
pub struct BindError;

impl ObjectServiceConfig {
    /// An entirely unset service is disabled; partial or invalid settings fail closed.
    pub fn from_settings(
        bind: Option<&str>,
        api: Option<&str>,
        imap: Option<&str>,
        outbound: Option<&str>,
    ) -> Result<Option<Self>, ConfigError> {
        if bind.is_none() && api.is_none() && imap.is_none() && outbound.is_none() {
            return Ok(None);
        }
        let config = Self {
            bind: bind
                .unwrap_or("127.0.0.1:2590")
                .parse()
                .map_err(|_| ConfigError)?,
            api_token: api.ok_or(ConfigError)?.to_owned().into(),
            imap_token: imap.ok_or(ConfigError)?.to_owned().into(),
            outbound_token: outbound.ok_or(ConfigError)?.to_owned().into(),
        };
        config.validate()?;
        Ok(Some(config))
    }

    pub fn validate(&self) -> Result<(), ConfigError> {
        let tokens = [
            &self.api_token.0,
            &self.imap_token.0,
            &self.outbound_token.0,
        ];
        if !self.bind.ip().is_loopback()
            || tokens
                .iter()
                .any(|token| token.is_empty() || !token.bytes().all(|b| b.is_ascii_graphic()))
            || tokens[0] == tokens[1]
            || tokens[0] == tokens[2]
            || tokens[1] == tokens[2]
        {
            return Err(ConfigError);
        }
        Ok(())
    }
}

pub async fn bind(config: &ObjectServiceConfig) -> Result<TcpListener, BindError> {
    config.validate().map_err(|_| BindError)?;
    TcpListener::bind(config.bind).await.map_err(|_| BindError)
}

#[derive(Clone)]
struct ObjectService {
    store: MailStore,
    gateway: Arc<S3BodyGateway>,
    credentials: Option<[[u8; 32]; 3]>,
    active_reads: Arc<Semaphore>,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum ServiceIdentity {
    Api,
    Imap,
    Outbound,
}

/// Callers are trusted services responsible for authorizing the message first.
pub fn router(
    store: MailStore,
    gateway: Arc<S3BodyGateway>,
    config: ObjectServiceConfig,
) -> Router {
    let credentials = config.validate().ok().map(|_| {
        [
            Sha256::digest(config.api_token.0.as_bytes()).into(),
            Sha256::digest(config.imap_token.0.as_bytes()).into(),
            Sha256::digest(config.outbound_token.0.as_bytes()).into(),
        ]
    });
    let service = ObjectService {
        store,
        gateway,
        credentials,
        active_reads: Arc::new(Semaphore::new(4)),
    };
    Router::new()
        .route("/internal/object/messages/:message_id", get(body))
        .with_state(service.clone())
        .layer(middleware::from_fn_with_state(service, authorize))
}

fn authenticated(headers: &HeaderMap, credentials: &[[u8; 32]; 3]) -> Option<ServiceIdentity> {
    let mut values = headers.get_all(header::AUTHORIZATION).iter();
    let value = values.next()?;
    if values.next().is_some() {
        return None;
    }
    let token = value
        .to_str()
        .ok()
        .and_then(|value| value.strip_prefix("Bearer "))?;
    let candidate: [u8; 32] = Sha256::digest(token.as_bytes()).into();
    let matches = credentials.map(|expected| bool::from(candidate.ct_eq(&expected)));
    match matches {
        [true, false, false] => Some(ServiceIdentity::Api),
        [false, true, false] => Some(ServiceIdentity::Imap),
        [false, false, true] => Some(ServiceIdentity::Outbound),
        _ => None,
    }
}

async fn authorize(
    State(service): State<ObjectService>,
    mut request: Request,
    next: Next,
) -> Response {
    if !request
        .extensions()
        .get::<ConnectInfo<SocketAddr>>()
        .is_some_and(|peer| peer.0.ip().is_loopback())
    {
        return StatusCode::FORBIDDEN.into_response();
    }
    let Some(identity) = service
        .credentials
        .as_ref()
        .and_then(|credentials| authenticated(request.headers(), credentials))
    else {
        return StatusCode::UNAUTHORIZED.into_response();
    };
    request.extensions_mut().insert(identity);
    if request.uri().query().is_some() {
        return StatusCode::BAD_REQUEST.into_response();
    }
    let (parts, body) = request.into_parts();
    if to_bytes(body, 0).await.is_err() {
        return StatusCode::BAD_REQUEST.into_response();
    }
    next.run(Request::from_parts(parts, Body::empty())).await
}

async fn body(
    State(service): State<ObjectService>,
    Extension(_identity): Extension<ServiceIdentity>,
    path: Result<Path<String>, PathRejection>,
) -> Response {
    let Ok(Path(message)) = path else {
        return StatusCode::BAD_REQUEST.into_response();
    };
    let Ok(message_id) = Uuid::parse_str(&message) else {
        return StatusCode::BAD_REQUEST.into_response();
    };
    let Ok(permit) = service.active_reads.clone().try_acquire_owned() else {
        return StatusCode::SERVICE_UNAVAILABLE.into_response();
    };
    let deadline = Instant::now() + READ_DEADLINE;
    let result = timeout_at(deadline, async {
        let recorded = match service.store.external_body_for_message(message_id).await {
            Ok(recorded) => recorded,
            Err(MailStoreError::ExternalBodyMissing) => return Err(StatusCode::NOT_FOUND),
            Err(_) => return Err(StatusCode::SERVICE_UNAVAILABLE),
        };
        service
            .gateway
            .read_version(&recorded)
            .await
            .map_err(|_| StatusCode::SERVICE_UNAVAILABLE)
    })
    .await;
    let body = match result {
        Ok(Ok(body)) => body,
        Ok(Err(status)) => return status.into_response(),
        Err(_) => return StatusCode::SERVICE_UNAVAILABLE.into_response(),
    };
    let length = body.len();
    let (sender, receiver) = mpsc::channel(1);
    let pending: Arc<Mutex<Option<PendingChunk>>> = Arc::new(Mutex::new(None));
    let producer_pending = Arc::clone(&pending);
    tokio::spawn(async move {
        let _permit = permit;
        let pump = async {
            let stream = body.into_stream();
            futures_util::pin_mut!(stream);
            loop {
                let slot = tokio::select! {
                    _ = sender.closed() => break,
                    slot = sender.reserve() => match slot {
                        Ok(slot) => slot,
                        Err(_) => break,
                    },
                };
                let item = tokio::select! {
                    _ = sender.closed() => break,
                    item = stream.next() => item,
                };
                match item {
                    Some(Ok(chunk)) => {
                        let chunk = Arc::new(Mutex::new(Some(chunk)));
                        *producer_pending.lock().expect("body buffer lock") = Some(chunk.clone());
                        slot.send(Ok(chunk));
                    }
                    Some(Err(error)) => {
                        slot.send(Err(error));
                        break;
                    }
                    None => break,
                }
            }
        };
        let timed_out = timeout_at(deadline, pump).await.is_err();
        if timed_out {
            // Expired responses must not retain an unread body chunk.
            if let Some(chunk) = producer_pending.lock().expect("body buffer lock").take() {
                chunk.lock().expect("body chunk lock").take();
            }
            let _ = sender.try_send(Err(GatewayError));
        }
    });
    let stream = stream::unfold(receiver, |mut receiver| async move {
        let item = receiver.recv().await?;
        let result = item.and_then(|chunk| {
            chunk
                .lock()
                .expect("body chunk lock")
                .take()
                .ok_or(GatewayError)
        });
        Some((result, receiver))
    });
    Response::builder()
        .header(header::CONTENT_TYPE, "application/octet-stream")
        .header(header::CONTENT_LENGTH, length)
        .body(Body::from_stream(stream))
        .expect("static response headers are valid")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bearer_credentials_resolve_to_distinct_internal_service_identities() {
        let credentials = [
            Sha256::digest(b"api-secret").into(),
            Sha256::digest(b"imap-secret").into(),
            Sha256::digest(b"outbound-secret").into(),
        ];
        let mut headers = HeaderMap::new();
        for (token, identity) in [
            ("api-secret", ServiceIdentity::Api),
            ("imap-secret", ServiceIdentity::Imap),
            ("outbound-secret", ServiceIdentity::Outbound),
        ] {
            headers.insert(
                header::AUTHORIZATION,
                format!("Bearer {token}").parse().unwrap(),
            );
            assert_eq!(authenticated(&headers, &credentials), Some(identity));
        }
        headers.insert(header::AUTHORIZATION, "Bearer wrong".parse().unwrap());
        assert_eq!(authenticated(&headers, &credentials), None);
    }
}
