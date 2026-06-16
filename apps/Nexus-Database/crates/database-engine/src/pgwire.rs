//! PostgreSQL Wire Protocol v3.0 implementation.
//!
//! Enables standard PostgreSQL tools (psql, DBeaver, Prisma, etc.)
//! to connect to Nexus-Database and treat it as a PostgreSQL-compatible server.
//!
//! Protocol flow:
//!   Startup → Authentication → ReadyForQuery → (Query → Response) × N → Terminate

use std::collections::HashMap;
use std::sync::Arc;
use tokio::io::{AsyncReadExt, AsyncWriteExt};
use tokio::net::{TcpListener, TcpStream};

use crate::catalog::DeltaMainRouter;

/// Default PostgreSQL port.
pub const PG_PORT: u16 = 5432;

// ── Wire protocol constants ────────────────────────────────────

const PROTOCOL_VERSION: i32 = 196608; // 3.0
const CANCEL_REQUEST: i32 = 80877102;
const SSL_REQUEST: i32 = 80877103;

// Message type bytes (frontend → backend)
const M_QUERY: u8 = b'Q';
const M_PARSE: u8 = b'P';
const M_BIND: u8 = b'B';
const M_EXECUTE: u8 = b'E';
const M_DESCRIBE: u8 = b'D';
const M_SYNC: u8 = b'S';
const M_TERMINATE: u8 = b'X';

// Message type bytes (backend → frontend)
const R_AUTH: u8 = b'R';
const R_PARAM_STATUS: u8 = b'S';
const R_BACKEND_KEY: u8 = b'K';
const R_READY: u8 = b'Z';
const R_ROW_DESC: u8 = b'T';
const R_DATA_ROW: u8 = b'D';
const R_CMD_COMPLETE: u8 = b'C';
const R_ERROR: u8 = b'E';
const R_NOTICE: u8 = b'N';
const R_EMPTY_QUERY: u8 = b'I';

// ── Public API ─────────────────────────────────────────────────

/// Start the PostgreSQL wire protocol listener with a shared persistent router.
pub async fn listen_with_router(addr: &str, router: Arc<DeltaMainRouter>) -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind(addr).await?;
    log::info!("Nexus-Database listening on {} (persistent mode)", addr);

    loop {
        let (stream, addr) = listener.accept().await?;
        log::info!("Connection from {}", addr);
        if let Err(e) = handle_connection_with_router(stream, &router).await {
            log::error!("Connection error: {}", e);
        }
    }
}

/// Start the PostgreSQL wire protocol listener (standalone, no shared state).
pub async fn listen(addr: &str) -> Result<(), Box<dyn std::error::Error>> {
    let listener = TcpListener::bind(addr).await?;
    log::info!("Nexus-Database listening on {} (PostgreSQL wire protocol)", addr);

    loop {
        let (stream, addr) = listener.accept().await?;
        log::info!("Connection from {}", addr);
        tokio::spawn(async move {
            if let Err(e) = handle_connection(stream).await {
                log::error!("Connection error: {}", e);
            }
        });
    }
}

// ── Connection handler with router ──────────────────────────────

