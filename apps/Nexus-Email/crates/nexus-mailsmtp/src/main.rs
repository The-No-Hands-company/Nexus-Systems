//! The Nexus SMTP daemon.
//!
//! Two listeners with different rules: an MX port for anonymous strangers,
//! which may only deliver to mailboxes we host, and a submission port for our
//! own users, which requires authentication and may send anywhere.

use std::sync::Arc;

use nexus_maildelivery::{Deliverer, Queue, Router};
use nexus_mailauth::SystemDns;
use nexus_mailsmtp::inbound::{AuthenticatingSink, PolicyMode};
use nexus_mailsmtp::policy::MailboxPolicy;
use nexus_mailimap::ImapServer;
use nexus_mailsmtp::authenticator::AuthService;
use nexus_mailsmtp::{Limits, Role, SmtpServer};
use nexus_mailstore::MailStore;
use sqlx::postgres::PgPoolOptions;
use tokio::net::TcpListener;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "nexus_mailsmtpd=info,nexus_mailsmtp=info".into()),
        )
        .init();

    let database_url = std::env::var("NEXUS_EMAIL_DATABASE_URL")
        .map_err(|_| "NEXUS_EMAIL_DATABASE_URL must be set")?;
    let domain = std::env::var("NEXUS_EMAIL_DOMAIN").unwrap_or_else(|_| "tnhc.dev".into());
    let hostname = std::env::var("NEXUS_EMAIL_HOSTNAME").unwrap_or_else(|_| format!("mail.{domain}"));

    // Observe by default, and deliberately so. Enforce refuses mail on the
    // strength of this implementation's reading of somebody else's DNS; it
    // should be switched on once its Authentication-Results headers have been
    // read against real traffic, not on the day it first runs.
    let mode = match std::env::var("NEXUS_EMAIL_POLICY").unwrap_or_default().as_str() {
        "enforce" => PolicyMode::Enforce,
        _ => PolicyMode::Observe,
    };

    // Defaults are unprivileged. Binding 25 needs root or
    // CAP_NET_BIND_SERVICE, and a daemon that refuses to start because it
    // cannot bind a privileged port is worse than one that runs where it can
    // and says so.
    let mx_bind = std::env::var("NEXUS_EMAIL_MX_BIND").unwrap_or_else(|_| "127.0.0.1:2525".into());
    let submission_bind =
        std::env::var("NEXUS_EMAIL_SUBMISSION_BIND").unwrap_or_else(|_| "127.0.0.1:2587".into());

    let pool = PgPoolOptions::new().max_connections(8).connect(&database_url).await?;
    let store = MailStore::new(pool.clone());

    let mut router = Router::new().with_local_domain(&domain);
    for peer in std::env::var("NEXUS_EMAIL_PEER_DOMAINS").unwrap_or_default().split(',') {
        let peer = peer.trim();
        if !peer.is_empty() {
            router = router.with_peer_domain(peer);
        }
    }

    let deliverer = Arc::new(Deliverer::new(store.clone(), Queue::new(pool), router));
    let dns = Arc::new(SystemDns::new());
    let policy = Arc::new(MailboxPolicy::new(store.clone()));

    let sink = Arc::new(AuthenticatingSink {
        store: store.clone(),
        deliverer: Arc::clone(&deliverer),
        dns: Arc::clone(&dns),
        receiving_host: hostname.clone(),
        mode,
    });

    let mx = Arc::new(SmtpServer {
        hostname: hostname.clone(),
        role: Role::Mx,
        limits: Limits::default(),
        policy: Arc::clone(&policy),
        sink: Arc::clone(&sink),
    });
    let submission = Arc::new(SmtpServer {
        hostname: hostname.clone(),
        role: Role::Submission,
        limits: Limits::default(),
        policy,
        sink,
    });

    // IMAP, so ordinary mail clients can use this mailbox. Credentials are
    // checked against Auth rather than against anything stored here.
    let imap_bind = std::env::var("NEXUS_EMAIL_IMAP_BIND").unwrap_or_else(|_| "127.0.0.1:2143".into());
    let auth_url = std::env::var("NEXUS_AUTH_INTERNAL_URL")
        .unwrap_or_else(|_| "http://127.0.0.1:4310".into());
    let imap = Arc::new(ImapServer {
        hostname: hostname.clone(),
        store: store.clone(),
        auth: Arc::new(AuthService::new(auth_url)),
    });
    let imap_listener = TcpListener::bind(&imap_bind).await?;

    let mx_listener = TcpListener::bind(&mx_bind).await?;
    let sub_listener = TcpListener::bind(&submission_bind).await?;
    tracing::info!(%mx_bind, %submission_bind, %imap_bind, %hostname, ?mode, "nexus-mailsmtpd listening");

    if mode == PolicyMode::Observe {
        tracing::info!(
            "policy mode is observe: SPF/DKIM/DMARC are evaluated and recorded, \
             but nothing is refused on their account. Set NEXUS_EMAIL_POLICY=enforce \
             once the Authentication-Results headers look right."
        );
    }

    // Both listeners share the process; if either dies the daemon should stop
    // rather than silently serve half its job.
    tokio::select! {
        r = mx.serve(mx_listener) => r?,
        r = submission.serve(sub_listener) => r?,
        r = imap.serve(imap_listener) => r?,
    }
    Ok(())
}
