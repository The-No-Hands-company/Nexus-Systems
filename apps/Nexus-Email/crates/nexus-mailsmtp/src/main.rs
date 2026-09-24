//! The Nexus SMTP daemon.
//!
//! Two listeners with different rules: an MX port for anonymous strangers,
//! which may only deliver to mailboxes we host, and a submission port for our
//! own users, which requires authentication and may send anywhere.

use std::sync::Arc;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use nexus_maildelivery::{Deliverer, Queue, Router};
use nexus_mailauth::{load_private_key_pem, Canon, DkimSigner, SystemDns};
use nexus_mailfed::client::HttpTransport;
use nexus_mailfed::ingest::{self, IngestState};
use nexus_mailfed::{NodeKey, PeerDirectory};
use nexus_mailout::{DeliveryWorker, Egress, WorkerConfig};
use nexus_mailsmtp::inbound::{AuthenticatingSink, PolicyMode};
use nexus_mailsmtp::policy::MailboxPolicy;
use nexus_mailimap::ImapServer;
use nexus_mailsmtp::authenticator::AuthService;
use nexus_mailsmtp::{Limits, Role, SmtpServer};
use nexus_mailstore::MailStore;
use sqlx::postgres::PgPoolOptions;
use tokio::net::TcpListener;

/// An environment variable, treating empty as unset: deploy.sh passes every
/// variable explicitly, so an unset optional one arrives as "".
fn setting(name: &str) -> Option<String> {
    std::env::var(name).ok().map(|v| v.trim().to_string()).filter(|v| !v.is_empty())
}

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

    // Federation. Parsed before anything binds, so a bad setting stops the
    // daemon at start rather than surfacing as mail that never leaves.
    let egress = Egress::parse(&setting("NEXUS_EMAIL_EGRESS").unwrap_or_default())?;
    let federation_bind = setting("NEXUS_EMAIL_FEDERATION_BIND").unwrap_or_else(|| "127.0.0.1:2580".into());
    // The host peers pin for us, which they sign for. Not the Host header:
    // the ecosystem proxy rewrites that.
    let federation_host = setting("NEXUS_EMAIL_FEDERATION_HOST").unwrap_or_else(|| hostname.clone());
    let home = std::env::var("HOME").unwrap_or_else(|_| ".".into());
    // Under $HOME, like the DKIM key: the project volume is NTFS, where file
    // permissions cannot protect a private key.
    let key_path =
        setting("NEXUS_EMAIL_NODE_KEY_PATH").unwrap_or_else(|| format!("{home}/.config/nexus-email/node.key"));
    let node_key = Arc::new(NodeKey::load_or_create(std::path::Path::new(&key_path))?);
    let dkim = match (
        setting("NEXUS_EMAIL_DKIM_KEY_PATH"),
        setting("NEXUS_EMAIL_DKIM_SELECTOR"),
    ) {
        (Some(path), Some(selector)) => Some(Arc::new(DkimSigner {
            domain: domain.clone(),
            selector,
            key: load_private_key_pem(&std::fs::read_to_string(&path)?)?,
            header_canon: Canon::Relaxed,
            body_canon: Canon::Relaxed,
        })),
        (None, None) => None,
        _ => return Err("set both NEXUS_EMAIL_DKIM_KEY_PATH and NEXUS_EMAIL_DKIM_SELECTOR, or neither".into()),
    };

    let pool = PgPoolOptions::new().max_connections(8).connect(&database_url).await?;
    let store = MailStore::new(pool.clone());
    let peers = PeerDirectory::new(pool.clone());

    // Pinned peers are federated destinations; the env list remains for
    // domains routed to a peer without one being pinned yet. A peer pinned
    // after start is picked up on restart.
    let mut router = Router::new().with_local_domain(&domain);
    let pinned: Vec<String> = peers.list().await?.into_iter().map(|p| p.domain).collect();
    for peer in std::env::var("NEXUS_EMAIL_PEER_DOMAINS")
        .unwrap_or_default()
        .split(',')
        .map(str::trim)
        .filter(|p| !p.is_empty())
        .map(str::to_string)
        .chain(pinned)
    {
        router = router.with_peer_domain(&peer);
    }

    let queue = Queue::new(pool);
    let deliverer = Arc::new(Deliverer::new(store.clone(), queue.clone(), router));
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

    let clock: Arc<dyn Fn() -> i64 + Send + Sync> =
        Arc::new(|| SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_secs() as i64).unwrap_or(0));
    let federation = ingest::router(Arc::new(IngestState {
        local_domain: domain.clone(),
        public_host: federation_host.clone(),
        key: Arc::clone(&node_key),
        peers: peers.clone(),
        deliverer: Arc::clone(&deliverer),
        clock: Arc::clone(&clock),
    }));
    let federation_listener = TcpListener::bind(&federation_bind).await?;

    let transport = Arc::new(HttpTransport::new(domain.clone(), Arc::clone(&node_key), peers.clone(), clock));
    let mut worker = DeliveryWorker::new(
        store.clone(),
        queue,
        WorkerConfig { ehlo_name: hostname.clone(), egress: egress.clone(), ..WorkerConfig::default() },
    )
    .with_transport(transport);
    if let Some(signer) = dkim.clone() {
        worker = worker.with_dkim(signer);
    }

    let mx_listener = TcpListener::bind(&mx_bind).await?;
    let sub_listener = TcpListener::bind(&submission_bind).await?;
    tracing::info!(%mx_bind, %submission_bind, %imap_bind, %hostname, ?mode, "nexus-mailsmtpd listening");
    tracing::info!(
        %federation_bind,
        %federation_host,
        ?egress,
        dkim = dkim.is_some(),
        public_key = %node_key.public_b64(),
        "federation listening; give peers this domain, URL and public key"
    );
    if egress == Egress::Direct {
        tracing::info!("egress is direct: outside mail needs an unfiltered port 25 from this host");
    }

    if mode == PolicyMode::Observe {
        tracing::info!(
            "policy mode is observe: SPF/DKIM/DMARC are evaluated and recorded, \
             but nothing is refused on their account. Set NEXUS_EMAIL_POLICY=enforce \
             once the Authentication-Results headers look right."
        );
    }

    // Both listeners share the process; if either dies the daemon should stop
    // rather than silently serve half its job.
    let prune = async {
        loop {
            // Twice the skew window: a nonce older than that belongs to a
            // request its timestamp alone already refuses.
            let window = Duration::from_secs(2 * nexus_mailfed::wire::MAX_SKEW_SECS as u64);
            if let Err(e) = peers.prune_nonces(window).await {
                tracing::warn!(error = %e, "nonce prune failed");
            }
            tokio::time::sleep(Duration::from_secs(300)).await;
        }
    };

    tokio::select! {
        r = mx.serve(mx_listener) => r?,
        r = submission.serve(sub_listener) => r?,
        r = imap.serve(imap_listener) => r?,
        r = axum::serve(federation_listener, federation) => r?,
        _ = worker.run() => {},
        _ = prune => {},
    }
    Ok(())
}