async fn handle_connection_with_router(mut stream: TcpStream, router: &DeltaMainRouter) -> Result<(), Box<dyn std::error::Error>> {
    // Startup handshake (same as standalone)
    let mut buf = vec![0u8; 4096];
    let n = stream.read(&mut buf).await?;
    if n < 8 { return Err("Short startup".into()); }
    let version = i32::from_be_bytes([buf[4], buf[5], buf[6], buf[7]]);

    if version == SSL_REQUEST {
        stream.write_all(b"N").await?;
        let n = stream.read(&mut buf).await?;
        let _ = i32::from_be_bytes([buf[4], buf[5], buf[6], buf[7]]);
    }

    // Authentication OK
    stream.write_all(&build_auth_ok()).await?;
    for (k, v) in &[
        ("server_version", "Nexus-Database/0.1.0"),
        ("server_encoding", "UTF8"),
        ("client_encoding", "UTF8"),
    ] { stream.write_all(&build_parameter_status(k, v)).await?; }
    stream.write_all(&build_backend_key(42, 0)).await?;
    stream.write_all(&build_ready_for_query()).await?;

    // Prepared statement cache
    let mut prepared: HashMap<String, String> = HashMap::new(); // name → SQL

    // Query loop with router
    let mut buf = vec![0u8; 16384];
    loop {
        match stream.read(&mut buf).await {
            Ok(0) => break,
            Ok(n) => {
                match buf[0] {
                    M_QUERY => {
                        let query = String::from_utf8_lossy(&buf[5..n - 1]).to_string();
                        handle_query_with_router(&mut stream, &query, router).await?;
                    }
                    M_PARSE => {
                        // Parse message: [name\0][query\0][param_types:u16]
                        let body = &buf[5..n];
                        let name_end = body.iter().position(|&b| b == 0).unwrap_or(0);
                        let name = String::from_utf8_lossy(&body[..name_end]).to_string();
                        let query_start = name_end + 1;
                        let query_end = body[query_start..].iter().position(|&b| b == 0).unwrap_or(0);
                        let sql = String::from_utf8_lossy(&body[query_start..query_start + query_end]).to_string();
                        prepared.insert(name, sql);
                        // ParseComplete ('1')
                        stream.write_all(b"1\0\0\0\x04").await?;
                    }
                    M_BIND => {
                        // BindComplete ('2')
                        stream.write_all(b"2\0\0\0\x04").await?;
                    }
                    M_DESCRIBE => {
                        // Return NoData for parameters, RowDescription for result
                        // For now: NoData ('n') for statement, RowDescription via query
                        let body = &buf[5..n];
                        if body.len() > 0 && body[0] == b'S' {
                            // Describe statement → ParameterDescription or NoData
                            stream.write_all(b"n\0\0\0\x04").await?; // NoData
                        } else {
                            stream.write_all(b"n\0\0\0\x04").await?; // NoData
                        }
                    }
                    M_EXECUTE => {
                        // Execute the unnamed portal — run the parsed query
                        if let Some(sql) = prepared.get("") {
                            handle_query_with_router(&mut stream, sql, router).await?;
                        } else {
                            stream.write_all(&build_cmd_complete("EXECUTE 0")).await?;
                        }
                    }
                    M_SYNC => {
                        stream.write_all(&build_ready_for_query()).await?;
                    }
                    M_TERMINATE => break,
                    _ => { stream.write_all(&build_ready_for_query()).await?; }
                }
            }
            Err(_) => break,
        }
    }
    Ok(())
}

// ── Query handler with router ────────────────────────────────────

