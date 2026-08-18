use nexus_mailmsg::{parse, tree, Limits};
use nexus_mailstore::{Address, FolderKind, MailStore, MailStoreError, Transport};
use uuid::Uuid;

use crate::error::{DeliveryError, Result};
use crate::queue::Queue;
use crate::route::{Route, Router};

/// What happened to each recipient of a submission.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Outcome {
    pub recipient: String,
    pub disposition: Disposition,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Disposition {
    /// Written into a mailbox on this node.
    DeliveredLocally,
    /// Accepted into the outbound queue for a peer or the outside world.
    Queued,
    /// Refused outright, with a reason the sender can act on.
    Rejected(String),
}

/// Ties the store, the router and the queue together: the thing that actually
/// delivers a message.
pub struct Deliverer {
    store: MailStore,
    queue: Queue,
    router: Router,
}

impl Deliverer {
    pub fn new(store: MailStore, queue: Queue, router: Router) -> Self {
        Self { store, queue, router }
    }

    pub fn router(&self) -> &Router {
        &self.router
    }

    /// Submit a message from a local sender to any set of recipients.
    ///
    /// Recipients are handled independently and the result says what happened
    /// to each. One bad address must not stop delivery to the other four, and
    /// the caller needs to know precisely which one failed to tell the sender.
    pub async fn submit(
        &self,
        raw: &[u8],
        from: &Address,
        recipients: &[Address],
        sender_mailbox: Option<Uuid>,
    ) -> Result<Vec<Outcome>> {
        let parsed = parse(raw, Limits::default())?;
        let subject = parsed.headers.get("subject").map(str::to_string);
        let msg_id = parsed.headers.get("message-id").map(str::to_string);
        let in_reply_to = parsed.headers.get("in-reply-to").map(str::to_string);
        let references: Vec<String> = parsed
            .headers
            .get("references")
            .map(|r| r.split_whitespace().map(str::to_string).collect())
            .unwrap_or_default();

        let thread_id = self
            .store
            .thread_for(in_reply_to.as_deref(), &references, subject.as_deref())
            .await?;

        // Stored once, before any routing. Every recipient — local, federated
        // or external — references this single row.
        let message_id = self
            .store
            .store_message(
                raw,
                thread_id,
                from,
                subject.as_deref(),
                msg_id.as_deref(),
                in_reply_to.as_deref(),
                &references,
                Transport::Internal,
                None,
            )
            .await?;

        if let Some(text) = Self::searchable_text(raw) {
            self.store.set_search_text(message_id, &text).await?;
        }

        let mut outcomes = Vec::with_capacity(recipients.len());
        for rcpt in recipients {
            let outcome = match self.router.route(rcpt) {
                Route::Local => match self.deliver_local(message_id, rcpt).await {
                    Ok(()) => Disposition::DeliveredLocally,
                    Err(DeliveryError::Permanent { reason, .. }) => Disposition::Rejected(reason),
                    Err(e) => return Err(e),
                },
                route => {
                    self.queue
                        .enqueue(message_id, &from.as_string(), &rcpt.as_string(), &route)
                        .await?;
                    Disposition::Queued
                }
            };
            outcomes.push(Outcome { recipient: rcpt.as_string(), disposition: outcome });
        }

        // The sender keeps a copy. Same message row again — a sent message is
        // not a second copy of itself.
        if let Some(mailbox) = sender_mailbox {
            let sent = self.store.folder(mailbox, FolderKind::Sent).await?;
            self.store.deliver(mailbox, message_id, sent).await?;
        }

        Ok(outcomes)
    }

    /// The displayable text of a message, for the search index.
    ///
    /// Prefers text/plain over text/html: the plain alternative is the same
    /// content without markup, and indexing HTML means indexing tag names and
    /// inline styles, which match everything and mean nothing.
    fn searchable_text(raw: &[u8]) -> Option<String> {
        let parsed = parse(raw, Limits::default()).ok()?;
        let root = tree(&parsed);
        let parts = root.walk();

        let pick = |mime: &str| -> Option<String> {
            parts
                .iter()
                .find(|p| p.content_type.mime_type == mime && !p.is_attachment())
                .map(|p| p.text())
        };

        pick("text/plain").or_else(|| pick("text/html")).or_else(|| {
            // A message with no recognised text part still has a subject and a
            // sender indexed; returning None here simply adds nothing more.
            (!root.body.is_empty()).then(|| root.text())
        })
    }

    /// Deliver an already-stored message into a local mailbox's INBOX.
    async fn deliver_local(&self, message_id: Uuid, rcpt: &Address) -> Result<()> {
        let mailbox = match self.store.resolve(rcpt).await {
            Ok(id) => id,
            // "No mailbox here" is a permanent answer, not an error: the
            // address will not start existing on a retry, and the sender needs
            // to be told now rather than in five days.
            Err(MailStoreError::NoSuchAddress(a)) => {
                return Err(DeliveryError::Permanent {
                    recipient: a,
                    reason: "no such mailbox on this node".into(),
                })
            }
            Err(e) => return Err(e.into()),
        };

        let inbox = self.store.folder(mailbox, FolderKind::Inbox).await?;
        self.store.deliver(mailbox, message_id, inbox).await?;
        Ok(())
    }

    /// Accept a message handed to us by a peer node.
    ///
    /// The mirror of federated sending. Recorded with `Transport::Federated`
    /// so later trust decisions can tell it apart from something an anonymous
    /// stranger sent over SMTP.
    pub async fn accept_federated(
        &self,
        raw: &[u8],
        from: &Address,
        recipient: &Address,
    ) -> Result<Uuid> {
        if !self.router.is_local_domain(&recipient.domain) {
            // Accepting mail for a domain we do not serve would make this node
            // an open relay for the federation.
            return Err(DeliveryError::Permanent {
                recipient: recipient.as_string(),
                reason: "this node does not serve that domain".into(),
            });
        }

        let parsed = parse(raw, Limits::default())?;
        let subject = parsed.headers.get("subject").map(str::to_string);
        let msg_id = parsed.headers.get("message-id").map(str::to_string);
        let in_reply_to = parsed.headers.get("in-reply-to").map(str::to_string);
        let references: Vec<String> = parsed
            .headers
            .get("references")
            .map(|r| r.split_whitespace().map(str::to_string).collect())
            .unwrap_or_default();

        let thread_id = self
            .store
            .thread_for(in_reply_to.as_deref(), &references, subject.as_deref())
            .await?;

        let message_id = self
            .store
            .store_message(
                raw,
                thread_id,
                from,
                subject.as_deref(),
                msg_id.as_deref(),
                in_reply_to.as_deref(),
                &references,
                Transport::Federated,
                None,
            )
            .await?;

        if let Some(text) = Self::searchable_text(raw) {
            self.store.set_search_text(message_id, &text).await?;
        }

        self.deliver_local(message_id, recipient).await?;
        Ok(message_id)
    }
}
