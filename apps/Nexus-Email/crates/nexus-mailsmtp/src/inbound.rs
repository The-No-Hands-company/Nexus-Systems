use std::sync::Arc;

use nexus_maildelivery::Deliverer;
use nexus_mailauth::{authenticate, Authentication, Lookup, Verdict};
use nexus_mailstore::{Address, FolderKind, MailStore};

use crate::server::{Inbound, Sink};

/// What to do with mail a domain's own DMARC policy says to refuse.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum PolicyMode {
    /// Honour p=reject and p=quarantine. What a receiver is supposed to do.
    Enforce,
    /// Evaluate and record, but deliver everything. The right setting while
    /// bringing a server up: it produces the evidence to check the
    /// implementation against real mail before it can start losing any.
    Observe,
}

/// The Sink that authenticates inbound mail and then stores it.
pub struct AuthenticatingSink<L: Lookup + 'static> {
    pub store: MailStore,
    pub deliverer: Arc<Deliverer>,
    pub dns: Arc<L>,
    pub receiving_host: String,
    pub mode: PolicyMode,
}

impl<L: Lookup + 'static> Sink for AuthenticatingSink<L> {
    async fn deliver(&self, msg: Inbound<'_>) -> Result<Option<String>, String> {
        let auth = authenticate(
            &*self.dns,
            msg.client_ip,
            msg.helo,
            msg.from,
            msg.data,
        )
        .await;

        tracing::info!(
            ip = %msg.client_ip,
            from = %msg.from,
            spf = ?auth.spf,
            dkim = auth.dkim_passed,
            dmarc = auth.dmarc_pass,
            verdict = ?auth.verdict,
            "inbound authenticated"
        );

        if self.mode == PolicyMode::Enforce && auth.verdict == Verdict::Reject {
            // Refused at SMTP time rather than accepted and discarded. A
            // silent discard leaves a legitimate sender — and it is sometimes
            // a legitimate sender, misconfigured — believing the mail arrived.
            return Ok(Some(format!(
                "550 5.7.1 Message rejected by {} policy for {}\r\n",
                dmarc_policy_name(&auth),
                auth.dkim_domain.as_deref().unwrap_or("the sending domain")
            )));
        }

        // The evidence is prepended even on a pass. A later dispute about
        // whether a message was authentic is unanswerable if the result was
        // thrown away at delivery time.
        let mut stored = auth.header(&self.receiving_host).into_bytes();
        stored.extend_from_slice(msg.data);

        let quarantine =
            self.mode == PolicyMode::Enforce && auth.verdict == Verdict::Quarantine;

        for rcpt in msg.recipients {
            let address = Address::parse(rcpt).map_err(|e| e.to_string())?;
            self.accept_one(&address, &stored, msg.from, quarantine, &auth)
                .await?;
        }
        Ok(None)
    }
}

impl<L: Lookup + 'static> AuthenticatingSink<L> {
    async fn accept_one(
        &self,
        recipient: &Address,
        raw: &[u8],
        envelope_from: &str,
        quarantine: bool,
        _auth: &Authentication,
    ) -> Result<(), String> {
        let from = Address::parse(envelope_from)
            // A null reverse path is a bounce and is legal. It still needs an
            // address shaped value to record, so the recipient's own domain
            // stands in for one rather than the message being refused.
            .unwrap_or_else(|_| Address {
                localpart: "postmaster".into(),
                domain: recipient.domain.clone(),
            });

        let message_id = self
            .deliverer
            .accept_smtp(raw, &from, recipient)
            .await
            .map_err(|e| e.to_string())?;

        if quarantine {
            // Delivered, but set aside. The domain asked for suspicious mail to
            // be held rather than destroyed, and honouring that literally is
            // the whole difference between quarantine and reject.
            let mailbox = self
                .store
                .resolve(recipient)
                .await
                .map_err(|e| e.to_string())?;
            let junk = self
                .store
                .folder_named_or_create(mailbox, "Junk", FolderKind::Custom)
                .await
                .map_err(|e| e.to_string())?;
            self.store
                .move_to_folder(mailbox, message_id, junk)
                .await
                .map_err(|e| e.to_string())?;
        }
        Ok(())
    }
}

fn dmarc_policy_name(auth: &Authentication) -> &'static str {
    match auth.dmarc_policy {
        nexus_mailauth::Policy::Reject => "DMARC reject",
        nexus_mailauth::Policy::Quarantine => "DMARC quarantine",
        nexus_mailauth::Policy::None => "DMARC",
    }
}
