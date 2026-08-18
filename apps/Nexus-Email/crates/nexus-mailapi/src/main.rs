use std::sync::Arc;

use nexus_maildelivery::{Deliverer, Queue, Router as MailRouter};
use nexus_mailapi::{router, AppState};
use nexus_mailstore::MailStore;
use sqlx::postgres::PgPoolOptions;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "nexus_mailapi=info,tower_http=warn".into()),
        )
        .init();

    let database_url = std::env::var("NEXUS_EMAIL_DATABASE_URL")
        .map_err(|_| "NEXUS_EMAIL_DATABASE_URL must be set")?;
    let domain = std::env::var("NEXUS_EMAIL_DOMAIN").unwrap_or_else(|_| "tnhc.dev".into());

    // Loopback by default, and that is a security control rather than a
    // convenience: this service trusts the X-Nexus-Subject header set by the
    // Dashboard, so anything that can reach the port can claim to be anyone.
    let bind = std::env::var("NEXUS_EMAIL_BIND").unwrap_or_else(|_| "127.0.0.1:3140".into());

    let pool = PgPoolOptions::new().max_connections(8).connect(&database_url).await?;
    let store = MailStore::new(pool.clone());
    let queue = Queue::new(pool);

    let mut mail_router = MailRouter::new().with_local_domain(&domain);
    for peer in std::env::var("NEXUS_EMAIL_PEER_DOMAINS").unwrap_or_default().split(',') {
        let peer = peer.trim();
        if !peer.is_empty() {
            mail_router = mail_router.with_peer_domain(peer);
        }
    }

    let state = Arc::new(AppState {
        store: store.clone(),
        deliverer: Deliverer::new(store, queue, mail_router),
        primary_domain: domain.clone(),
    });

    let listener = tokio::net::TcpListener::bind(&bind).await?;
    tracing::info!(%bind, %domain, "nexus-mailapi listening");
    axum::serve(listener, router(state)).await?;
    Ok(())
}
