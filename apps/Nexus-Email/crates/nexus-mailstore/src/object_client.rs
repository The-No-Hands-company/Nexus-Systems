//! Private loopback client for the Mail-owned message-body service.

use std::{future::Future, net::IpAddr, pin::Pin, time::Duration};

use reqwest::{header, redirect::Policy, Client, Url};
use uuid::Uuid;

use crate::MAX_EXTERNAL_BODY_BYTES;

#[derive(Debug, thiserror::Error)]
#[error("message body temporarily unavailable")]
pub struct ObjectBodyClientError;

pub trait ObjectBodyClient: Send + Sync {
    fn read_message(
        &self,
        message_id: Uuid,
    ) -> Pin<Box<dyn Future<Output = Result<Vec<u8>, ObjectBodyClientError>> + Send + '_>>;
}

pub struct HttpObjectBodyClient {
    client: Client,
    base: Url,
    authorization: header::HeaderValue,
}

impl HttpObjectBodyClient {
    pub fn new(url: &str, secret: &str) -> Result<Self, ObjectBodyClientError> {
        let base = Url::parse(url).map_err(|_| ObjectBodyClientError)?;
        let loopback = base
            .host_str()
            .and_then(|host| host.trim_matches(['[', ']']).parse::<IpAddr>().ok())
            .is_some_and(|ip| ip.is_loopback());
        if base.scheme() != "http"
            || !loopback
            || base.port().is_none()
            || base.path() != "/"
            || base.query().is_some()
            || base.fragment().is_some()
            || !base.username().is_empty()
            || base.password().is_some()
            || secret.is_empty()
            || !secret.bytes().all(|b| b.is_ascii_graphic())
        {
            return Err(ObjectBodyClientError);
        }
        let mut authorization = header::HeaderValue::from_str(&format!("Bearer {secret}"))
            .map_err(|_| ObjectBodyClientError)?;
        authorization.set_sensitive(true);
        let client = Client::builder()
            .no_proxy()
            .redirect(Policy::none())
            .timeout(Duration::from_secs(35))
            .build()
            .map_err(|_| ObjectBodyClientError)?;
        Ok(Self { client, base, authorization })
    }
}

impl ObjectBodyClient for HttpObjectBodyClient {
    fn read_message(
        &self,
        message_id: Uuid,
    ) -> Pin<Box<dyn Future<Output = Result<Vec<u8>, ObjectBodyClientError>> + Send + '_>> {
        Box::pin(async move {
            let url = self.base.join(&format!("/internal/object/messages/{message_id}"))
                .map_err(|_| ObjectBodyClientError)?;
            let mut response = self.client.get(url)
                .header(header::AUTHORIZATION, self.authorization.clone())
                .send().await.map_err(|_| ObjectBodyClientError)?;
            if !response.status().is_success()
                || response.content_length().is_some_and(|len| len > MAX_EXTERNAL_BODY_BYTES as u64)
            {
                return Err(ObjectBodyClientError);
            }
            let mut body = Vec::new();
            while let Some(chunk) = response.chunk().await.map_err(|_| ObjectBodyClientError)? {
                if chunk.len() > MAX_EXTERNAL_BODY_BYTES.saturating_sub(body.len()) {
                    return Err(ObjectBodyClientError);
                }
                body.extend_from_slice(&chunk);
            }
            Ok(body)
        })
    }
}
