use axum::{extract::State, http::StatusCode, response::IntoResponse, Json};
use database_core::{
    DatabaseConfig, DatabaseEngine, DatabaseError, DatabaseInfo, DatabaseStatus,
    HealthResponse, ProvisionDatabaseRequest, QueryRequest, QueryResponse,
};
use uuid::Uuid;

use crate::state::AppState;

/// GET /
pub async fn root(
    State(state): State<AppState>,
    headers: axum::http::HeaderMap,
) -> axum::response::Response {
    let accept = headers
        .get(axum::http::header::ACCEPT)
        .and_then(|v| v.to_str().ok())
        .unwrap_or("");

    if accept.contains("text/html") {
        let uptime = state.start_time.elapsed().as_secs();
        let db_count = state.database_count().await;
        let html = format!(
            r#"<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>Nexus Database Engine Studio</title>
  <style>
    :root {{ --bg: #090a0f; --card: #12151f; --line: #1e2436; --text: #e2e8f0; --muted: #798399; --accent: #f59e0b; }}
    body {{ margin: 0; background: var(--bg); color: var(--text); font-family: ui-sans-serif, system-ui, -apple-system, sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; padding: 20px; }}
    .card {{ background: var(--card); border: 1px solid var(--line); border-radius: 16px; padding: 36px; max-width: 580px; width: 100%; box-shadow: 0 20px 40px rgba(0,0,0,0.4); }}
    .badge {{ display: inline-flex; align-items: center; gap: 8px; font-family: ui-monospace, monospace; font-size: 11px; text-transform: uppercase; letter-spacing: 0.1em; color: var(--accent); background: rgba(245,158,11,0.1); border: 1px solid rgba(245,158,11,0.3); border-radius: 999px; padding: 4px 12px; margin-bottom: 20px; }}
    .dot {{ width: 8px; height: 8px; border-radius: 50%; background: var(--accent); }}
    h1 {{ margin: 0 0 10px; font-size: 24px; font-weight: 700; color: #fff; }}
    p {{ margin: 0 0 24px; color: var(--muted); font-size: 14px; line-height: 1.6; }}
    .meta {{ display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 24px; font-size: 13px; font-family: ui-monospace, monospace; }}
    .meta-box {{ background: rgba(255,255,255,0.02); border: 1px solid var(--line); border-radius: 8px; padding: 12px; }}
    .meta-box .k {{ color: var(--muted); font-size: 11px; margin-bottom: 4px; text-transform: uppercase; }}
    .meta-box .v {{ color: #fff; font-weight: 600; }}
    .btn {{ display: inline-flex; align-items: center; justify-content: center; gap: 8px; width: 100%; background: #d97706; color: #fff; text-decoration: none; padding: 12px 20px; border-radius: 8px; font-weight: 600; font-size: 14px; transition: background 0.2s; }}
    .btn:hover {{ background: #b45309; }}
  </style>
</head>
<body>
  <div class="card">
    <div class="badge"><span class="dot"></span> Engine Online</div>
    <h1>Nexus Database Engine</h1>
    <p>Consistency-tiered sovereign database engine, multi-tenant persistence manager, and high-performance Postgres wire protocol provider for Nexus Systems.</p>
    <div class="meta">
      <div class="meta-box"><div class="k">Version</div><div class="v">{}</div></div>
      <div class="meta-box"><div class="k">Managed DBs</div><div class="v">{}</div></div>
      <div class="meta-box"><div class="k">Uptime</div><div class="v">{}s</div></div>
      <div class="meta-box"><div class="k">Wire Protocol</div><div class="v">Postgres 5432</div></div>
    </div>
    <a href="https://app.tnhc.dev/" class="btn">Return to Platform Dashboard →</a>
  </div>
</body>
</html>"#,
            env!("CARGO_PKG_VERSION"),
            db_count,
            uptime
        );
        axum::response::Html(html).into_response()
    } else {
        Json(HealthResponse {
            status: "ok",
            version: env!("CARGO_PKG_VERSION"),
            managed_databases: state.database_count().await,
            uptime_seconds: state.start_time.elapsed().as_secs_f64(),
            persistence: state.persistence.is_active(),
            cloud_system_id: state.cloud_system_id.clone(),
        })
        .into_response()
    }
}

/// GET /health
pub async fn health(State(state): State<AppState>) -> Json<HealthResponse> {
    Json(HealthResponse {
        status: "ok",
        version: env!("CARGO_PKG_VERSION"),
        managed_databases: state.database_count().await,
        uptime_seconds: state.start_time.elapsed().as_secs_f64(),
        persistence: state.persistence.is_active(),
        cloud_system_id: state.cloud_system_id.clone(),
    })
}

/// GET /databases
pub async fn list_databases(
    State(state): State<AppState>,
) -> Result<Json<Vec<DatabaseInfo>>, DatabaseError> {
    let dbs = state.databases.read().await;
    let infos: Vec<DatabaseInfo> = dbs
        .iter()
        .map(|(id, config)| DatabaseInfo {
            id: *id,
            name: config.name.clone(),
            engine: config.engine.clone(),
            status: DatabaseStatus::Healthy,
            pool_size: config.max_connections,
            created_at: config.created_at,
            tags: config.tags.clone(),
        })
        .collect();
    Ok(Json(infos))
}

/// POST /databases — provision a new database.
pub async fn provision_database(
    State(state): State<AppState>,
    Json(req): Json<ProvisionDatabaseRequest>,
) -> Result<(StatusCode, Json<DatabaseInfo>), DatabaseError> {
    if req.name.trim().is_empty() {
        return Err(DatabaseError::Config("name is required".into()));
    }

    if state.name_exists(&req.name).await {
        return Err(DatabaseError::Config(format!(
            "database '{}' already exists",
            req.name
        )));
    }

    let engine = match req.engine.to_lowercase().as_str() {
        "postgres" | "postgresql" => DatabaseEngine::Postgres,
        "sqlite" => DatabaseEngine::Sqlite,
        other => {
            return Err(DatabaseError::Config(format!(
                "unsupported engine: {}",
                other
            )))
        }
    };

    let now = chrono::Utc::now();
    let id = Uuid::new_v4();
    let config = DatabaseConfig {
        id,
        name: req.name.clone(),
        engine: engine.scheme().to_string(),
        connection_string: req.connection_string.clone().unwrap_or_else(|| match &engine {
            DatabaseEngine::Sqlite => "sqlite::memory:".into(),
            DatabaseEngine::Postgres => format!(
                "postgres://{}:{}@localhost:5432/{}",
                req.name, "changeme", req.name
            ),
        }),
        max_connections: req.max_connections.unwrap_or(10),
        auto_migrate: false,
        tags: req.tags.clone(),
        created_at: now,
        updated_at: now,
    };

    {
        let mut dbs = state.databases.write().await;
        dbs.insert(id, config.clone());
    }

    // Persist to control-plane (fire-and-forget — failure is logged, not fatal)
    state.persistence.insert(&config).await;

    let info = DatabaseInfo {
        id,
        name: req.name,
        engine: engine.scheme().to_string(),
        status: DatabaseStatus::Provisioning,
        pool_size: config.max_connections,
        created_at: now,
        tags: req.tags,
    };

    Ok((StatusCode::CREATED, Json(info)))
}

/// GET /databases/:id
pub async fn get_database(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
) -> Result<Json<DatabaseInfo>, DatabaseError> {
    let dbs = state.databases.read().await;
    let config = dbs
        .get(&id)
        .ok_or_else(|| DatabaseError::DatabaseNotFound(id.to_string()))?;

    Ok(Json(DatabaseInfo {
        id,
        name: config.name.clone(),
        engine: config.engine.clone(),
        status: DatabaseStatus::Healthy,
        pool_size: config.max_connections,
        created_at: config.created_at,
        tags: config.tags.clone(),
    }))
}

/// POST /databases/:id/query — execute a SQL query against a managed database.
pub async fn query_database(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
    Json(req): Json<QueryRequest>,
) -> Result<Json<QueryResponse>, DatabaseError> {
    let config = {
        let dbs = state.databases.read().await;
        dbs.get(&id)
            .cloned()
            .ok_or_else(|| DatabaseError::DatabaseNotFound(id.to_string()))?
    };

    let pool = state.get_or_create_pool(&config).await?;

    if req.readonly {
        pool.execute_query(&req.sql).await.map(Json)
    } else {
        let affected = pool.execute_write(&req.sql).await?;
        Ok(Json(QueryResponse {
            columns: vec!["rows_affected".into()],
            rows: vec![vec![serde_json::Value::Number(affected.into())]],
            row_count: 1,
            elapsed_ms: 0.0,
        }))
    }
}

/// DELETE /databases/:id
pub async fn delete_database(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
) -> Result<StatusCode, DatabaseError> {
    let mut dbs = state.databases.write().await;
    if dbs.remove(&id).is_none() {
        return Err(DatabaseError::DatabaseNotFound(id.to_string()));
    }

    // Remove pool and control-plane record (fire-and-forget)
    state.remove_pool(id).await;
    state.persistence.delete(id).await;

    Ok(StatusCode::NO_CONTENT)
}
