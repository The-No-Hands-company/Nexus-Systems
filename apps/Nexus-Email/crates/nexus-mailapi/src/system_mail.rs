//! Mail the platform itself sends: an invitation, a notice that an access
//! request was approved.
//!
//! Only Nexus-Auth calls this, over loopback, with a service token. The caller
//! names a template and supplies a recipient and a link; it supplies no free
//! text. Subject and body are composed here, and the link must point at this
//! node's own identity host on the template's own path. Together those make it
//! impossible to use the endpoint to send arbitrary mail to arbitrary people,
//! which is what would turn it into a spam relay.

use std::sync::Mutex;

use chrono::{DateTime, NaiveDate, Utc};
use nexus_mailstore::Address;
use serde::Deserialize;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SystemTemplate {
    Invite,
    AccessApproved,
}

impl SystemTemplate {
    fn parse(name: &str) -> Option<Self> {
        match name {
            "invite" => Some(Self::Invite),
            "access-approved" => Some(Self::AccessApproved),
            _ => None,
        }
    }
}

/// The request body, exactly as it arrives. Nothing in it is trusted until
/// [`parse_system_mail`] has checked it.
#[derive(Debug, Deserialize)]
pub struct SystemMailRequest {
    pub template: String,
    pub to: String,
    pub link: String,
    #[serde(rename = "expiresAt")]
    pub expires_at: Option<String>,
}

/// A request that passed every check.
#[derive(Debug)]
pub struct SystemMail {
    pub template: SystemTemplate,
    pub to: Address,
    pub link: String,
    pub expires_at: Option<DateTime<Utc>>,
}

#[derive(Debug, PartialEq, Eq)]
pub enum SystemMailRejection {
    UnknownTemplate,
    InvalidRecipient,
    LinkNotAllowed,
    InvalidExpiry,
}

impl SystemMailRejection {
    pub fn message(&self) -> &'static str {
        match self {
            Self::UnknownTemplate => "unknown template",
            Self::InvalidRecipient => "invalid recipient",
            Self::LinkNotAllowed => "link is not an identity-host link for this template",
            Self::InvalidExpiry => "expiresAt must be an RFC 3339 timestamp",
        }
    }
}

const MAX_LINK_LEN: usize = 512;

pub fn parse_system_mail(
    req: &SystemMailRequest,
    primary_domain: &str,
) -> Result<SystemMail, SystemMailRejection> {
    let template = SystemTemplate::parse(&req.template).ok_or(SystemMailRejection::UnknownTemplate)?;
    if req.to.len() > 254 || req.to.chars().any(|c| c.is_control() || c.is_whitespace()) {
        return Err(SystemMailRejection::InvalidRecipient);
    }
    let to = Address::parse(&req.to).map_err(|_| SystemMailRejection::InvalidRecipient)?;
    if !is_allowed_link(template, &req.link, primary_domain) {
        return Err(SystemMailRejection::LinkNotAllowed);
    }
    let expires_at = match &req.expires_at {
        None => None,
        Some(raw) => Some(
            DateTime::parse_from_rfc3339(raw)
                .map_err(|_| SystemMailRejection::InvalidExpiry)?
                .with_timezone(&Utc),
        ),
    };
    Ok(SystemMail { template, to, link: req.link.clone(), expires_at })
}

/// The only links a template may carry: its own page on this node's identity
/// host, with a query in the exact shape that page reads.
fn is_allowed_link(template: SystemTemplate, link: &str, primary_domain: &str) -> bool {
    if link.len() > MAX_LINK_LEN {
        return false;
    }
    let base = format!("https://auth.{}/", primary_domain.to_ascii_lowercase());
    let Some(rest) = link.strip_prefix(&base) else { return false };
    match template {
        SystemTemplate::Invite => rest
            .strip_prefix("join?code=")
            .is_some_and(|code| !code.is_empty() && code.len() <= 128 && code.bytes().all(|b| b.is_ascii_hexdigit())),
        SystemTemplate::AccessApproved => rest.strip_prefix("claim?email=").is_some_and(|email| {
            !email.is_empty()
                && email.bytes().all(|b| b.is_ascii_alphanumeric() || matches!(b, b'%' | b'.' | b'_' | b'-' | b'+'))
        }),
    }
}

