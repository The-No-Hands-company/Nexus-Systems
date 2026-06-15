use axum::{routing::{delete, get, post}, Extension, Router, middleware};
use database_core::ServerConfig;
use tracing::info;

mod auth;
mod cloud;
mod persistence;
mod routes;
mod state;

use cloud::CloudClient;
use persistence::Persistence;
use state::AppState;

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    // Initialise tracing
    tracing_subscriber::fmt()
        .with_env_filter(
            tracing_subscriber::EnvFilter::try_from_default_env()
                .unwrap_or_else(|_| "nexus_database=info,tower_http=info".into()),
        )
        .init();

    // Load configuration
    let config = database_core::ServerConfig::from_env().unwrap_or_else(|e| {
        info!("No NEXUS_DB_ env vars found ({e}), using defaults");
        database_core::ServerConfig {
            host: "0.0.0.0".into(),
            port: 3000,
            control_plane_url: None,
            databases_config: None,
            log_level: "info".into(),
        }
    });

    // Initialise auth token set
    let token_set = auth::TokenSet::from_env();

    let persistence = Persistence::new(config.control_plane_url.as_deref()).await;
    let mut state = AppState::new(persistence).await;

    // ── Cloud registration ─────────────────────────────────────────
    let cloud_config = cloud::CloudConfig::from_env();
    let cloud = CloudClient::register(cloud_config, config.listen_addr()).await;

    // Record the system ID and start heartbeat
    if let Some(ref c) = cloud {
        state.cloud_system_id = c.system_id().map(String::from);
        let state_clone = state.clone();
        let start = state.start_time;
        c.start_heartbeat(move || {
            let count = state_clone.databases.try_read().map(|d| d.len()).unwrap_or(0);
            let uptime = start.elapsed().as_secs_f64();
            (count, uptime)
        });
    }

    // ── Routes ─────────────────────────────────────────────────────
    let read_routes = Router::new()
        .route("/databases", get(routes::list_databases))
        .route("/databases/:id", get(routes::get_database))
        .route_layer(middleware::from_fn(auth::require_read));

    // Admin-level auth: POST/PUT/DELETE endpoints
    let write_routes = Router::new()
        .route("/databases", post(routes::provision_database))
        .route("/databases/:id", delete(routes::delete_database))
        .route("/databases/:id/query", post(routes::query_database))
        .route_layer(middleware::from_fn(auth::require_admin));

    let app = Router::new()
        .route("/health", get(routes::health))
        .merge(read_routes)
        .merge(write_routes)
        .layer(Extension(token_set))
        .with_state(state);

    let addr = config.listen_addr();
    info!("Nexus Database starting on http://{addr}");

    // ── PostgreSQL wire protocol listener (port 5432) ──────────
    tokio::spawn(async {
        info!("PG wire protocol starting on 0.0.0.0:5432");
        if let Err(e) = database_engine::pgwire::listen("0.0.0.0:5432").await {
            tracing::error!("PG wire protocol error: {}", e);
        }
    });

    let listener = tokio::net::TcpListener::bind(&addr).await?;

    // Graceful shutdown: deregister from cloud on SIGINT/SIGTERM
    let shutdown_signal = async {
        tokio::signal::ctrl_c()
            .await
            .expect("failed to install Ctrl+C handler");
        info!("Shutdown signal received, cleaning up...");
    };

    axum::serve(listener, app)
        .with_graceful_shutdown(shutdown_signal)
        .await?;

    // Cleanup
    if let Some(c) = cloud {
        c.shutdown().await;
    }

    info!("Nexus Database stopped");
    Ok(())
}
