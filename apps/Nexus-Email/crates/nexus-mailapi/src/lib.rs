//! HTTP API for Nexus Email.
//!
//! **Binds loopback only, and trusts `X-Nexus-Subject`.** That header names the
//! authenticated user, and it is set by the Dashboard server after it has asked
//! Auth who the caller is — the same trust model the Cloud console proxy uses.
//! It is only sound because nothing outside this machine can reach the port: if
//! this service is ever exposed publicly, the header becomes a way for anyone
//! to claim to be anyone, so the bind address is a security control, not a
//! deployment detail.

use std::sync::Arc;

use axum::{
    body::Body,
    extract::{Path, Query, State},
    http::{HeaderMap, StatusCode},
    response::{IntoResponse, Response},
    routing::{get, post},
    Json, Router,
};
use nexus_maildelivery::{Deliverer, Disposition};
use nexus_mailmsg::MessageBuilder;
use nexus_mailstore::{Address, FolderKind, MailStore, MessageSummary};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

pub mod html;

pub struct AppState {
    pub store: MailStore,
    pub deliverer: Deliverer,
    /// The domain this node issues addresses in, used to build a sender
    /// address for a user who has one.
    pub primary_domain: String,
}

pub type SharedState = Arc<AppState>;

/// An error the API can return without leaking internals.
pub struct ApiError(StatusCode, String);

impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
        (self.0, Json(serde_json::json!({ "error": self.1 }))).into_response()
    }
}

fn bad(msg: &str) -> ApiError {
    ApiError(StatusCode::BAD_REQUEST, msg.to_string())
}

fn server(msg: &str) -> ApiError {
    // Deliberately vague to the caller; the detail goes to the log.
    tracing::error!("{msg}");
    ApiError(StatusCode::INTERNAL_SERVER_ERROR, "internal error".into())
}

/// The caller's ecosystem identity, or 401.
fn subject(headers: &HeaderMap) -> Result<String, ApiError> {
    headers
        .get("x-nexus-subject")
        .and_then(|v| v.to_str().ok())
        .filter(|s| !s.is_empty())
        .map(str::to_string)
        .ok_or(ApiError(StatusCode::UNAUTHORIZED, "not authenticated".into()))
}

/// The caller's mailbox, or 404 if they have none yet.
async fn mailbox_of(state: &SharedState, headers: &HeaderMap) -> Result<Uuid, ApiError> {
    let subj = subject(headers)?;
    state
        .store
        .mailbox_for_subject(&subj)
        .await
        .map_err(|e| server(&format!("mailbox lookup: {e}")))?
        .ok_or(ApiError(StatusCode::NOT_FOUND, "no mailbox for this account".into()))
}

#[derive(Serialize)]
pub struct FolderDto {
    pub id: Uuid,
    pub name: String,
    pub kind: String,
}

#[derive(Serialize)]
pub struct SummaryDto {
    pub id: Uuid,
    pub thread_id: Uuid,
    pub subject: Option<String>,
    pub from: String,
    pub received_at: String,
    pub seen: bool,
    pub flagged: bool,
    pub snippet: Option<String>,
}

impl From<MessageSummary> for SummaryDto {
    fn from(m: MessageSummary) -> Self {
        Self {
            id: m.id,
            thread_id: m.thread_id,
            subject: m.subject,
            from: m.from_address,
            received_at: m.received_at.to_rfc3339(),
            seen: m.seen,
            flagged: m.flagged,
            snippet: m.snippet,
        }
    }
}

pub fn router(state: SharedState) -> Router {
    Router::new()
        .route("/api/v1/health", get(|| async { Json(serde_json::json!({"ok": true})) }))
        .route("/api/v1/folders", get(list_folders))
        .route("/api/v1/folders/:id/messages", get(list_messages))
        .route("/api/v1/messages/:id", get(read_message))
        .route("/api/v1/messages/:id/seen", post(mark_seen))
        .route("/api/v1/messages", post(send_message))
        .route("/api/v1/search", get(search))
        .route("/api/v1/threads/:id/messages", get(thread_messages))
        .route("/api/v1/messages/:id/attachments/:index", get(download_attachment))
        .with_state(state)
}

