//! SQL Parser + Query Executor.
//!
//! Parses a subset of SQL and executes against the Delta-Main router.
//! Supported: CREATE TABLE, INSERT, SELECT (WHERE, ORDER BY, LIMIT).

use std::collections::BTreeMap;

use crate::catalog::{ColumnType, DeltaMainRouter, StorageEngine};
use crate::row::Row;
use crate::Result;

// ── AST ──────────────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub enum Statement {
    CreateTable {
        name: String,
        columns: Vec<(String, ColumnType)>,
        engine: StorageEngine,
    },
    Insert {
        table: String,
        columns: Option<Vec<String>>,
        values: Vec<Vec<Literal>>,
    },
    Select {
        columns: Vec<SelectColumn>,
        table: String,
        where_clause: Option<WhereClause>,
        order_by: Option<(String, OrderDir)>,
        limit: Option<usize>,
    },
    Update {
        table: String,
        sets: Vec<(String, Literal)>,
        where_clause: Option<WhereClause>,
    },
    Delete {
        table: String,
        where_clause: Option<WhereClause>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub enum SelectColumn {
    All,
    Named(String),
}

#[derive(Debug, Clone, PartialEq)]
pub struct WhereClause {
    pub column: String,
    pub op: CompareOp,
    pub value: Literal,
}

#[derive(Debug, Clone, PartialEq)]
pub enum CompareOp {
    Eq,
    Neq,
    Gt,
    Lt,
    Gte,
    Lte,
}

#[derive(Debug, Clone, PartialEq)]
pub enum OrderDir {
    Asc,
    Desc,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Literal {
    Text(String),
    Integer(i64),
    Float(f64),
    Boolean(bool),
    Null,
}

impl Literal {
    fn to_bytes(&self) -> Vec<u8> {
        match self {
            Self::Text(s) => s.clone().into_bytes(),
            Self::Integer(i) => i.to_string().into_bytes(),
            Self::Float(f) => f.to_string().into_bytes(),
            Self::Boolean(b) => b.to_string().into_bytes(),
            Self::Null => vec![],
        }
    }

    fn from_bytes(bytes: &[u8], col_type: &ColumnType) -> Self {
        match col_type {
            ColumnType::Text => Self::Text(String::from_utf8_lossy(bytes).to_string()),
            ColumnType::Integer => {
                let s = String::from_utf8_lossy(bytes);
                Self::Integer(s.parse().unwrap_or(0))
            }
            ColumnType::Float => {
                let s = String::from_utf8_lossy(bytes);
                Self::Float(s.parse().unwrap_or(0.0))
            }
            ColumnType::Boolean => Self::Boolean(!bytes.is_empty() && bytes[0] != b'0'),
            ColumnType::Blob => Self::Text(format!("<blob:{}>", bytes.len())),
            ColumnType::Jsonb => Self::Text(String::from_utf8_lossy(bytes).to_string()),
        }
    }

    fn compare(&self, other: &Literal, op: &CompareOp) -> bool {
        let ordering = match (self, other) {
            (Self::Integer(a), Self::Integer(b)) => a.cmp(b),
            (Self::Float(a), Self::Float(b)) => a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal),
            (Self::Text(a), Self::Text(b)) => a.cmp(b),
            (Self::Boolean(a), Self::Boolean(b)) => a.cmp(b),
            _ => return false,
        };
        match op {
            CompareOp::Eq => ordering == std::cmp::Ordering::Equal,
            CompareOp::Neq => ordering != std::cmp::Ordering::Equal,
            CompareOp::Gt => ordering == std::cmp::Ordering::Greater,
            CompareOp::Lt => ordering == std::cmp::Ordering::Less,
            CompareOp::Gte => ordering != std::cmp::Ordering::Less,
            CompareOp::Lte => ordering != std::cmp::Ordering::Greater,
        }
    }
}

// ── Parser ───────────────────────────────────────────────────────

pub fn parse_sql(input: &str) -> std::result::Result<Statement, String> {
    let trimmed = input.trim();
    let upper = trimmed.to_uppercase();

    if upper.starts_with("CREATE TABLE") {
        parse_create_table(trimmed)
    } else if upper.starts_with("INSERT INTO") {
        parse_insert(trimmed)
    } else if upper.starts_with("UPDATE") {
        parse_update(trimmed)
    } else if upper.starts_with("DELETE") {
        parse_delete(trimmed)
    } else if upper.starts_with("SELECT") {
        parse_select(trimmed)
    } else {
        Err(format!("Unsupported SQL: {}", &trimmed[..trimmed.len().min(50)]))
    }
}

fn parse_create_table(input: &str) -> std::result::Result<Statement, String> {
    // CREATE TABLE name (col1 TYPE, col2 TYPE) ENGINE=btree
    let rest = input.strip_prefix("CREATE TABLE").unwrap_or(input).trim();
    let rest = rest.strip_prefix("IF NOT EXISTS").unwrap_or(rest).trim();

    // Extract table name
    let (name, rest) = rest.split_once('(').ok_or("Expected '(' after table name")?;
    let name = name.trim().trim_matches('"').to_string();
    // Put the '(' back for proper matching
    let rest_str = format!("({}", rest);

    // Find matching closing paren
    let paren_end = find_matching_paren(&rest_str)?;
    let columns_str = &rest_str[..paren_end];
    let after = rest_str[paren_end + 1..].trim();

    // Parse columns
    let mut columns = Vec::new();
    for col_def in split_by_comma(columns_str) {
        let parts: Vec<_> = col_def.trim().split_whitespace().collect();
        if parts.len() >= 2 {
            let col_name = parts[0].trim_matches('"').to_string();
            let col_type = ColumnType::from_str(parts[1])
                .unwrap_or(ColumnType::Text);
            columns.push((col_name, col_type));
        }
    }

    // Parse ENGINE=xxx
    let engine = if let Some(eng_str) = after.strip_prefix("ENGINE=").or_else(|| after.strip_prefix("ENGINE =")) {
        StorageEngine::from_str(eng_str.trim()).unwrap_or(StorageEngine::Auto)
    } else {
        StorageEngine::Auto
    };

    Ok(Statement::CreateTable { name, columns, engine })
}

fn parse_insert(input: &str) -> std::result::Result<Statement, String> {
    // INSERT INTO name VALUES (v1, v2)
    // INSERT INTO name (c1, c2) VALUES (v1, v2)
    let rest = input.strip_prefix("INSERT INTO").unwrap_or(input).trim();
    let (name, rest) = rest.split_once("VALUES").ok_or("Expected VALUES")?;
    let name = name.trim().trim_matches('"').to_string();
    let rest = rest.trim();

    // Check for column list
    let (name, columns) = if name.contains('(') {
        let (n, cols) = name.split_once('(').unwrap();
        let cols = cols.trim().trim_end_matches(')');
        let col_names: Vec<String> = cols.split(',').map(|c| c.trim().trim_matches('"').to_string()).collect();
        (n.trim().to_string(), Some(col_names))
    } else {
        (name, None)
    };

    // Parse values
    let mut values = Vec::new();
    for val_group in split_paren_groups(rest) {
        let literals: Vec<Literal> = val_group.split(',')
            .map(|v| parse_literal(v.trim()))
            .collect();
        values.push(literals);
    }

    Ok(Statement::Insert { table: name, columns, values })
}

fn parse_select(input: &str) -> std::result::Result<Statement, String> {
    // SELECT col1, col2 FROM name WHERE col = val ORDER BY col ASC LIMIT n
    let rest = input.strip_prefix("SELECT").unwrap_or(input).trim();

    // Parse columns
    let (columns_str, rest) = rest.split_once("FROM").ok_or("Expected FROM")?;
    let columns = if columns_str.trim() == "*" {
        vec![SelectColumn::All]
    } else {
        columns_str.split(',')
            .map(|c| SelectColumn::Named(c.trim().trim_matches('"').to_string()))
            .collect()
    };

    let mut rest = rest.trim();
    let mut table = String::new();
    let mut where_clause = None;
    let mut order_by = None;
    let mut limit: Option<usize> = None;

    // Parse table name (before WHERE/ORDER/LIMIT)
    for keyword in &["WHERE", "ORDER", "LIMIT", ";"] {
        if let Some(pos) = rest.to_uppercase().find(keyword) {
            if pos > 0 {
                table = rest[..pos].trim().trim_matches('"').to_string();
                rest = &rest[pos..];
                break;
            }
        }
    }
    if table.is_empty() {
        table = rest.trim().trim_matches('"').trim_end_matches(';').to_string();
        rest = "";
    }

    // Parse WHERE
    if rest.to_uppercase().starts_with("WHERE") {
        let where_rest = rest.strip_prefix("WHERE").unwrap_or(rest).trim();
        let parts: Vec<_> = where_rest.split_whitespace().collect();
        if parts.len() >= 3 {
            let col = parts[0].to_string();
            let op = match parts[1] {
                "=" => CompareOp::Eq,
                "!=" | "<>" => CompareOp::Neq,
                ">" => CompareOp::Gt,
                "<" => CompareOp::Lt,
                ">=" => CompareOp::Gte,
                "<=" => CompareOp::Lte,
                _ => CompareOp::Eq,
            };
            let val = parse_literal(parts[2].trim_matches('\'').trim_matches('"'));
            where_clause = Some(WhereClause { column: col, op, value: val });
        }
        // Find next keyword position
        let after_where = &where_rest[parts.iter().take(3).map(|s| s.len() + 1).sum::<usize>()..];
        rest = after_where;
    }

    // Parse ORDER BY
    if rest.to_uppercase().contains("ORDER BY") {
        let ob_pos = rest.to_uppercase().find("ORDER BY").unwrap();
        let ob_rest = rest[ob_pos + 8..].trim();
        let parts: Vec<_> = ob_rest.split_whitespace().collect();
        if !parts.is_empty() {
            let col = parts[0].to_string();
            let dir = if parts.len() > 1 && parts[1].to_uppercase() == "DESC" {
                OrderDir::Desc
            } else {
                OrderDir::Asc
            };
            order_by = Some((col, dir));
        }
    }

    // Parse LIMIT
    if rest.to_uppercase().contains("LIMIT") {
        let lim_pos = rest.to_uppercase().find("LIMIT").unwrap();
        let lim_rest = rest[lim_pos + 5..].trim();
        if let Ok(n) = lim_rest.split_whitespace().next().unwrap_or("0").parse() {
            limit = Some(n);
        }
    }

    Ok(Statement::Select { columns, table, where_clause, order_by, limit })
}

// ── Executor ─────────────────────────────────────────────────────

pub fn execute(router: &DeltaMainRouter, stmt: &Statement) -> Result<ExecuteResult> {
    match stmt {
        Statement::CreateTable { name, columns, engine } => {
            router.catalog().create_table(name, *engine, columns.clone())?;
            // Initialize columnar store for this table
            router.columnar.write().create_table(name, columns);
            Ok(ExecuteResult::CreateTable(name.clone()))
        }
        Statement::Insert { table, columns: _, values } => {
            let meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;
            let mut count = 0;
            for row_vals in values {
                let col_data: Vec<Option<Vec<u8>>> = row_vals.iter().enumerate()
                    .map(|(i, val)| {
                        if i < meta.column_names.len() { Some(val.to_bytes()) } else { None }
                    })
                    .collect();
                let row = Row::new(col_data.clone());
                router.insert(table, &row)?;
                // Also write to columnar store for SELECT queries
                router.columnar.write().append_row(table, &col_data, &meta.column_types)?;
                count += 1;
            }
            Ok(ExecuteResult::Insert(count))
        }
        Statement::Select { columns, table, where_clause, order_by, limit } => {
            let meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;

            // Read from columnar store
            let col_store = router.columnar.read();
            let row_count = col_store.row_count(table);

            // Build rows from columnar data
            let mut all_rows: Vec<Row> = Vec::new();
            for row_idx in 0..row_count {
                let mut col_values: Vec<Option<Vec<u8>>> = Vec::new();
                for col_name in &meta.column_names {
                    let val = col_store.get_column(table, col_name)
                        .ok()
                        .and_then(|col| col.get(row_idx).cloned())
                        .flatten();
                    col_values.push(val);
                }
                all_rows.push(Row::new(col_values));
            }
            drop(col_store);

            // Filter with WHERE
            let filtered: Vec<Row> = if let Some(wc) = where_clause {
                let col_idx = meta.column_names.iter().position(|c| c == &wc.column);
                all_rows.into_iter().filter(|row| {
                    if let Some(idx) = col_idx {
                        if let Some(Some(val)) = row.columns.get(idx) {
                            let lit = Literal::from_bytes(val, &meta.column_types[idx]);
                            return lit.compare(&wc.value, &wc.op);
                        }
                    }
                    false
                }).collect()
            } else {
                all_rows
            };

            // Sort with ORDER BY
            let mut sorted = if let Some((col, dir)) = order_by {
                let col_idx = meta.column_names.iter().position(|c| c == col);
                let mut rows = filtered;
                if let Some(idx) = col_idx {
                    rows.sort_by(|a, b| {
                        let va = a.columns.get(idx).and_then(|c| c.as_ref());
                        let vb = b.columns.get(idx).and_then(|c| c.as_ref());
                        let cmp = va.cmp(&vb);
                        match dir { OrderDir::Desc => cmp.reverse(), OrderDir::Asc => cmp }
                    });
                }
                rows
            } else {
                filtered
            };

            // Apply LIMIT
            if let Some(n) = limit {
                sorted.truncate(*n);
            }

            // Select requested columns
            let col_indices: Vec<usize> = match columns.first() {
                Some(SelectColumn::All) | None => (0..meta.column_names.len()).collect(),
                _ => columns.iter().filter_map(|c| {
                    if let SelectColumn::Named(name) = c {
                        meta.column_names.iter().position(|n| n == name)
                    } else { None }
                }).collect(),
            };

            let result_cols: Vec<String> = col_indices.iter()
                .map(|&i| meta.column_names[i].clone())
                .collect();

            let result_rows: Vec<Vec<String>> = sorted.iter().map(|row| {
                col_indices.iter().map(|&i| {
                    row.columns.get(i)
                        .and_then(|c| c.as_ref())
                        .map(|b| String::from_utf8_lossy(b).to_string())
                        .unwrap_or_else(|| "NULL".to_string())
                }).collect()
            }).collect();

            Ok(ExecuteResult::Select { columns: result_cols, rows: result_rows })
        }
        Statement::Update { table: _, sets: _, where_clause: _ } => {
            // Count matching rows (column mutation pending)
            Ok(ExecuteResult::Update(0))
        }
        Statement::Delete { table: _, where_clause: _ } => {
            Ok(ExecuteResult::Delete(0))
        }
    }
}

#[derive(Debug, Clone)]
pub enum ExecuteResult {
    CreateTable(String),
    Insert(usize),
    Select { columns: Vec<String>, rows: Vec<Vec<String>> },
    Update(usize),
    Delete(usize),
}

impl ExecuteResult {
    pub fn to_pgwire_response(&self) -> (Vec<String>, Vec<Vec<String>>) {
        match self {
            Self::CreateTable(name) => (vec!["result".into()], vec![vec![format!("CREATE TABLE {}", name)]]),
            Self::Insert(count) => (vec!["rows".into()], vec![vec![count.to_string()]]),
            Self::Select { columns, rows } => (columns.clone(), rows.clone()),
            Self::Update(count) => (vec!["rows".into()], vec![vec![format!("UPDATE {}", count)]]),
            Self::Delete(count) => (vec!["rows".into()], vec![vec![format!("DELETE {}", count)]]),
        }
    }
}

// ── Helpers ──────────────────────────────────────────────────────

fn find_matching_paren(s: &str) -> std::result::Result<usize, String> {
    let mut depth = 0;
    for (i, c) in s.char_indices() {
        match c {
            '(' => depth += 1,
            ')' => {
                depth -= 1;
                if depth == 0 { return Ok(i); }
            }
            _ => {}
        }
    }
    Err("Unmatched parenthesis".into())
}

fn split_by_comma(s: &str) -> Vec<String> {
    let mut parts = Vec::new();
    let mut current = String::new();
    let mut depth = 0;
    for c in s.chars() {
        match c {
            '(' => { depth += 1; current.push(c); }
            ')' => { depth -= 1; current.push(c); }
            ',' if depth == 0 => {
                parts.push(current.trim().to_string());
                current.clear();
            }
            _ => current.push(c),
        }
    }
    if !current.trim().is_empty() {
        parts.push(current.trim().to_string());
    }
    parts
}

fn split_paren_groups(s: &str) -> Vec<String> {
    let mut groups = Vec::new();
    let mut current = String::new();
    let mut depth = 0;
    for c in s.chars() {
        match c {
            '(' => { depth += 1; if depth == 1 { continue; } current.push(c); }
            ')' => {
                depth -= 1;
                if depth == 0 {
                    groups.push(current.trim().to_string());
                    current.clear();
                    continue;
                }
                current.push(c);
            }
            _ if depth > 0 => current.push(c),
            _ => {}
        }
    }
    groups
}

fn parse_literal(s: &str) -> Literal {
    let s = s.trim();
    if s.eq_ignore_ascii_case("NULL") { return Literal::Null; }
    if s.eq_ignore_ascii_case("TRUE") { return Literal::Boolean(true); }
    if s.eq_ignore_ascii_case("FALSE") { return Literal::Boolean(false); }
    if let Ok(i) = s.parse::<i64>() { return Literal::Integer(i); }
    if let Ok(f) = s.parse::<f64>() { return Literal::Float(f); }
    // Strip quotes
    let s = s.trim_matches('\'').trim_matches('"');
    Literal::Text(s.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_create_table() {
        let stmt = parse_sql("CREATE TABLE users (id INTEGER, name TEXT)").unwrap();
        assert_eq!(stmt, Statement::CreateTable {
            name: "users".into(),
            columns: vec![("id".into(), ColumnType::Integer), ("name".into(), ColumnType::Text)],
            engine: StorageEngine::BTree,
        });
    }

    #[test]
    fn test_parse_insert() {
        let stmt = parse_sql("INSERT INTO users VALUES (1, 'alice')").unwrap();
        match stmt {
            Statement::Insert { table, values, .. } => {
                assert_eq!(table, "users");
                assert_eq!(values.len(), 1);
                assert_eq!(values[0][0], Literal::Integer(1));
            }
            _ => panic!("Expected Insert"),
        }
    }

    #[test]
    fn test_parse_select() {
        let stmt = parse_sql("SELECT * FROM users WHERE name = 'alice' ORDER BY id DESC LIMIT 10").unwrap();
        match stmt {
            Statement::Select { table, where_clause, order_by, limit, .. } => {
                assert_eq!(table, "users");
                assert!(where_clause.is_some());
                assert_eq!(order_by.unwrap().0, "id");
                assert_eq!(limit, Some(10));
            }
            _ => panic!("Expected Select"),
        }
    }

    #[test]
    fn test_literal_parsing() {
        assert_eq!(parse_literal("42"), Literal::Integer(42));
        assert_eq!(parse_literal("'hello'"), Literal::Text("hello".into()));
        assert_eq!(parse_literal("NULL"), Literal::Null);
        assert_eq!(parse_literal("TRUE"), Literal::Boolean(true));
    }
}

// ── UPDATE / DELETE parsing ──────────────────────────────────────

fn parse_update(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("UPDATE").unwrap_or(input).trim();
    let (table, rest) = rest.split_once("SET").ok_or("Expected SET")?;
    let table = table.trim().trim_matches('"').to_string();
    let rest = rest.trim();

    let (sets_str, where_str) = if let Some(pos) = rest.to_uppercase().find("WHERE") {
        (&rest[..pos], Some(&rest[pos + 5..]))
    } else { (rest, None) };

    let sets: Vec<(String, Literal)> = sets_str.split(',')
        .filter_map(|pair| {
            let parts: Vec<_> = pair.trim().splitn(2, '=').collect();
            if parts.len() == 2 { Some((parts[0].trim().to_string(), parse_literal(parts[1].trim()))) } else { None }
        }).collect();

    let where_clause = where_str.and_then(|w| parse_where(w));
    Ok(Statement::Update { table, sets, where_clause })
}

fn parse_delete(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("DELETE FROM").unwrap_or(input).trim();
    let (table, rest) = if let Some(pos) = rest.to_uppercase().find("WHERE") {
        (rest[..pos].trim(), Some(&rest[pos + 5..]))
    } else { (rest.trim(), None) };
    let table = table.trim_matches('"').trim_end_matches(';').to_string();
    let where_clause = rest.and_then(|w| parse_where(w));
    Ok(Statement::Delete { table, where_clause })
}

fn parse_where(input: &str) -> Option<WhereClause> {
    let parts: Vec<_> = input.trim().split_whitespace().collect();
    if parts.len() >= 3 {
        Some(WhereClause {
            column: parts[0].to_string(),
            op: match parts[1] { "=" => CompareOp::Eq, "!=" => CompareOp::Neq, ">" => CompareOp::Gt, "<" => CompareOp::Lt, _ => CompareOp::Eq },
            value: parse_literal(parts[2].trim_matches('\'').trim_matches('"')),
        })
    } else { None }
}
