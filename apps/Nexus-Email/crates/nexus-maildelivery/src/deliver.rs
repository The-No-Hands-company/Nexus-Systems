use nexus_mailmsg::{parse, tree, Limits};
use nexus_mailstore::{
    Address, FolderKind, MailStore, MailStoreError, MessageReferenceKind, Transport,
};
use sqlx::{Postgres, Transaction};
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

struct PreparedMessage {
    thread_id: Uuid,
    subject: Option<String>,
    msg_id: Option<String>,
    in_reply_to: Option<String>,
    references: Vec<String>,
    search_text: Option<String>,
}

enum ResolvedRecipient {
    Local { mailbox: Uuid, folder: Uuid },
    Queued(Route),
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
        let prepared = self.prepare(raw).await?;
        let mut resolved = Vec::with_capacity(recipients.len());
        for rcpt in recipients {
            resolved.push(match self.router.route(rcpt) {
                Route::Local => match self.resolve_local(rcpt).await {
                    Ok((mailbox, folder)) => ResolvedRecipient::Local { mailbox, folder },
                    Err(DeliveryError::Permanent { reason, .. }) => {
                        ResolvedRecipient::Rejected(reason)
                    }
                    Err(e) => return Err(e),
                },
                route => ResolvedRecipient::Queued(route),
            });
        }
        let sender_copy = match sender_mailbox {
            Some(mailbox) => Some((mailbox, self.store.folder(mailbox, FolderKind::Sent).await?)),
            None => None,
        };
        let submission_id = Uuid::now_v7();
        let mut tx = self.store.begin_reference_transaction().await?;
        let message_id = self
            .store_prepared(
                &mut tx,
                raw,
                from,
                Transport::Internal,
                &prepared,
                submission_id,
            )
            .await?;
        let mut outcomes = Vec::with_capacity(recipients.len());
        for (rcpt, route) in recipients.iter().zip(resolved) {
            let (outcome, membership_id) = match route {
                ResolvedRecipient::Local { mailbox, folder } => {
                    let membership_id = self
                        .store
                        .deliver_in_transaction(&mut tx, mailbox, message_id, folder)
                        .await?;
                    (Disposition::DeliveredLocally, Some(membership_id))
                }
                ResolvedRecipient::Queued(route) => {
                    self.queue
                        .enqueue_in_transaction(
                            &mut tx,
                            message_id,
                            &from.as_string(),
                            &rcpt.as_string(),
                            &route,
                        )
                        .await?;
                    (Disposition::Queued, None)
                }
                ResolvedRecipient::Rejected(reason) => (Disposition::Rejected(reason), None),
            };
            let (disposition, reason) = match &outcome {
                Disposition::DeliveredLocally => ("mailbox", None),
                Disposition::Queued => ("outbound", None),
                Disposition::Rejected(reason) => ("rejected", Some(reason.as_str())),
            };
            sqlx::query(
                "INSERT INTO message_submission_dispositions
                 (id, submission_id, message_id, recipient, disposition, reason, membership_id)
                 VALUES ($1, $2, $3, $4, $5, $6, $7)",
            )
            .bind(Uuid::now_v7())
            .bind(submission_id)
            .bind(message_id)
            .bind(rcpt.as_string())
            .bind(disposition)
            .bind(reason)
            .bind(membership_id)
            .execute(&mut *tx)
            .await?;
            outcomes.push(Outcome {
                recipient: rcpt.as_string(),
                disposition: outcome,
            });
        }

        if let Some((mailbox, folder)) = sender_copy {
            self.store
                .deliver_in_transaction(&mut tx, mailbox, message_id, folder)
                .await?;
        }
        MailStore::release_reference_in_transaction(
            &mut tx,
            message_id,
            MessageReferenceKind::Submission,
            submission_id,
        )
        .await?;
        tx.commit().await?;
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

    async fn resolve_local(&self, rcpt: &Address) -> Result<(Uuid, Uuid)> {
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
        Ok((mailbox, inbox))
    }

    /// Accept a message that arrived over SMTP from the outside world.
    ///
    /// Separate from `accept_federated` because the provenance differs and is
    /// recorded: this came from an anonymous stranger, while a federated
    /// handover was vouched for by a peer we already trust. Later trust
    /// decisions depend on being able to tell them apart.
    pub async fn accept_smtp(
        &self,
        raw: &[u8],
        from: &Address,
        recipient: &Address,
    ) -> Result<Uuid> {
        self.accept_external(raw, from, recipient, Transport::Smtp).await
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
        self.accept_external(raw, from, recipient, Transport::Federated).await
    }

