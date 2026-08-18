//! One pass of the delivery worker against the real queue.
//!
//! Exists to answer a question no unit test can: what actually happens when
//! this node tries to deliver to the outside world from a connection whose
//! port 25 is filtered.

use nexus_maildelivery::Queue;
use nexus_mailout::{DeliveryWorker, WorkerConfig};
use nexus_mailstore::MailStore;
use sqlx::postgres::PgPoolOptions;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    tracing_subscriber::fmt().with_max_level(tracing::Level::INFO).init();

    let url = std::env::var("NEXUS_EMAIL_DATABASE_URL")?;
    let pool = PgPoolOptions::new().max_connections(2).connect(&url).await?;

    let worker = DeliveryWorker::new(
        MailStore::new(pool.clone()),
        Queue::new(pool),
        WorkerConfig {
            ehlo_name: std::env::var("NEXUS_EMAIL_EHLO").unwrap_or_else(|_| "mail.tnhc.dev".into()),
            ..Default::default()
        },
    );

    let attempted = worker.tick().await.map_err(|e| e.to_string())?;
    println!("attempted {attempted} deliveries");
    Ok(())
}