async fn handle_query_with_router(stream: &mut TcpStream, query: &str, router: &DeltaMainRouter) -> Result<(), Box<dyn std::error::Error>> {
    let upper = query.trim().to_uppercase();
    if upper.is_empty() {
        stream.write_all(&build_empty_query()).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // Handle transaction commands
    if upper == "BEGIN" || upper == "BEGIN TRANSACTION" || upper == "BEGIN WORK" || upper == "START TRANSACTION" {
        router.undo_stack.write().clear();
        stream.write_all(&build_cmd_complete("BEGIN")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }
    if upper == "COMMIT" || upper == "COMMIT TRANSACTION" || upper == "COMMIT WORK" {
        router.undo_stack.write().clear();
        stream.write_all(&build_cmd_complete("COMMIT")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }
    if upper == "ROLLBACK" || upper == "ROLLBACK TRANSACTION" || upper == "ROLLBACK WORK" {
        undo_all(router);
        stream.write_all(&build_cmd_complete("ROLLBACK")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // Handle EXPLAIN — show query plan without executing
    if upper.starts_with("EXPLAIN") {
        let inner = query.trim().strip_prefix("EXPLAIN").unwrap_or(query).trim();
        let plan = generate_explain_plan(inner, router);
        send_query_result(stream, &vec!["QUERY PLAN".into()], &plan).await?;
        stream.write_all(&build_cmd_complete("EXPLAIN")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // Handle CREATE VIEW / DROP VIEW / SELECT from views
    if upper.starts_with("CREATE VIEW") {
        let rest = query.trim().strip_prefix("CREATE VIEW").unwrap_or(query).trim();
        if let Some((name, view_sql)) = rest.split_once("AS") {
            let name = name.trim().trim_matches('"').to_string();
            let view_sql = view_sql.trim().trim_end_matches(';').to_string();
            router.views.write().insert(name.clone(), view_sql);
            let cols = vec!["result".into()];
            let rows = vec![vec![format!("CREATE VIEW {}", name)]];
            send_query_result(stream, &cols, &rows).await?;
            stream.write_all(&build_cmd_complete("CREATE VIEW")).await?;
        }
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }
    if upper.starts_with("DROP VIEW") {
        let name = query.trim().strip_prefix("DROP VIEW").unwrap_or(query).trim()
            .strip_prefix("IF EXISTS").unwrap_or(query).trim()
            .trim_matches('"').trim_end_matches(';').to_string();
        router.views.write().remove(&name);
        let cols = vec!["result".into()];
        let rows = vec![vec![format!("DROP VIEW {}", name)]];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("DROP VIEW")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // Check if SELECT targets a view
    if upper.starts_with("SELECT") {
        // Extract table name: SELECT ... FROM table_name
        if let Some(from_pos) = upper.find("FROM ") {
            let after_from = &query[from_pos + 5..].trim();
            let table_name = after_from.split_whitespace().next().unwrap_or("").trim_matches('"').trim_end_matches(';');
            if let Some(view_sql) = router.views.read().get(table_name) {
                let view_query = view_sql.clone();
                drop(router.views.read());
                // Re-execute the view's stored query recursively
                return Box::pin(handle_query_with_router(stream, &view_query, router)).await;
            }
        }
    }

    // Try SQL parser + executor with the shared router
    if let Ok(stmt) = crate::sql::parse_sql(query) {
        match crate::sql::execute(router, &stmt) {
            Ok(result) => {
                let (cols, rows) = result.to_pgwire_response();
                send_query_result(stream, &cols, &rows).await?;
                let tag = match result {
                    crate::sql::ExecuteResult::CreateTable(_) => "CREATE TABLE",
                    crate::sql::ExecuteResult::Insert(n) => &format!("INSERT 0 {}", n),
                    crate::sql::ExecuteResult::Select { rows, .. } => &format!("SELECT {}", rows.len()),
                    crate::sql::ExecuteResult::Update(n) => &format!("UPDATE {}", n),
                    crate::sql::ExecuteResult::Delete(n) => &format!("DELETE {}", n),
                    crate::sql::ExecuteResult::Truncate(_) => "TRUNCATE TABLE",
                    crate::sql::ExecuteResult::AlterTable(_) => "ALTER TABLE",
                    crate::sql::ExecuteResult::DropTable(_) => "DROP TABLE",
                    crate::sql::ExecuteResult::DropIndex(_) => "DROP INDEX",
                };
                stream.write_all(&build_cmd_complete(tag)).await?;
                stream.write_all(&build_ready_for_query()).await?;
                return Ok(());
            }
            Err(e) => {
                stream.write_all(&build_error("42P01", &format!("Error: {}", e))).await?;
                stream.write_all(&build_ready_for_query()).await?;
                return Ok(());
            }
        }
    }

    // Handle special queries
    if upper.contains("VERSION()") {
        let cols = vec!["version".to_string()];
        let rows = vec![vec!["Nexus-Database 0.1.0 (Rust, persistent)".to_string()]];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // List tables + pg_catalog: return real schema data from internal catalog
    if upper.starts_with("\\DT") || upper.contains("PG_CLASS") || upper.contains("PG_NAMESPACE") 
        || upper.contains("PG_ATTRIBUTE") || upper.contains("PG_TYPE") || upper.contains("PG_INDEX") {
        handle_catalog_query(stream, query, router).await?;
        return Ok(());
    }

    // Generic fallback
    let cols = vec!["?column?".to_string()];
    let rows = vec![vec![format!("Query: {}", &query[..query.len().min(50)])]];
    send_query_result(stream, &cols, &rows).await?;
    stream.write_all(&build_cmd_complete("SELECT 1")).await?;
    stream.write_all(&build_ready_for_query()).await?;
    Ok(())
}

// ── Connection handler (standalone, no shared state) ────────────

async fn handle_connection(mut stream: TcpStream) -> Result<(), Box<dyn std::error::Error>> {
    let mut buf = vec![0u8; 4096];

    // 1. Read startup message
    let n = stream.read(&mut buf).await?;
    if n < 8 { return Err("Short startup".into()); }

    let len = i32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]]);
    let version = i32::from_be_bytes([buf[4], buf[5], buf[6], buf[7]]);

    if version == SSL_REQUEST {
        // Reject SSL for now — clients will fallback to plaintext
        stream.write_all(b"N").await?;
        // Read again after SSL rejection
        let n = stream.read(&mut buf).await?;
        let _ = i32::from_be_bytes([buf[0], buf[1], buf[2], buf[3]]);
        let _ = i32::from_be_bytes([buf[4], buf[5], buf[6], buf[7]]);
    }

    // Parse startup parameters
    let mut params = HashMap::new();
    let mut pos = 8usize;
    while pos + 1 < len as usize {
        let end = buf[pos..].iter().position(|&b| b == 0).unwrap_or(0);
        if end == 0 { pos += 1; break; } // null terminator
        let key = String::from_utf8_lossy(&buf[pos..pos + end]).to_string();
        pos += end + 1;
        let end = buf[pos..].iter().position(|&b| b == 0).unwrap_or(0);
        let val = String::from_utf8_lossy(&buf[pos..pos + end]).to_string();
        pos += end + 1;
        params.insert(key, val);
    }

    let _user = params.get("user").cloned().unwrap_or_else(|| "nexus".into());
    let _database = params.get("database").cloned().unwrap_or_else(|| "nexus".into());
    log::info!("Startup: user={}, database={}", _user, _database);

    // 2. Authentication — Trust (no password required)
    stream.write_all(&build_auth_ok()).await?;

    // 3. Server parameters
    for (key, value) in &[
        ("server_version", "Nexus-Database/0.1.0"),
        ("server_encoding", "UTF8"),
        ("client_encoding", "UTF8"),
        ("DateStyle", "ISO, MDY"),
        ("integer_datetimes", "on"),
    ] {
        stream.write_all(&build_parameter_status(key, value)).await?;
    }

    // 4. Backend key data (for cancel requests)
    stream.write_all(&build_backend_key(42, 0)).await?;

    // 5. Ready for queries
    stream.write_all(&build_ready_for_query()).await?;

    // 6. Query loop
    let mut buf = vec![0u8; 16384];
    loop {
        match stream.read(&mut buf).await {
            Ok(0) => break, // connection closed
            Ok(n) => {
                let msg_type = buf[0];
                match msg_type {
                    M_QUERY => {
                        let query = String::from_utf8_lossy(&buf[5..n - 1]).to_string();
                        handle_query(&mut stream, &query).await?;
                    }
                    M_TERMINATE => break,
                    _ => {
                        // Unknown message — skip and send ready
                        stream.write_all(&build_ready_for_query()).await?;
                    }
                }
            }
            Err(_) => break,
        }
    }

    log::info!("Connection closed");
    Ok(())
}

// ── Query execution ────────────────────────────────────────────

async fn handle_query(stream: &mut TcpStream, query: &str) -> Result<(), Box<dyn std::error::Error>> {
    let upper = query.trim().to_uppercase();

    if upper.is_empty() {
        stream.write_all(&build_empty_query()).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // SELECT version()
    if upper.contains("VERSION()") {
        let cols = vec!["version".to_string()];
        let rows = vec![vec!["Nexus-Database 0.1.0 (Rust)".to_string()]];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // SELECT current_database()
    if upper.contains("CURRENT_DATABASE") {
        let cols = vec!["current_database".to_string()];
        let rows = vec![vec!["nexus".to_string()]];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // pg_catalog queries (tools like psql connect and query these)
    if upper.contains("PG_CLASS") || upper.contains("PG_NAMESPACE") || upper.contains("PG_ATTRIBUTE") {
        let empty: Vec<Vec<String>> = vec![];
        let cols = vec!["oid".to_string(), "relname".to_string(), "relnamespace".to_string(), "relkind".to_string(), "reltuples".to_string(), "relpages".to_string()];
        // Return minimal valid catalog — prevents tools from crashing
        if upper.contains("PG_CLASS") && !upper.contains("PG_NAMESPACE") {
            let rows = vec![
                vec!["1".to_string(), "nexus_table".to_string(), "1".to_string(), "r".to_string(), "0".to_string(), "0".to_string()],
            ];
            send_query_result(stream, &cols, &rows).await?;
        } else if upper.contains("PG_NAMESPACE") {
            let cols = vec!["oid".to_string(), "nspname".to_string(), "nspowner".to_string(), "nspacl".to_string()];
            let rows = vec![
                vec!["1".to_string(), "public".to_string(), "1".to_string(), "".to_string()],
                vec!["11".to_string(), "pg_catalog".to_string(), "1".to_string(), "".to_string()],
            ];
            send_query_result(stream, &cols, &rows).await?;
        } else {
            send_query_result(stream, &cols, &empty).await?;
        }
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // CREATE TABLE name (columns)
    if upper.starts_with("CREATE TABLE") {
        stream.write_all(&build_cmd_complete("CREATE TABLE")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // INSERT INTO name VALUES (...)
    if upper.starts_with("INSERT INTO") {
        stream.write_all(&build_cmd_complete("INSERT 0 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // SELECT * FROM name
    if upper.starts_with("SELECT") {
        // Return a placeholder result
        let cols = vec!["?column?".to_string()];
        let rows: Vec<Vec<String>> = vec![vec!["nexus-database".to_string()]];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // Generic — echo as notice
    stream.write_all(&build_notice(&format!("Query received: {}", query))).await?;
    stream.write_all(&build_cmd_complete("OK")).await?;
    stream.write_all(&build_ready_for_query()).await?;
    Ok(())
}

// ── Message builders ───────────────────────────────────────────

fn build_message(msg_type: u8, payload: &[u8]) -> Vec<u8> {
    let mut msg = Vec::with_capacity(5 + payload.len());
    msg.push(msg_type);
    let len = (4 + payload.len()) as i32;
    msg.extend_from_slice(&len.to_be_bytes());
    msg.extend_from_slice(payload);
    msg
}

fn build_auth_ok() -> Vec<u8> {
    // AuthenticationOk: int32(0)
    build_message(R_AUTH, &0i32.to_be_bytes())
}

fn build_parameter_status(key: &str, value: &str) -> Vec<u8> {
    let mut payload = Vec::new();
    payload.extend_from_slice(key.as_bytes());
    payload.push(0);
    payload.extend_from_slice(value.as_bytes());
    payload.push(0);
    build_message(R_PARAM_STATUS, &payload)
}

fn build_backend_key(pid: i32, secret: i32) -> Vec<u8> {
    let mut payload = Vec::new();
    payload.extend_from_slice(&pid.to_be_bytes());
    payload.extend_from_slice(&secret.to_be_bytes());
    build_message(R_BACKEND_KEY, &payload)
}

fn build_ready_for_query() -> Vec<u8> {
    // 'I' = idle, 'T' = in transaction, 'E' = error
    build_message(R_READY, b"I")
}

fn undo_all(router: &DeltaMainRouter) {
    let mut stack = router.undo_stack.write();
    while let Some(action) = stack.pop() {
        match action {
            crate::catalog::UndoAction::Insert { table, row_idx } => {
                let _ = router.columnar.write().delete_row(&table, row_idx);
            }
            crate::catalog::UndoAction::Update { table, row_idx, column, old_value } => {
                let _ = router.columnar.write().update_row(&table, &column, row_idx, old_value);
            }
            crate::catalog::UndoAction::Delete { table, row_idx, data } => {
                // Re-insert: append row at position (not quite right — we'd need re-insert)
                // For now, restore data at row_idx position
                let store = router.columnar.read();
                let meta = router.catalog().get_table(&table);
                if let Some(meta) = meta {
                    for (col_i, col_name) in meta.column_names.iter().enumerate() {
                        if col_i < data.len() {
                            let _ = router.columnar.write().update_row(&table, col_name, row_idx, data[col_i].clone());
                        }
                    }
                }
            }
            crate::catalog::UndoAction::Truncate { table, data } => {
                // Re-insert all rows
                let meta = router.catalog().get_table(&table);
                if let Some(meta) = meta {
                    for row_data in &data {
                        let _ = router.columnar.write().append_row(&table, row_data, &meta.column_types);
                    }
                }
            }
            crate::catalog::UndoAction::AlterAddColumn { table, column } => {
                router.columnar.write().drop_column(&table, &column);
                let _ = router.catalog().drop_column(&table, &column);
            }
            crate::catalog::UndoAction::AlterDropColumn { table, column, data } => {
                // Re-add column with old data
                let col_type = crate::catalog::ColumnType::Text; // approximate
                router.columnar.write().add_column(&table, &column, col_type.clone());
                let _ = router.catalog().add_column(&table, &column, col_type);
                for (row_idx, val) in data.iter().enumerate() {
                    let _ = router.columnar.write().update_row(&table, &column, row_idx, val.clone());
                }
            }
        }
    }
}

fn build_cmd_complete(tag: &str) -> Vec<u8> {
    let mut payload = tag.as_bytes().to_vec();
    payload.push(0);
    build_message(R_CMD_COMPLETE, &payload)
}

fn build_empty_query() -> Vec<u8> {
    build_message(R_EMPTY_QUERY, b"")
}

fn build_notice(msg: &str) -> Vec<u8> {
    let mut payload = Vec::new();
    // Severity
    payload.push(b'S'); payload.extend_from_slice(b"NOTICE"); payload.push(0);
    // Message
    payload.push(b'M'); payload.extend_from_slice(msg.as_bytes()); payload.push(0);
    payload.push(0); // terminator
    build_message(R_NOTICE, &payload)
}

fn build_error(code: &str, msg: &str) -> Vec<u8> {
    let mut payload = Vec::new();
    payload.push(b'S'); payload.extend_from_slice(b"ERROR"); payload.push(0);
    payload.push(b'C'); payload.extend_from_slice(code.as_bytes()); payload.push(0);
    payload.push(b'M'); payload.extend_from_slice(msg.as_bytes()); payload.push(0);
    payload.push(0);
    build_message(R_ERROR, &payload)
}

async fn send_query_result(
    stream: &mut TcpStream,
    columns: &[String],
    rows: &[Vec<String>],
) -> Result<(), Box<dyn std::error::Error>> {
    // RowDescription
    let mut payload = Vec::new();
    payload.extend_from_slice(&(columns.len() as i16).to_be_bytes());
    for col in columns {
        payload.extend_from_slice(col.as_bytes()); payload.push(0); // name
        payload.extend_from_slice(&0i32.to_be_bytes());  // table_oid
        payload.extend_from_slice(&0i16.to_be_bytes());  // column_attr
        payload.extend_from_slice(&25i32.to_be_bytes()); // type_oid (TEXT)
        payload.extend_from_slice(&(-1i16).to_be_bytes()); // type_size
        payload.extend_from_slice(&(-1i32).to_be_bytes()); // type_mod
        payload.extend_from_slice(&0i16.to_be_bytes());  // format_code (text)
    }
    stream.write_all(&build_message(R_ROW_DESC, &payload)).await?;

    // DataRows
    for row in rows {
        let mut payload = Vec::new();
        payload.extend_from_slice(&(row.len() as i16).to_be_bytes());
        for val in row {
            let bytes = val.as_bytes();
            payload.extend_from_slice(&(bytes.len() as i32).to_be_bytes());
            payload.extend_from_slice(bytes);
        }
        stream.write_all(&build_message(R_DATA_ROW, &payload)).await?;
    }

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_message_building() {
        let msg = build_auth_ok();
        assert_eq!(msg[0], R_AUTH);
        assert_eq!(msg.len(), 9); // 1 type + 4 len + 4 value
    }

    #[test]
    fn test_ready_for_query() {
        let msg = build_ready_for_query();
        assert_eq!(msg[0], R_READY);
        assert_eq!(msg[5], b'I'); // idle
    }
}

// ── EXPLAIN plan generator ──────────────────────────────────────

fn generate_explain_plan(query: &str, router: &DeltaMainRouter) -> Vec<Vec<String>> {
    let mut plan = Vec::new();
    let upper = query.trim().to_uppercase();

    if let Ok(stmt) = crate::sql::parse_sql(query) {
        match stmt {
            crate::sql::Statement::Select { table, ref columns, ref where_clause, ref order_by, limit, offset, ref join, .. } => {
                let meta = router.catalog().get_table(&table);
                let row_count = router.columnar.read().row_count(&table);

                if let Some(jc) = join {
                    plan.push(vec![format!("Nested Loop {:?} Join", jc.join_type)]);
                    plan.push(vec![format!("  Join Cond: ({}.{} {:?} {}.{})", jc.left_table, jc.condition.left, jc.condition.op, jc.right_table, jc.condition.right)]);
                    plan.push(vec![format!("  ->  Seq Scan on {}", table)]);
                    plan.push(vec![format!("        Rows: ~{}", row_count)]);
                    plan.push(vec![format!("  ->  Seq Scan on {}", jc.right_table)]);
                    plan.push(vec![format!("        Rows: ~{}", router.columnar.read().row_count(&jc.right_table))]);
                } else {
                    let scan_type = if let Some(wc) = where_clause {
                        if wc.is_compare() && router.indexes.find_best_index(&table, wc.column()).is_some() {
                            "Index Scan"
                        } else { "Seq Scan" }
                    } else { "Seq Scan" };
                    plan.push(vec![format!("{} on {}", scan_type, table)]);
                    plan.push(vec![format!("  Rows: ~{}", row_count)]);
                }

                if let Some(wc) = where_clause {
                    plan.push(vec![format!("  Filter: {:?}", wc.predicate)]);
                }

                if !order_by.is_empty() {
                    let keys: Vec<_> = order_by.iter().map(|(c, d)| format!("{} {:?}", c, d)).collect();
                    plan.push(vec![format!("  Sort Key: {}", keys.join(", "))]);
                }

                if let Some(n) = limit {
                    plan.push(vec![format!("  Limit: {}", n)]);
                }

                if let Some(n) = offset {
                    plan.push(vec![format!("  Offset: {}", n)]);
                }

                if let Some(meta) = meta {
                    plan.push(vec![format!("  Engine: {:?}", meta.engine)]);
                    plan.push(vec![format!("  Columns: {}", meta.column_names.join(", "))]);
                }
            }
            crate::sql::Statement::Insert { table, values, .. } => {
                plan.push(vec![format!("Insert on {}", table)]);
                plan.push(vec![format!("  Rows: {}", values.len())]);
                plan.push(vec![format!("  Engine: LSM (write-optimized Delta store)")]);
            }
            crate::sql::Statement::Update { table, sets, where_clause: _ } => {
                plan.push(vec![format!("Update on {}", table)]);
                plan.push(vec![format!("  SET: {}", sets.iter().map(|(c, v)| format!("{}= {}", c, format!("{v:?}"))).collect::<Vec<_>>().join(", "))]);
            }
            crate::sql::Statement::Delete { table, .. } => {
                plan.push(vec![format!("Delete on {}", table)]);
            }
            crate::sql::Statement::Truncate { table } => {
                plan.push(vec![format!("Truncate {}", table)]);
            }
            crate::sql::Statement::AlterTable { table, action } => {
                plan.push(vec![format!("Alter Table {}", table)]);
                plan.push(vec![format!("  Action: {:?}", action)]);
            }
            crate::sql::Statement::DropTable { table, .. } => {
                plan.push(vec![format!("Drop Table {}", table)]);
            }
            crate::sql::Statement::DropIndex { name, .. } => {
                plan.push(vec![format!("Drop Index {}", name)]);
            }
            crate::sql::Statement::CreateTable { name, columns, engine, .. } => {
                plan.push(vec![format!("Create Table {}", name)]);
                plan.push(vec![format!("  Columns: {}", columns.iter().map(|(c, t)| format!("{} {:?}", c, t)).collect::<Vec<_>>().join(", "))]);
                plan.push(vec![format!("  Engine: {:?}", engine)]);
            }
            _ => {
                plan.push(vec![format!("Query: {}", &query[..query.len().min(60)])]);
            }
        }
    } else {
        plan.push(vec![format!("(Parse error: {})", &query[..query.len().min(60)])]);
    }

    if plan.is_empty() {
        plan.push(vec!["(no plan available)".into()]);
    }

    plan
}

// ── Catalog query handler ───────────────────────────────────────

async fn handle_catalog_query(stream: &mut TcpStream, query: &str, router: &DeltaMainRouter) -> Result<(), Box<dyn std::error::Error>> {
    let upper = query.trim().to_uppercase();
    let tables = router.catalog().list_tables();

    // pg_class — all relations (tables, indexes)
    if upper.contains("PG_CLASS") && !upper.contains("PG_NAMESPACE") && !upper.contains("PG_ATTRIBUTE") {
        let cols = vec!["oid".into(), "relname".into(), "relnamespace".into(), "relkind".into(), "reltuples".into(), "relpages".into()];
        let mut rows: Vec<Vec<String>> = Vec::new();
        for (i, t) in tables.iter().enumerate() {
            let oid = (i + 1000).to_string();
            rows.push(vec![oid, t.name.clone(), "2200".into(), "r".into(), "0".into(), "0".into()]);
        }
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // pg_attribute — columns for each table
    if upper.contains("PG_ATTRIBUTE") {
        let cols = vec!["attrelid".into(), "attname".into(), "atttypid".into(), "attnum".into(), "attnotnull".into(), "attlen".into()];
        let mut rows: Vec<Vec<String>> = Vec::new();
        for (i, t) in tables.iter().enumerate() {
            let oid = (i + 1000).to_string();
            for (j, col) in t.column_names.iter().enumerate() {
                let type_oid = match t.column_types.get(j) {
                    Some(crate::catalog::ColumnType::Integer) => "23",
                    Some(crate::catalog::ColumnType::Float) => "701",
                    Some(crate::catalog::ColumnType::Boolean) => "16",
                    Some(crate::catalog::ColumnType::Jsonb) => "3802",
                    _ => "25", // TEXT
                };
                rows.push(vec![oid.clone(), col.clone(), type_oid.into(), (j+1).to_string(), "f".into(), "-1".into()]);
            }
        }
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // pg_namespace — schemas
    if upper.contains("PG_NAMESPACE") {
        let cols = vec!["oid".into(), "nspname".into(), "nspowner".into(), "nspacl".into()];
        let rows = vec![
            vec!["2200".into(), "public".into(), "1".into(), "".into()],
            vec!["11".into(), "pg_catalog".into(), "1".into(), "".into()],
        ];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // pg_type — data types
    if upper.contains("PG_TYPE") {
        let cols = vec!["oid".into(), "typname".into(), "typlen".into(), "typbasetype".into()];
        let rows = vec![
            vec!["23".into(), "int4".into(), "4".into(), "0".into()],
            vec!["25".into(), "text".into(), "-1".into(), "0".into()],
            vec!["16".into(), "bool".into(), "1".into(), "0".into()],
            vec!["701".into(), "float8".into(), "8".into(), "0".into()],
            vec!["3802".into(), "jsonb".into(), "-1".into(), "0".into()],
        ];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // pg_index — indexes
    if upper.contains("PG_INDEX") {
        let cols = vec!["indexrelid".into(), "indrelid".into(), "indisunique".into(), "indisprimary".into()];
        let rows: Vec<Vec<String>> = vec![];
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
        return Ok(());
    }

    // \dt — list tables (psql shorthand)
    if upper.starts_with("\\DT") {
        let cols = vec!["Schema".into(), "Name".into(), "Type".into(), "Owner".into()];
        let rows: Vec<Vec<String>> = tables.iter().map(|t| {
            vec!["public".into(), t.name.clone(), "table".into(), "nexus".into()]
        }).collect();
        send_query_result(stream, &cols, &rows).await?;
        stream.write_all(&build_cmd_complete("SELECT 1")).await?;
        stream.write_all(&build_ready_for_query()).await?;
    }

    Ok(())
}