async fn list_folders(
    State(state): State<SharedState>,
    headers: HeaderMap,
) -> Result<Json<Vec<FolderDto>>, ApiError> {
    let mailbox = mailbox_of(&state, &headers).await?;
    let folders = state
        .store
        .folders(mailbox)
        .await
        .map_err(|e| server(&format!("folders: {e}")))?;

    Ok(Json(
        folders
            .into_iter()
            .map(|f| FolderDto { id: f.id, name: f.name, kind: f.kind.as_str().to_string() })
            .collect(),
    ))
}

#[derive(Deserialize)]
pub struct Page {
    #[serde(default)]
    pub offset: i64,
    pub limit: Option<i64>,
}

async fn list_messages(
    State(state): State<SharedState>,
    headers: HeaderMap,
    Path(folder_id): Path<Uuid>,
    Query(page): Query<Page>,
) -> Result<Json<Vec<SummaryDto>>, ApiError> {
    let mailbox = mailbox_of(&state, &headers).await?;
    let msgs = state
        .store
        .list_folder(mailbox, folder_id, page.limit.unwrap_or(50), page.offset)
        .await
        .map_err(|e| server(&format!("list: {e}")))?;
    Ok(Json(msgs.into_iter().map(Into::into).collect()))
}

#[derive(Serialize)]
pub struct MessageDto {
    pub id: Uuid,
    pub thread_id: Option<Uuid>,
    pub subject: Option<String>,
    pub from: String,
    pub to: Option<String>,
    pub date: Option<String>,
    pub text: String,
    /// Already sanitised. The browser never sees the sender's original markup.
    pub html: Option<String>,
    /// True when remote images or similar were stripped, so the UI can offer to
    /// load them instead of silently showing a different message than was sent.
    pub blocked_remote: bool,
    pub attachments: Vec<AttachmentDto>,
}

#[derive(Serialize)]
pub struct AttachmentDto {
    /// Position in the MIME tree, which is how it is fetched. Filenames are
    /// chosen by the sender and are neither unique nor safe as identifiers.
    pub index: usize,
    pub filename: String,
    pub mime_type: String,
    pub size: usize,
}

async fn read_message(
    State(state): State<SharedState>,
    headers: HeaderMap,
    Path(message_id): Path<Uuid>,
) -> Result<Json<MessageDto>, ApiError> {
    let mailbox = mailbox_of(&state, &headers).await?;
    // The store scopes this by membership, so a message the caller does not
    // hold is not readable even with a valid id.
    let raw = state
        .store
        .raw_message(mailbox, message_id)
        .await
        .map_err(|_| ApiError(StatusCode::NOT_FOUND, "no such message".into()))?;

    let parsed = nexus_mailmsg::parse(&raw, nexus_mailmsg::Limits::default())
        .map_err(|e| server(&format!("parse: {e}")))?;
    let root = nexus_mailmsg::tree(&parsed);
    let parts = root.walk();

    let body_of = |mime: &str| {
        parts
            .iter()
            .find(|p| p.content_type.mime_type == mime && !p.is_attachment())
            .map(|p| p.text())
    };

    let attachments = parts
        .iter()
        .enumerate()
        .filter(|(_, p)| p.is_attachment())
        .map(|(index, p)| AttachmentDto {
            index,
            filename: p.filename().unwrap_or_else(|| "attachment".into()),
            mime_type: p.content_type.mime_type.clone(),
            size: p.body.len(),
        })
        .collect();

    let raw_html = body_of("text/html");
    let sanitised = raw_html.as_deref().map(|h| html::sanitise(h, false));

    Ok(Json(MessageDto {
        id: message_id,
        thread_id: None,
        subject: parsed
            .headers
            .get("subject")
            .map(nexus_mailmsg::encoding::decode_encoded_words),
        from: parsed.headers.get("from").unwrap_or_default().to_string(),
        to: parsed.headers.get("to").map(str::to_string),
        date: parsed.headers.get("date").map(str::to_string),
        text: body_of("text/plain").unwrap_or_else(|| root.text()),
        blocked_remote: sanitised.as_ref().map(|s| s.blocked_remote).unwrap_or(false),
        html: sanitised.map(|s| s.html),
        attachments,
    }))
}

