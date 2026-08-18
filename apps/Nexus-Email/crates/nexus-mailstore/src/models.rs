use chrono::{DateTime, Utc};
use serde::{Deserialize, Serialize};
use uuid::Uuid;

/// Who owns a mailbox.
///
/// Role addresses like `info@` and `postmaster@` belong to the node, not to a
/// person. Modelling that explicitly avoids inventing a placeholder user to
/// hold them — which is how mail systems end up with accounts nobody can log
/// into but everybody is afraid to delete.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum OwnerKind {
    Identity,
    Node,
}

impl OwnerKind {
    pub fn as_str(self) -> &'static str {
        match self {
            OwnerKind::Identity => "identity",
            OwnerKind::Node => "node",
        }
    }
}

/// How a message reached this node.
///
/// Provenance, not plumbing trivia: a message that arrived over the
/// authenticated node-to-node channel has been vouched for by a peer we
/// already trust, while an SMTP delivery is from an anonymous stranger. Later
/// layers make trust decisions on this, so it is recorded at write time when
/// it is still known.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum Transport {
    /// Both parties are on this node; the message never touched a network.
    Internal,
    /// Another Nexus node handed it over the authenticated channel.
    Federated,
    /// It came in over SMTP from the outside world.
    Smtp,
}

impl Transport {
    pub fn as_str(self) -> &'static str {
        match self {
            Transport::Internal => "internal",
            Transport::Federated => "federated",
            Transport::Smtp => "smtp",
        }
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
pub enum FolderKind {
    Inbox,
    Sent,
    Drafts,
    Trash,
    Archive,
    Custom,
}

impl FolderKind {
    pub fn as_str(self) -> &'static str {
        match self {
            FolderKind::Inbox => "inbox",
            FolderKind::Sent => "sent",
            FolderKind::Drafts => "drafts",
            FolderKind::Trash => "trash",
            FolderKind::Archive => "archive",
            FolderKind::Custom => "custom",
        }
    }
}

#[derive(Debug, Clone)]
pub struct Mailbox {
    pub id: Uuid,
    pub owner_subject: Option<String>,
    pub owner_kind: OwnerKind,
    pub display_name: String,
    pub created_at: DateTime<Utc>,
}

#[derive(Debug, Clone)]
pub struct Folder {
    pub id: Uuid,
    pub mailbox_id: Uuid,
    pub name: String,
    pub kind: FolderKind,
}

/// Where a message body lives.
///
/// Small bodies sit inline so reading a message is one query. Anything larger
/// goes to object storage keyed by content hash: attachments are most of the
/// bytes in real mail, and Postgres is a poor blob store.
#[derive(Debug, Clone)]
pub enum Body {
    Inline(Vec<u8>),
    Object { key: String },
}

/// A message as stored: immutable, content-addressed, owned by no mailbox.
/// Mailbox membership lives in `mailbox_messages`.
#[derive(Debug, Clone)]
pub struct Message {
    pub id: Uuid,
    pub content_hash: String,
    pub rfc822_msg_id: Option<String>,
    pub thread_id: Uuid,
    pub in_reply_to: Option<String>,
    pub references_ids: Vec<String>,
    pub subject: Option<String>,
    pub from_address: String,
    pub sent_at: Option<DateTime<Utc>>,
    pub received_at: DateTime<Utc>,
    pub transport: Transport,
    pub size_bytes: i64,
    pub body: Body,
}

/// Per-mailbox state for a shared message: where it sits and how it is flagged.
#[derive(Debug, Clone, Default)]
pub struct Flags {
    pub seen: bool,
    pub answered: bool,
    pub flagged: bool,
    pub draft: bool,
    pub deleted: bool,
    pub keywords: Vec<String>,
}