pub struct ComposedSystemMail {
    pub subject: String,
    pub text: String,
}

pub fn compose_system_mail(mail: &SystemMail, primary_domain: &str) -> ComposedSystemMail {
    match mail.template {
        SystemTemplate::Invite => {
            let expiry = mail
                .expires_at
                .map(|at| format!(" It expires on {} (UTC).", at.format("%-d %B %Y")))
                .unwrap_or_default();
            ComposedSystemMail {
                subject: "You're invited to Nexus".into(),
                text: format!(
                    "You have been invited to join Nexus, the free, privacy-focused cloud from \
The No Hands Company.\n\n\
Create your account here:\n{link}\n\n\
This invitation works once, and only for this email address.{expiry}\n\n\
If you were not expecting it, you can ignore this email. Nothing happens unless the link is used.\n\n\
Nexus · {domain}\n",
                    link = mail.link,
                    domain = primary_domain,
                ),
            }
        }
        SystemTemplate::AccessApproved => ComposedSystemMail {
            subject: "Your Nexus access request was approved".into(),
            text: format!(
                "Your request to join Nexus was approved.\n\n\
Claim your account here, using the claim code you were shown when you asked:\n{link}\n\n\
Lost the claim code? Ask whoever runs this Nexus node for an invitation instead.\n\n\
Nexus · {domain}\n",
                link = mail.link,
                domain = primary_domain,
            ),
        },
    }
}

/// How many system messages may go out per UTC day. A safety net, not a
/// quota: it bounds what a leaked service token could send, and keeps invites
/// from spending the daily allowance of the outbound relay that people's own
/// mail depends on.
pub struct SystemMailBudget {
    limit: u32,
    spent: Mutex<(NaiveDate, u32)>,
}

impl SystemMailBudget {
    pub fn new(limit: u32) -> Self {
        Self { limit, spent: Mutex::new((NaiveDate::MIN, 0)) }
    }

