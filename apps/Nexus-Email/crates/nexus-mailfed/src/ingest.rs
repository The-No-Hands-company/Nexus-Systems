//! Receiving handoffs from pinned peers.
//!
//! Mounted on its own listener, never on the webmail API: that service trusts
//! a caller-supplied identity header and is only safe on loopback, while this
//! one is meant to be reached by other nodes.

use std::sync::Arc;

use axum::body::Bytes;
use axum::extract::{DefaultBodyLimit, State};
use axum::http::{HeaderMap, StatusCode};
use axum::response::{IntoResponse, Response};
use axum::routing::{get, post};
use axum::{Json, Router};
use nexus_maildelivery::{Deliverer, DeliveryError, Route};
use nexus_mailstore::Address;
use serde_json::json;

use crate::key::NodeKey;
use crate::peers::PeerDirectory;
use crate::signature::{verify, SignedRequest};
use crate::wire::{self, HandoffResponse, RecipientResult};

pub struct IngestState {
    /// The domain this node serves; what `GET /federation/v1/key` announces.
    pub local_domain: String,
    /// The host peers pinned for us. Requests are verified against this, not
    /// the incoming `Host` header, which the ecosystem proxy rewrites.
    pub public_host: String,
    pub key: Arc<NodeKey>,
    pub peers: PeerDirectory,
    pub deliverer: Arc<Deliverer>,
    /// Unix seconds. Injected so the replay window can be tested.
    pub clock: Arc<dyn Fn() -> i64 + Send + Sync>,
}

pub fn router(state: Arc<IngestState>) -> Router {
    Router::new()
        .route(wire::KEY_PATH, get(public_key))
        .route(wire::MAIL_PATH, post(receive))
        .layer(DefaultBodyLimit::max(wire::MAX_BODY_BYTES))
        .with_state(state)
}

async fn public_key(State(state): State<Arc<IngestState>>) -> Json<serde_json::Value> {
    Json(json!({ "domain": state.local_domain, "public_key": state.key.public_b64() }))
}

fn refuse(status: StatusCode, why: &str) -> Response {
    (status, Json(json!({ "error": why }))).into_response()
}

fn header<'a>(headers: &'a HeaderMap, name: &str) -> Option<&'a str> {
    headers.get(name).and_then(|v| v.to_str().ok()).map(str::trim).filter(|v| !v.is_empty())
}

async fn receive(State(state): State<Arc<IngestState>>, headers: HeaderMap, body: Bytes) -> Response {
    let (Some(node), Some(from), Some(rcpts), Some(ts), Some(nonce), Some(sig)) = (
        header(&headers, wire::H_NODE),
        header(&headers, wire::H_FROM),
        header(&headers, wire::H_RECIPIENTS),
        header(&headers, wire::H_TIMESTAMP),
        header(&headers, wire::H_NONCE),
        header(&headers, wire::H_SIGNATURE),
    ) else {
        return refuse(StatusCode::BAD_REQUEST, "missing federation headers");
    };
    let Ok(timestamp) = ts.parse::<i64>() else {
        return refuse(StatusCode::BAD_REQUEST, "malformed timestamp");
    };
    let recipients: Vec<String> = rcpts.split(',').map(|r| r.trim().to_string()).collect();

    // Authentication, cheapest check first. Every refusal here is the same
    // 401 so a prober learns nothing about which part failed.
    let peer = match state.peers.get(node).await {
        Ok(Some(p)) => p,
        Ok(None) => return refuse(StatusCode::UNAUTHORIZED, "not authenticated"),
        Err(e) => {
            tracing::warn!(error = %e, "peer lookup failed");
            return refuse(StatusCode::SERVICE_UNAVAILABLE, "try again later");
        }
    };
    if (timestamp - (state.clock)()).abs() > wire::MAX_SKEW_SECS {
        return refuse(StatusCode::UNAUTHORIZED, "not authenticated");
    }
    let signed = SignedRequest {
        method: "POST".into(),
        path: wire::MAIL_PATH.into(),
        host: state.public_host.clone(),
        timestamp,
        nonce: nonce.to_string(),
        envelope_from: from.to_string(),
        recipients: recipients.clone(),
        body_sha256: SignedRequest::body_digest(&body),
    };
    if verify(&peer.public_key, &signed, sig).is_err() {
        return refuse(StatusCode::UNAUTHORIZED, "not authenticated");
    }
    // Only after the signature: unauthenticated junk must not be able to fill
    // the nonce table.
    match state.peers.record_nonce(&peer.domain, nonce).await {
        Ok(true) => {}
        Ok(false) => return refuse(StatusCode::UNAUTHORIZED, "not authenticated"),
        Err(e) => {
            tracing::warn!(error = %e, "nonce record failed");
            return refuse(StatusCode::SERVICE_UNAVAILABLE, "try again later");
        }
    }

    // Authenticated. A peer speaks only for its own domain, whether the mail
    // is for us or for the world: otherwise any pinned peer could put words in
    // any sender's mouth, ours included.
    let Ok(from_addr) = Address::parse(from) else {
        return refuse(StatusCode::BAD_REQUEST, "malformed envelope-from");
    };
    if from_addr.domain != peer.domain {
        return refuse(StatusCode::FORBIDDEN, "a peer may only send as its own domain");
    }

    let mut results = Vec::with_capacity(recipients.len());
    let mut relay = Vec::new();
    for raw_rcpt in &recipients {
        let Ok(rcpt) = Address::parse(raw_rcpt) else {
            results.push(no(raw_rcpt, "malformed address"));
            continue;
        };
        match state.deliverer.router().route(&rcpt) {
            Route::Local => match state.deliverer.accept_federated(&body, &from_addr, &rcpt).await {
                Ok(_) => results.push(yes(raw_rcpt)),
                Err(DeliveryError::Permanent { reason, .. }) => results.push(no(raw_rcpt, &reason)),
                Err(e) => return transient(e),
            },
            Route::Federated { .. } => results.push(no(raw_rcpt, "mail is not forwarded between peers")),
            Route::External if peer.may_relay => relay.push(rcpt),
            Route::External => results.push(no(raw_rcpt, "this peer may not relay through this node")),
        }
    }

    if !relay.is_empty() {
        match state.deliverer.relay_for_peer(&body, &from_addr, &relay).await {
            Ok(_) => results.extend(relay.iter().map(|r| yes(&r.as_string()))),
            Err(DeliveryError::Permanent { reason, .. }) => {
                results.extend(relay.iter().map(|r| no(&r.as_string(), &reason)))
            }
            Err(e) => return transient(e),
        }
    }

    (StatusCode::OK, Json(HandoffResponse { results })).into_response()
}

fn yes(rcpt: &str) -> RecipientResult {
    RecipientResult { recipient: rcpt.to_string(), accepted: true, reason: None }
}

fn no(rcpt: &str, why: &str) -> RecipientResult {
    RecipientResult { recipient: rcpt.to_string(), accepted: false, reason: Some(why.to_string()) }
}

/// A storage failure is ours, not the sender's: 503 so they retry.
fn transient(e: DeliveryError) -> Response {
    tracing::warn!(error = %e, "federated handoff failed");
    refuse(StatusCode::SERVICE_UNAVAILABLE, "try again later")
}