    /// Accept a message a pinned peer asked us to deliver to the outside
    /// world on its behalf — federated egress.
    ///
    /// Stored once, with `Transport::Federated` provenance, and queued for
    /// SMTP per recipient. Whether the peer is *allowed* to relay is the
    /// caller's decision (it knows the peer); this only refuses recipients
    /// that are not outside at all, which must be delivered, never relayed.
    pub async fn relay_for_peer(
        &self,
        raw: &[u8],
        from: &Address,
        recipients: &[Address],
    ) -> Result<Uuid> {
        for rcpt in recipients {
            if self.router.route(rcpt) != Route::External {
                return Err(DeliveryError::Permanent {
                    recipient: rcpt.as_string(),
                    reason: "not an outside address; deliver it, do not relay it".into(),
                });
            }
        }

        let prepared = self.prepare(raw).await?;
        let submission_id = Uuid::now_v7();
        let mut tx = self.store.begin_reference_transaction().await?;
        let message_id = self
            .store_prepared(
                &mut tx,
                raw,
                from,
                Transport::Federated,
                &prepared,
                submission_id,
            )
            .await?;
        for rcpt in recipients {
            self.queue
                .enqueue_in_transaction(
                    &mut tx,
                    message_id,
                    &from.as_string(),
                    &rcpt.as_string(),
                    &Route::External,
                )
                .await?;
            sqlx::query("INSERT INTO message_submission_dispositions (id, submission_id, message_id, recipient, disposition) VALUES ($1, $2, $3, $4, 'outbound')")
                .bind(Uuid::now_v7()).bind(submission_id).bind(message_id).bind(rcpt.as_string())
                .execute(&mut *tx).await?;
        }
        MailStore::release_reference_in_transaction(
            &mut tx,
            message_id,
            MessageReferenceKind::Submission,
            submission_id,
        )
        .await?;
        tx.commit().await?;
        Ok(message_id)
    }

    async fn accept_external(
        &self,
        raw: &[u8],
        from: &Address,
        recipient: &Address,
        transport: Transport,
    ) -> Result<Uuid> {
        if !self.router.is_local_domain(&recipient.domain) {
            // Accepting mail for a domain we do not serve would make this node
            // an open relay for the federation.
            return Err(DeliveryError::Permanent {
                recipient: recipient.as_string(),
                reason: "this node does not serve that domain".into(),
            });
        }

        let (mailbox, folder) = self.resolve_local(recipient).await?;
        let prepared = self.prepare(raw).await?;
        let submission_id = Uuid::now_v7();
        let mut tx = self.store.begin_reference_transaction().await?;
        let message_id = self
            .store_prepared(&mut tx, raw, from, transport, &prepared, submission_id)
            .await?;
        let membership_id = self
            .store
            .deliver_in_transaction(&mut tx, mailbox, message_id, folder)
            .await?;
        sqlx::query("INSERT INTO message_submission_dispositions (id, submission_id, message_id, recipient, disposition, membership_id) VALUES ($1, $2, $3, $4, 'mailbox', $5)")
            .bind(Uuid::now_v7()).bind(submission_id).bind(message_id).bind(recipient.as_string())
            .bind(membership_id)
            .execute(&mut *tx).await?;
        MailStore::release_reference_in_transaction(
            &mut tx,
            message_id,
            MessageReferenceKind::Submission,
            submission_id,
        )
        .await?;
        tx.commit().await?;
        Ok(message_id)
    }

    async fn prepare(&self, raw: &[u8]) -> Result<PreparedMessage> {
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

        Ok(PreparedMessage {
            thread_id,
            subject,
            msg_id,
            in_reply_to,
            references,
            search_text: Self::searchable_text(raw),
        })
    }

    async fn store_prepared(
        &self,
        tx: &mut Transaction<'_, Postgres>,
        raw: &[u8],
        from: &Address,
        transport: Transport,
        prepared: &PreparedMessage,
        submission_id: Uuid,
    ) -> Result<Uuid> {
        let message_id = self
            .store
            .store_message_in_transaction(
                tx,
                raw,
                prepared.thread_id,
                from,
                prepared.subject.as_deref(),
                prepared.msg_id.as_deref(),
                prepared.in_reply_to.as_deref(),
                &prepared.references,
                transport,
                None,
                Some(submission_id),
            )
            .await?;

        if let Some(text) = &prepared.search_text {
            self.store
                .set_search_text_in_transaction(tx, message_id, text)
                .await?;
        }
        Ok(message_id)
    }
}
