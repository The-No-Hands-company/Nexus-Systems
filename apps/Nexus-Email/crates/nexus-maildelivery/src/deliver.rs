use nexus_mailmsg::{parse, tree, Limits};
use nexus_mailstore::{Address, FolderKind, MailStore, MailStoreError, Transport};
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
    subject: Option<String>,
    msg_id: Option<String>,
    in_reply_to: Option<String>,
    references: Vec<String>,
    search_text: Option<String>,
}

struct SubmissionMessage<'a> {
    raw: &'a [u8],
    from: &'a Address,
    transport: Transport,
    prepared: &'a PreparedMessage,
}

enum ResolvedRecipient {
    Local {
        mailbox: Uuid,
        folder: Uuid,
        force_folder: bool,
    },
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
        let prepared = self.prepare(raw)?;
        let mut resolved = Vec::with_capacity(recipients.len());
        for rcpt in recipients {
            resolved.push(match self.router.route(rcpt) {
                Route::Local => match self.resolve_local(rcpt).await {
                    Ok((mailbox, folder)) => ResolvedRecipient::Local { mailbox, folder, force_folder: false },
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
        let recipient_strings: Vec<String> = recipients.iter().map(Address::as_string).collect();
        let (_, outcomes) = self
            .persist_resolved(
                SubmissionMessage {
                    raw,
                    from,
                    transport: Transport::Internal,
                    prepared: &prepared,
                },
                &recipient_strings,
                resolved,
                sender_copy,
            )
            .await?;
        Ok(outcomes)
    }

    async fn persist_resolved(
        &self,
        message: SubmissionMessage<'_>,
        recipients: &[String],
        resolved: Vec<ResolvedRecipient>,
        sender_copy: Option<(Uuid, Uuid)>,
    ) -> Result<(Uuid, Vec<Outcome>)> {
        let SubmissionMessage { raw, from, transport, prepared } = message;
        let mut tx = self.store.begin_reference_transaction().await?;
        let message_id = self
            .store_prepared(
                &mut tx,
                raw,
                from,
                transport,
                prepared,
            )
            .await?;
        let submission_id = Self::ensure_submission(&mut tx, message_id).await?;
        let mut existing_queue = Self::existing_queue(&mut tx, message_id, recipients).await?;
        let mut mailboxes: Vec<Uuid> = resolved
            .iter()
            .filter_map(|route| match route {
                ResolvedRecipient::Local { mailbox, .. } => Some(*mailbox),
                _ => None,
            })
            .collect();
        if let Some((mailbox, _)) = sender_copy {
            mailboxes.push(mailbox);
        }
        Self::lock_mailboxes(&mut tx, &mailboxes).await?;
        let mut outcomes = Vec::with_capacity(recipients.len());
        for (rcpt, route) in recipients.iter().zip(resolved) {
            let (outcome, route_name, state, membership_id, queue_id) = match route {
                ResolvedRecipient::Local { mailbox, folder, force_folder } => {
                    let membership_id = self
                        .store
                        .deliver_in_transaction(&mut tx, mailbox, message_id, folder)
                        .await?;
                    if force_folder {
                        sqlx::query(
                            "UPDATE mailbox_messages SET folder_id = $2, trash_expires_at = NULL
                             WHERE id = $1",
                        ).bind(membership_id).bind(folder).execute(&mut *tx).await?;
                    }
                    (Disposition::DeliveredLocally, "local", "delivered", Some(membership_id), None)
                }
                ResolvedRecipient::Queued(route) => {
                    let route_name = match route {
                        Route::Federated { .. } => "federated",
                        Route::External => "smtp",
                        Route::Local => unreachable!(),
                    };
                    if Self::consume_existing(&mut existing_queue, rcpt) {
                        outcomes.push(Outcome {
                            recipient: rcpt.clone(),
                            disposition: Disposition::Queued,
                        });
                        continue;
                    }
                    let queue_id = self.queue
                        .enqueue_submission_in_transaction(
                            &mut tx,
                            message_id,
                            &from.as_string(),
                            rcpt,
                            &route,
                        )
                        .await?;
                    (Disposition::Queued, route_name, "outbound_pending", None, Some(queue_id))
                }
                ResolvedRecipient::Rejected(reason) =>
                    (Disposition::Rejected(reason), "local", "failed", None, None),
            };
            sqlx::query(
                "INSERT INTO mail_submission_recipients
                 (id, submission_id, recipient, route, state, membership_id, queue_id)
                 VALUES ($1, $2, $3, $4, $5, $6, $7)",
            )
            .bind(Uuid::now_v7())
            .bind(submission_id)
            .bind(rcpt)
            .bind(route_name)
            .bind(state)
            .bind(membership_id)
            .bind(queue_id)
            .execute(&mut *tx)
            .await?;
            outcomes.push(Outcome {
                recipient: rcpt.clone(),
                disposition: outcome,
            });
        }

        if let Some((mailbox, folder)) = sender_copy {
            self.store
                .deliver_in_transaction(&mut tx, mailbox, message_id, folder)
                .await?;
        }
        MailStore::erase_orphaned_message_in_transaction(&mut tx, message_id).await?;
        tx.commit().await?;
        Ok((message_id, outcomes))
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
        self.accept_smtp_envelope(raw, from, std::slice::from_ref(recipient), false)
            .await
    }

    /// Accept all local SMTP recipients in one transaction.
    pub async fn accept_smtp_envelope(
        &self,
        raw: &[u8],
        from: &Address,
        recipients: &[Address],
        quarantine: bool,
    ) -> Result<Uuid> {
        self.accept_external(raw, from, recipients, Transport::Smtp, quarantine)
            .await
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
        self.accept_external(
            raw,
            from,
            std::slice::from_ref(recipient),
            Transport::Federated,
            false,
        )
        .await
    }

    /// Resolve a peer's complete envelope before committing local and relay work.
    pub async fn accept_federated_envelope(
        &self,
        raw: &[u8],
        from: &Address,
        recipients: &[String],
        may_relay: bool,
    ) -> Result<Vec<Outcome>> {
        let prepared = self.prepare(raw)?;
        let mut resolved = Vec::with_capacity(recipients.len());
        let mut normalized = Vec::with_capacity(recipients.len());
        for raw_recipient in recipients {
            let recipient = match Address::parse(raw_recipient) {
                Ok(recipient) => recipient,
                Err(_) => {
                    normalized.push(raw_recipient.clone());
                    resolved.push(ResolvedRecipient::Rejected("malformed address".into()));
                    continue;
                }
            };
            normalized.push(recipient.as_string());
            resolved.push(match self.router.route(&recipient) {
                Route::Local => match self.resolve_local(&recipient).await {
                    Ok((mailbox, folder)) => ResolvedRecipient::Local {
                        mailbox,
                        folder,
                        force_folder: false,
                    },
                    Err(DeliveryError::Permanent { reason, .. }) => {
                        ResolvedRecipient::Rejected(reason)
                    }
                    Err(e) => return Err(e),
                },
                Route::Federated { .. } => {
                    ResolvedRecipient::Rejected("mail is not forwarded between peers".into())
                }
                Route::External if may_relay => ResolvedRecipient::Queued(Route::External),
                Route::External => {
                    ResolvedRecipient::Rejected("this peer may not relay through this node".into())
                }
            });
        }
        let (_, outcomes) = self
            .persist_resolved(
                SubmissionMessage {
                    raw,
                    from,
                    transport: Transport::Federated,
                    prepared: &prepared,
                },
                &normalized,
                resolved,
                None,
            )
            .await?;
        Ok(outcomes)
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
        if recipients.is_empty() {
            return Err(DeliveryError::Permanent {
                recipient: String::new(),
                reason: "no relay recipients".into(),
            });
        }
        for rcpt in recipients {
            if self.router.route(rcpt) != Route::External {
                return Err(DeliveryError::Permanent {
                    recipient: rcpt.as_string(),
                    reason: "not an outside address; deliver it, do not relay it".into(),
                });
            }
        }

        let prepared = self.prepare(raw)?;
        let mut tx = self.store.begin_reference_transaction().await?;
        let message_id = self
            .store_prepared(
                &mut tx,
                raw,
                from,
                Transport::Federated,
                &prepared,
            )
            .await?;
        let submission_id = Self::ensure_submission(&mut tx, message_id).await?;
        let recipient_strings: Vec<String> = recipients.iter().map(Address::as_string).collect();
        let mut existing_queue =
            Self::existing_queue(&mut tx, message_id, &recipient_strings).await?;
        for rcpt in recipients {
            if Self::consume_existing(&mut existing_queue, &rcpt.as_string()) {
                continue;
            }
            let queue_id = self.queue
                .enqueue_submission_in_transaction(
                    &mut tx,
                    message_id,
                    &from.as_string(),
                    &rcpt.as_string(),
                    &Route::External,
                )
                .await?;
            sqlx::query("INSERT INTO mail_submission_recipients (id, submission_id, recipient, route, state, queue_id) VALUES ($1, $2, $3, 'smtp', 'outbound_pending', $4)")
                .bind(Uuid::now_v7()).bind(submission_id).bind(rcpt.as_string()).bind(queue_id)
                .execute(&mut *tx).await?;
        }
        MailStore::erase_orphaned_message_in_transaction(&mut tx, message_id).await?;
        tx.commit().await?;
        Ok(message_id)
    }

    async fn accept_external(
        &self,
        raw: &[u8],
        from: &Address,
        recipients: &[Address],
        transport: Transport,
        quarantine: bool,
    ) -> Result<Uuid> {
        if recipients.is_empty() {
            return Err(DeliveryError::Permanent {
                recipient: String::new(),
                reason: "no recipients".into(),
            });
        }
        let mut resolved = Vec::with_capacity(recipients.len());
        for recipient in recipients {
            if !self.router.is_local_domain(&recipient.domain) {
                return Err(DeliveryError::Permanent {
                    recipient: recipient.as_string(),
                    reason: "this node does not serve that domain".into(),
                });
            }
            let (mailbox, mut folder) = self.resolve_local(recipient).await?;
            if quarantine {
                folder = self
                    .store
                    .folder_named_or_create(mailbox, "Junk", FolderKind::Custom)
                    .await?;
            }
            resolved.push(ResolvedRecipient::Local {
                mailbox,
                folder,
                force_folder: quarantine,
            });
        }
        let prepared = self.prepare(raw)?;
        let recipient_strings: Vec<String> = recipients.iter().map(Address::as_string).collect();
        let (message_id, _) = self
            .persist_resolved(
                SubmissionMessage {
                    raw,
                    from,
                    transport,
                    prepared: &prepared,
                },
                &recipient_strings,
                resolved,
                None,
            )
            .await?;
        Ok(message_id)
    }

    fn prepare(&self, raw: &[u8]) -> Result<PreparedMessage> {
        let parsed = parse(raw, Limits::default())?;
        let subject = parsed.headers.get("subject").map(str::to_string);
        let msg_id = parsed.headers.get("message-id").map(str::to_string);
        let in_reply_to = parsed.headers.get("in-reply-to").map(str::to_string);
        let references: Vec<String> = parsed
            .headers
            .get("references")
            .map(|r| r.split_whitespace().map(str::to_string).collect())
            .unwrap_or_default();

        Ok(PreparedMessage {
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
    ) -> Result<Uuid> {
        let thread_id = self
            .store
            .thread_for_in_transaction(
                tx,
                prepared.in_reply_to.as_deref(),
                &prepared.references,
                prepared.subject.as_deref(),
            )
            .await?;
        let message_id = self
            .store
            .store_submission_message_in_transaction(
                tx,
                raw,
                thread_id,
                from,
                prepared.subject.as_deref(),
                prepared.msg_id.as_deref(),
                prepared.in_reply_to.as_deref(),
                &prepared.references,
                transport,
                None,
            )
            .await?;

        let (persisted_thread,): (Uuid,) =
            sqlx::query_as("SELECT thread_id FROM messages WHERE id = $1")
                .bind(message_id)
                .fetch_one(&mut **tx)
                .await?;
        if persisted_thread != thread_id {
            MailStore::delete_unreferenced_thread_in_transaction(tx, thread_id).await?;
        }
        if let Some(text) = &prepared.search_text {
            self.store
                .set_search_text_in_transaction(tx, message_id, text)
                .await?;
        }
        Ok(message_id)
    }

    async fn ensure_submission(
        tx: &mut Transaction<'_, Postgres>,
        message_id: Uuid,
    ) -> Result<Uuid> {
        Ok(MailStore::ensure_submission_in_transaction(tx, message_id).await?)
    }

    async fn lock_mailboxes(tx: &mut Transaction<'_, Postgres>, ids: &[Uuid]) -> Result<()> {
        let locked: Vec<(Uuid,)> =
            sqlx::query_as("SELECT id FROM mailboxes WHERE id = ANY($1) ORDER BY id FOR UPDATE")
                .bind(ids)
                .fetch_all(&mut **tx)
                .await?;
        let mut unique = ids.to_vec();
        unique.sort_unstable();
        unique.dedup();
        if locked.len() != unique.len() {
            return Err(sqlx::Error::RowNotFound.into());
        }
        Ok(())
    }

    async fn existing_queue(
        tx: &mut Transaction<'_, Postgres>,
        message_id: Uuid,
        recipients: &[String],
    ) -> Result<Vec<String>> {
        Ok(sqlx::query_scalar(
            "SELECT candidate.recipient
             FROM mail_submission_recipients r
             JOIN mail_submissions s ON s.id = r.submission_id
             JOIN LATERAL (
                 SELECT c.recipient FROM unnest($2::text[]) AS c(recipient)
                 WHERE c.recipient = r.recipient OR
                       r.retry_fingerprint = sha256(
                           uuid_send(s.id) || convert_to(r.route || ':' || c.recipient, 'UTF8')
                       )
                 LIMIT 1
             ) candidate ON true
             WHERE s.message_id = $1 AND r.route IN ('smtp', 'federated')
             ORDER BY r.created_at, r.id",
        )
        .bind(message_id)
        .bind(recipients)
        .fetch_all(&mut **tx)
        .await?)
    }

    fn consume_existing(existing: &mut Vec<String>, recipient: &str) -> bool {
        if let Some(index) = existing.iter().position(|r| r == recipient) {
            existing.remove(index);
            true
        } else {
            false
        }
    }
}
