use axum::{extract::State, http::StatusCode, response::IntoResponse, Json};
use database_core::{
    DatabaseConfig, DatabaseEngine, DatabaseError, DatabaseInfo, DatabaseStatus,
    HealthResponse, ProvisionDatabaseRequest, QueryRequest, QueryResponse,
};
use uuid::Uuid;

use crate::state::AppState;

const STUDIO_HTML: &str = include_str!("../static/index.html");

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
        axum::response::Html(STUDIO_HTML).into_response()
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

/// GET /studio
pub async fn studio() -> axum::response::Html<&'static str> {
    axum::response::Html(STUDIO_HTML)
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

#[derive(serde::Serialize, serde::Deserialize)]
pub struct SnapshotTable {
    pub name: String,
    pub columns: Vec<String>,
    pub rows: Vec<Vec<serde_json::Value>>,
}

#[derive(serde::Serialize, serde::Deserialize)]
pub struct DatabaseSnapshot {
    pub database_id: Uuid,
    pub name: String,
    pub engine: String,
    pub created_at: chrono::DateTime<chrono::Utc>,
    pub tables: Vec<SnapshotTable>,
}

/// GET /databases/:id/snapshot — dump tables and data as a downloadable snapshot
pub async fn export_snapshot(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
) -> Result<axum::response::Response, DatabaseError> {
    let config = {
        let dbs = state.databases.read().await;
        dbs.get(&id)
            .cloned()
            .ok_or_else(|| DatabaseError::DatabaseNotFound(id.to_string()))?
    };

    let pool = state.get_or_create_pool(&config).await?;

    let tables_sql = if config.engine.contains("sqlite") {
        "SELECT name AS table_name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%'"
    } else {
        "SELECT table_name FROM information_schema.tables WHERE table_schema = 'public'"
    };

    let mut tables = Vec::new();
    if let Ok(res) = pool.execute_query(tables_sql).await {
        for row in res.rows {
            if let Some(name_val) = row.get(0) {
                if let Some(table_name) = name_val.as_str() {
                    let dump_sql = format!("SELECT * FROM \"{}\" LIMIT 1000", table_name);
                    if let Ok(dump_res) = pool.execute_query(&dump_sql).await {
                        tables.push(SnapshotTable {
                            name: table_name.to_string(),
                            columns: dump_res.columns,
                            rows: dump_res.rows,
                        });
                    }
                }
            }
        }
    }

    let snapshot = DatabaseSnapshot {
        database_id: id,
        name: config.name.clone(),
        engine: config.engine.clone(),
        created_at: chrono::Utc::now(),
        tables,
    };

    let json_bytes = serde_json::to_vec_pretty(&snapshot)
        .map_err(|e| DatabaseError::Query(format!("Failed to serialize snapshot: {e}")))?;

    let filename = format!("snapshot-{}-{}.json", config.name, chrono::Utc::now().format("%Y%m%d%H%M%S"));

    let response = axum::response::Response::builder()
        .header(axum::http::header::CONTENT_TYPE, "application/json")
        .header(
            axum::http::header::CONTENT_DISPOSITION,
            format!("attachment; filename=\"{}\"", filename),
        )
        .body(axum::body::Body::from(json_bytes))
        .map_err(|e| DatabaseError::Query(format!("Response build failed: {e}")))?;

    Ok(response)
}

/// POST /databases/:id/snapshot/restore — restore tables and data from a snapshot
pub async fn restore_snapshot(
    State(state): State<AppState>,
    axum::extract::Path(id): axum::extract::Path<Uuid>,
    Json(snapshot): Json<DatabaseSnapshot>,
) -> Result<Json<serde_json::Value>, DatabaseError> {
    let config = {
        let dbs = state.databases.read().await;
        dbs.get(&id)
            .cloned()
            .ok_or_else(|| DatabaseError::DatabaseNotFound(id.to_string()))?
    };

    let pool = state.get_or_create_pool(&config).await?;
    let mut restored_tables = 0;
    let mut restored_rows = 0;

    for table in snapshot.tables {
        if table.columns.is_empty() {
            continue;
        }
        let col_defs = table
            .columns
            .iter()
            .map(|c| format!("\"{}\" TEXT", c))
            .collect::<Vec<_>>()
            .join(", ");
        let create_sql = format!("CREATE TABLE IF NOT EXISTS \"{}\" ({})", table.name, col_defs);
        let _ = pool.execute_write(&create_sql).await;

        for row in table.rows {
            if row.len() == table.columns.len() {
                let cols = table
                    .columns
                    .iter()
                    .map(|c| format!("\"{}\"", c))
                    .collect::<Vec<_>>()
                    .join(", ");
                let vals = row
                    .iter()
                    .map(|v| match v {
                        serde_json::Value::Null => "NULL".to_string(),
                        serde_json::Value::String(s) => format!("'{}'", s.replace('\'', "''")),
                        serde_json::Value::Number(n) => n.to_string(),
                        serde_json::Value::Bool(b) => {
                            if *b {
                                "TRUE".to_string()
                            } else {
                                "FALSE".to_string()
                            }
                        }
                        other => format!("'{}'", other.to_string().replace('\'', "''")),
                    })
                    .collect::<Vec<_>>()
                    .join(", ");

                let insert_sql = format!("INSERT INTO \"{}\" ({}) VALUES ({})", table.name, cols, vals);
                if pool.execute_write(&insert_sql).await.is_ok() {
                    restored_rows += 1;
                }
            }
        }
        restored_tables += 1;
    }

    Ok(Json(serde_json::json!({
        "status": "ok",
        "restored_tables": restored_tables,
        "restored_rows": restored_rows,
    })))
}