    /// Spend one message from today's budget; false when it is used up.
    pub fn try_spend(&self, today: NaiveDate) -> bool {
        let mut spent = self.spent.lock().unwrap_or_else(|poisoned| poisoned.into_inner());
        if spent.0 != today {
            *spent = (today, 0);
        }
        if spent.1 >= self.limit {
            return false;
        }
        spent.1 += 1;
        true
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const DOMAIN: &str = "tnhc.dev";
    const CODE: &str = "0123456789abcdef0123456789abcdef";  // pragma: allowlist secret (fake test code)

    fn request(template: &str, to: &str, link: &str) -> SystemMailRequest {
        SystemMailRequest { template: template.into(), to: to.into(), link: link.into(), expires_at: None }
    }

    #[test]
    fn test_parse_system_mail_when_invite_is_well_formed_should_accept() {
        let req = request("invite", "ada@example.com", &format!("https://auth.tnhc.dev/join?code={CODE}"));
        let mail = parse_system_mail(&req, DOMAIN).unwrap();
        assert_eq!(mail.template, SystemTemplate::Invite);
        assert_eq!(mail.to.to_string(), "ada@example.com");
    }

    #[test]
    fn test_parse_system_mail_when_template_is_unknown_should_reject() {
        let req = request("newsletter", "ada@example.com", &format!("https://auth.tnhc.dev/join?code={CODE}"));
        assert_eq!(parse_system_mail(&req, DOMAIN).unwrap_err(), SystemMailRejection::UnknownTemplate);
    }

    #[test]
    fn test_parse_system_mail_when_link_leaves_the_identity_host_should_reject() {
        for link in [
            format!("https://evil.example/join?code={CODE}"),
            format!("http://auth.tnhc.dev/join?code={CODE}"),
            format!("https://auth.tnhc.dev.evil.example/join?code={CODE}"),
            format!("https://app.tnhc.dev/join?code={CODE}"),
            "https://auth.tnhc.dev/claim?email=ada%40example.com".to_string(),
            "https://auth.tnhc.dev/join?code=".to_string(),
            "https://auth.tnhc.dev/join?code=abc\"><script>".to_string(),
            format!("https://auth.tnhc.dev/join?code={CODE}&next=https://evil.example"),
        ] {
            let req = request("invite", "ada@example.com", &link);
            assert_eq!(parse_system_mail(&req, DOMAIN).unwrap_err(), SystemMailRejection::LinkNotAllowed, "{link}");
        }
    }

    #[test]
    fn test_parse_system_mail_when_approval_link_is_a_claim_link_should_accept() {
        let req = request("access-approved", "ada@example.com", "https://auth.tnhc.dev/claim?email=ada%40example.com");
        assert!(parse_system_mail(&req, DOMAIN).is_ok());
        let wrong = request("access-approved", "ada@example.com", &format!("https://auth.tnhc.dev/join?code={CODE}"));
        assert_eq!(parse_system_mail(&wrong, DOMAIN).unwrap_err(), SystemMailRejection::LinkNotAllowed);
    }

    #[test]
    fn test_parse_system_mail_when_recipient_is_malformed_should_reject() {
        for to in ["", "not-an-address", "ada@example.com\r\nBcc: victim@example.com", "a b@example.com"] {
            let req = request("invite", to, &format!("https://auth.tnhc.dev/join?code={CODE}"));
            assert_eq!(parse_system_mail(&req, DOMAIN).unwrap_err(), SystemMailRejection::InvalidRecipient, "{to:?}");
        }
    }

    #[test]
    fn test_parse_system_mail_when_expiry_is_not_rfc3339_should_reject() {
        let mut req = request("invite", "ada@example.com", &format!("https://auth.tnhc.dev/join?code={CODE}"));
        req.expires_at = Some("next tuesday".into());
        assert_eq!(parse_system_mail(&req, DOMAIN).unwrap_err(), SystemMailRejection::InvalidExpiry);
    }

    #[test]
    fn test_compose_system_mail_when_invite_has_expiry_should_state_link_and_date() {
        let mut req = request("invite", "ada@example.com", &format!("https://auth.tnhc.dev/join?code={CODE}"));
        req.expires_at = Some("2026-10-20T12:00:00Z".into());
        let composed = compose_system_mail(&parse_system_mail(&req, DOMAIN).unwrap(), DOMAIN);
        assert_eq!(composed.subject, "You're invited to Nexus");
        assert!(composed.text.contains(&format!("https://auth.tnhc.dev/join?code={CODE}")));
        assert!(composed.text.contains("20 October 2026"));
        assert!(composed.text.contains("only for this email address"));
    }

    #[test]
    fn test_compose_system_mail_when_approved_should_not_carry_a_claim_code() {
        let req = request("access-approved", "ada@example.com", "https://auth.tnhc.dev/claim?email=ada%40example.com");
        let composed = compose_system_mail(&parse_system_mail(&req, DOMAIN).unwrap(), DOMAIN);
        assert!(composed.text.contains("/claim?email=ada%40example.com"));
        assert!(!composed.text.to_lowercase().contains("code:"));
    }

    #[test]
    fn test_system_mail_budget_when_limit_is_reached_should_refuse_until_the_next_day() {
        let budget = SystemMailBudget::new(2);
        let today = NaiveDate::from_ymd_opt(2026, 10, 13).unwrap();
        assert!(budget.try_spend(today));
        assert!(budget.try_spend(today));
        assert!(!budget.try_spend(today));
        assert!(budget.try_spend(today.succ_opt().unwrap()));
    }
}