#[derive(Deserialize)]
pub struct SeenBody {
    pub seen: bool,
}

async fn mark_seen(
    State(state): State<SharedState>,
    headers: HeaderMap,
    Path(message_id): Path<Uuid>,
    Json(body): Json<SeenBody>,
) -> Result<StatusCode, ApiError> {
    let mailbox = mailbox_of(&state, &headers).await?;
    state
        .store
        .set_seen(mailbox, message_id, body.seen)
        .await
        .map_err(|e| server(&format!("set_seen: {e}")))?;
    Ok(StatusCode::NO_CONTENT)
}

#[derive(Deserialize)]
pub struct SendRequest {
    pub to: Vec<String>,
    #[serde(default)]
    pub cc: Vec<String>,
    pub subject: String,
    pub text: String,
    pub html: Option<String>,
    pub in_reply_to: Option<String>,
    #[serde(default)]
    pub references: Vec<String>,
}

#[derive(Serialize)]
pub struct SendResponse {
    pub outcomes: Vec<OutcomeDto>,
}

#[derive(Serialize)]
pub struct OutcomeDto {
    pub recipient: String,
    pub status: String,
    pub reason: Option<String>,
}

async fn send_message(
    State(state): State<SharedState>,
    headers: HeaderMap,
    Json(req): Json<SendRequest>,
) -> Result<Json<SendResponse>, ApiError> {
    let subj = subject(&headers)?;
    let mailbox = mailbox_of(&state, &headers).await?;

    if req.to.is_empty() && req.cc.is_empty() {
        return Err(bad("a message needs at least one recipient"));
    }

    // The mailbox's own address, not the subject id. Composing From out of an
    // internal user identifier produced senders like usr-msosh4ui-2@tnhc.dev,
    // which is neither routable back to the sender nor something anyone would
    // want in a recipient's inbox.
    let from = match state
        .store
        .primary_address(mailbox)
        .await
        .map_err(|e| server(&format!("primary address: {e}")))?
    {
        Some(addr) => addr,
        None => {
            return Err(ApiError(
                StatusCode::CONFLICT,
                "this mailbox has no address to send from".into(),
            ))
        }
    };
    let _ = &subj;

    let mut recipients = Vec::new();
    for raw in req.to.iter().chain(req.cc.iter()) {
        recipients.push(Address::parse(raw).map_err(|_| bad(&format!("malformed address: {raw}")))?);
    }

    let msg_id = format!("<{}@{}>", Uuid::now_v7().simple(), state.primary_domain);
    let mut builder = MessageBuilder::new(from.as_string(), msg_id)
        .subject(&req.subject)
        .text(&req.text);
    for t in &req.to {
        builder = builder.to(t);
    }
    for c in &req.cc {
        builder = builder.cc(c);
    }
    if let Some(html) = &req.html {
        builder = builder.html(html);
    }
    if let Some(irt) = &req.in_reply_to {
        builder = builder.in_reply_to(irt);
    }
    if !req.references.is_empty() {
        builder = builder.references(req.references.clone());
    }

    let raw = builder.build();
    let outcomes = state
        .deliverer
        .submit(&raw, &from, &recipients, Some(mailbox))
        .await
        .map_err(|e| server(&format!("submit: {e}")))?;

    Ok(Json(SendResponse {
        outcomes: outcomes
            .into_iter()
            .map(|o| match o.disposition {
                Disposition::DeliveredLocally => OutcomeDto {
                    recipient: o.recipient,
                    status: "delivered".into(),
                    reason: None,
                },
                Disposition::Queued => OutcomeDto {
                    recipient: o.recipient,
                    status: "queued".into(),
                    reason: None,
                },
                Disposition::Rejected(reason) => OutcomeDto {
                    recipient: o.recipient,
                    status: "rejected".into(),
                    reason: Some(reason),
                },
            })
            .collect(),
    }))
}

