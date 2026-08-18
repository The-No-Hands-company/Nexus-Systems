//! Storage for Nexus Email: mailboxes, addresses, messages, threads, folders
//! and flags.
//!
//! Two shapes here are deliberate and worth knowing before changing anything:
//!
//! - **An address is a routing rule, not an account.** Mailboxes belong to an
//!   ecosystem identity that already exists in Auth, or to the node itself for
//!   role addresses. Aliases are therefore ordinary, not a special case.
//! - **A message is stored once.** Mailbox membership, folder placement and
//!   per-mailbox flags live in `mailbox_messages`, so delivering to five
//!   recipients writes one message row and five membership rows.
//!
//! There is no network code in this crate.

pub mod address;
pub mod error;
pub mod models;
pub mod store;

pub use address::Address;
pub use error::{MailStoreError, Result};
pub use models::{Body, Flags, Folder, FolderKind, Mailbox, Message, OwnerKind, Transport};
pub use store::{content_hash, normalise_subject, ImapMessage, MailStore, MessageSummary, INLINE_BODY_LIMIT};