#[derive(Deserialize)]
pub struct SearchQuery {
    pub q: String,
}

async fn search(
    State(state): State<SharedState>,
    headers: HeaderMap,
    Query(q): Query<SearchQuery>,
) -> Result<Json<Vec<SummaryDto>>, ApiError> {
    let mailbox = mailbox_of(&state, &headers).await?;
    if q.q.trim().is_empty() {
        return Ok(Json(Vec::new()));
    }
    let hits = state
        .store
        .search(mailbox, &q.q, 50)
        .await
        .map_err(|e| server(&format!("search: {e}")))?;
    Ok(Json(hits.into_iter().map(Into::into).collect()))
}

async fn thread_messages(
    State(state): State<SharedState>,
    headers: HeaderMap,
    Path(thread_id): Path<Uuid>,
) -> Result<Json<Vec<SummaryDto>>, ApiError> {
    let mailbox = mailbox_of(&state, &headers).await?;
    let msgs = state
        .store
        .thread_messages(mailbox, thread_id)
        .await
        .map_err(|e| server(&format!("thread: {e}")))?;
    Ok(Json(msgs.into_iter().map(Into::into).collect()))
}

/// Serve one attachment.
///
/// Always as a download, never inline. Attachment bytes are chosen entirely by
/// the sender, and serving them inline on app.<domain> would let an HTML or SVG
/// attachment execute script on the origin holding the session cookie. The
/// Content-Type is deliberately not the sender's claimed one, `nosniff` stops
/// the browser guessing a better one, and the CSP is a sandbox for anything
/// that still finds a way to render.
async fn download_attachment(
    State(state): State<SharedState>,
    headers: HeaderMap,
    Path((message_id, index)): Path<(Uuid, usize)>,
) -> Result<Response, ApiError> {
    let mailbox = mailbox_of(&state, &headers).await?;
    let raw = state
        .store
        .raw_message(mailbox, message_id)
        .await
        .map_err(|_| ApiError(StatusCode::NOT_FOUND, "no such message".into()))?;

    let parsed = nexus_mailmsg::parse(&raw, nexus_mailmsg::Limits::default())
        .map_err(|e| server(&format!("parse: {e}")))?;
    let root = nexus_mailmsg::tree(&parsed);
    let parts = root.walk();

    let part = parts
        .get(index)
        .filter(|p| p.is_attachment())
        .ok_or(ApiError(StatusCode::NOT_FOUND, "no such attachment".into()))?;

    // The sender's filename is used for the download name only, with anything
    // that could escape a directory or confuse a shell removed.
    let name: String = part
        .filename()
        .unwrap_or_else(|| "attachment".into())
        .chars()
        .filter(|c| c.is_alphanumeric() || matches!(c, '.' | '-' | '_' | ' '))
        .take(120)
        .collect();
    let name = if name.trim().is_empty() { "attachment".to_string() } else { name };

    Ok((
        StatusCode::OK,
        [
            ("content-type", "application/octet-stream".to_string()),
            ("content-disposition", format!("attachment; filename=\"{name}\"")),
            ("x-content-type-options", "nosniff".to_string()),
            ("content-security-policy", "default-src 'none'; sandbox".to_string()),
        ],
        Body::from(part.body.clone()),
    )
        .into_response())
}

/// Folder kinds, exposed so the UI can label the special-use folders.
pub fn folder_kinds() -> Vec<&'static str> {
    vec![
        FolderKind::Inbox.as_str(),
        FolderKind::Sent.as_str(),
        FolderKind::Drafts.as_str(),
        FolderKind::Trash.as_str(),
        FolderKind::Archive.as_str(),
    ]
}
