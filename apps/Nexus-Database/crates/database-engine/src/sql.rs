//! SQL Parser + Query Executor.
//!
//! Parses a subset of SQL and executes against the Delta-Main router.
//! Supported: CREATE TABLE, INSERT, SELECT (WHERE, ORDER BY, LIMIT).

use std::collections::BTreeMap;

use crate::catalog::{ColumnType, DeltaMainRouter, StorageEngine, UndoAction};
use crate::columnar::{AggregateFunc as ColAggFunc, AggregateState};
use crate::row::Row;
use crate::Result;

// ── AST ──────────────────────────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub enum Statement {
    CreateTable {
        name: String,
        columns: Vec<(String, ColumnType)>,
        column_defs: Vec<String>,
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
        order_by: Vec<(String, OrderDir)>,
        limit: Option<usize>,
        offset: Option<usize>,
        group_by: Option<String>,
        aggregates: Vec<AggregateExpr>,
        join: Option<JoinClause>,
        distinct: bool,
        having: Option<WhereClause>,
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
    Truncate {
        table: String,
    },
    AlterTable {
        table: String,
        action: AlterAction,
    },
    DropTable {
        table: String,
        if_exists: bool,
    },
    DropIndex {
        name: String,
        if_exists: bool,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub enum AlterAction {
    AddColumn { name: String, col_type: ColumnType, column_def: String },
    DropColumn { name: String },
    RenameTable { new_name: String },
    RenameColumn { old_name: String, new_name: String },
}

#[derive(Debug, Clone, PartialEq)]
pub struct AggregateExpr {
    pub func: String,  // COUNT, SUM, AVG, MIN, MAX
    pub column: String,
    pub alias: Option<String>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum SelectColumn {
    All,
    Named(String),
}

#[derive(Debug, Clone, PartialEq)]
pub struct WhereClause {
    pub predicate: WherePredicate,
}

#[derive(Debug, Clone, PartialEq)]
pub enum WherePredicate {
    Compare { column: String, op: CompareOp, value: Literal },
    IsNull { column: String, not: bool },
    Like { column: String, pattern: String, not: bool },
    InList { column: String, values: Vec<Literal>, not: bool },
    Between { column: String, low: Literal, high: Literal, not: bool },
}

impl WhereClause {
    pub fn column(&self) -> &str {
        match &self.predicate {
            WherePredicate::Compare { column, .. } | WherePredicate::IsNull { column, .. } | WherePredicate::Like { column, .. } | WherePredicate::InList { column, .. } | WherePredicate::Between { column, .. } => column,
        }
    }

    /// Evaluate the predicate against a string value from a row.
    pub fn matches(&self, row_val: &str) -> bool {
        match &self.predicate {
            WherePredicate::Compare { op, value, .. } => {
                let lit = Literal::Text(row_val.to_string());
                lit.compare(value, op)
            }
            WherePredicate::IsNull { not, .. } => {
                let is_null = row_val == "NULL" || row_val.is_empty();
                if *not { !is_null } else { is_null }
            }
            WherePredicate::Like { pattern, not, .. } => {
                let matched = like_match(row_val, pattern);
                if *not { !matched } else { matched }
            }
            WherePredicate::InList { values, not, .. } => {
                let lit = Literal::Text(row_val.to_string());
                let found = values.iter().any(|v| lit.compare(v, &CompareOp::Eq));
                if *not { !found } else { found }
            }
            WherePredicate::Between { low, high, not, .. } => {
                let lit = Literal::Text(row_val.to_string());
                let in_range = lit.compare(low, &CompareOp::Gte) && lit.compare(high, &CompareOp::Lte);
                if *not { !in_range } else { in_range }
            }
        }
    }

    /// Evaluate against a byte-slice value (used in UPDATE/DELETE/JOIN).
    pub fn matches_bytes(&self, val: &Option<Vec<u8>>, col_type: &crate::catalog::ColumnType) -> bool {
        if let WherePredicate::Compare { value, op, .. } = &self.predicate {
            if let Some(bytes) = val {
                let lit = Literal::from_bytes(bytes, col_type);
                return lit.compare(value, op);
            }
            return matches!(value, Literal::Null);
        }
        if let WherePredicate::IsNull { not, .. } = &self.predicate {
            return if *not { val.is_some() } else { val.is_none() };
        }
        let s = val.as_ref().map(|b| String::from_utf8_lossy(b).to_string()).unwrap_or("NULL".into());
        self.matches(&s)
    }
}

/// Simple SQL LIKE pattern matching.
fn like_match(s: &str, pattern: &str) -> bool {
    let pat = pattern.trim_matches('\'').trim_matches('"');
    if !pat.contains('%') && !pat.contains('_') {
        return s == pat;
    }
    let mut si = 0;
    let chars: Vec<char> = pat.chars().collect();
    let mut i = 0;
    while i < chars.len() {
        match chars[i] {
            '%' => {
                i += 1;
                if i >= chars.len() { return true; }
                let mut found = false;
                while si < s.len() {
                    if like_match_rest(&s[si..], &chars[i..]) {
                        found = true;
                        break;
                    }
                    si += 1;
                }
                return found;
            }
            '_' => {
                if si >= s.len() { return false; }
                si += 1;
                i += 1;
            }
            ch => {
                if si >= s.len() || s.chars().nth(si) != Some(ch) { return false; }
                si += 1;
                i += 1;
            }
        }
    }
    si == s.len()
}

fn like_match_rest(s: &str, pattern: &[char]) -> bool {
    let mut si = 0;
    let mut i = 0;
    while i < pattern.len() {
        match pattern[i] {
            '%' => {
                i += 1;
                if i >= pattern.len() { return true; }
                while si < s.len() {
                    if like_match_rest(&s[si..], &pattern[i..]) { return true; }
                    si += 1;
                }
                return false;
            }
            '_' => {
                if si >= s.len() { return false; }
                si += 1;
                i += 1;
            }
            ch => {
                if si >= s.len() || s.chars().nth(si) != Some(ch) { return false; }
                si += 1;
                i += 1;
            }
        }
    }
    si == s.len()
}

#[derive(Debug, Clone, PartialEq)]
pub struct JoinClause {
    pub join_type: JoinType,
    pub left_table: String,
    pub right_table: String,
    pub condition: JoinCondition,
}

#[derive(Debug, Clone, PartialEq)]
pub enum JoinType {
    Inner,
    Left,
}

#[derive(Debug, Clone, PartialEq)]
pub struct JoinCondition {
    pub left: String,  // table.col or just col
    pub op: CompareOp,
    pub right: String,
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
            (Self::Text(a), Self::Integer(b)) => a.parse::<i64>().unwrap_or(0).cmp(b),
            (Self::Integer(a), Self::Text(b)) => a.cmp(&b.parse::<i64>().unwrap_or(0)),
            (Self::Text(a), Self::Float(b)) => a.parse::<f64>().unwrap_or(0.0).partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal),
            (Self::Float(a), Self::Text(b)) => a.partial_cmp(&b.parse::<f64>().unwrap_or(0.0)).unwrap_or(std::cmp::Ordering::Equal),
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
    } else if upper.starts_with("TRUNCATE") {
        parse_truncate(trimmed)
    } else if upper.starts_with("ALTER TABLE") {
        parse_alter_table(trimmed)
    } else if upper.starts_with("DROP TABLE") {
        parse_drop_table(trimmed)
    } else if upper.starts_with("DROP INDEX") {
        parse_drop_index(trimmed)
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
    let columns_str = &rest_str[1..paren_end];
    let after = rest_str[paren_end + 1..].trim();

    // Parse columns
    let mut columns = Vec::new();
    let mut column_defs = Vec::new();
    for col_def in split_by_comma(columns_str) {
        let trimmed = col_def.trim();
        column_defs.push(trimmed.to_string());
        let parts: Vec<_> = trimmed.split_whitespace().collect();
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

    Ok(Statement::CreateTable { name, columns, column_defs, engine })
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
    let rest = input.strip_prefix("SELECT").unwrap_or(input).trim();

    // Check for DISTINCT
    let (distinct, rest) = if rest.to_uppercase().starts_with("DISTINCT ") {
        (true, rest.strip_prefix("DISTINCT").unwrap_or(rest).trim())
    } else {
        (false, rest)
    };

    // Parse columns — may include aggregates like COUNT(col), SUM(col)
    let (columns_str, rest) = rest.split_once("FROM").ok_or("Expected FROM")?;
    let mut columns = Vec::new();
    let mut aggregates = Vec::new();
    for col_part in columns_str.split(',') {
        let col_part = col_part.trim();
        if col_part == "*" {
            columns.push(SelectColumn::All);
        } else if let Some(agg) = parse_aggregate(col_part) {
            aggregates.push(agg);
        } else {
            columns.push(SelectColumn::Named(col_part.trim_matches('"').to_string()));
        }
    }

    let mut rest = rest.trim();
    let mut table = String::new();
    let mut where_clause = None;
    let mut order_by: Vec<(String, OrderDir)> = Vec::new();
    let mut limit: Option<usize> = None;
    let mut offset: Option<usize> = None;
    let mut group_by: Option<String> = None;
    let mut having: Option<WhereClause> = None;
    let mut join: Option<JoinClause> = None;

    // Check for JOIN clause
    let rest_upper = rest.to_uppercase();
    let join_keyword = rest_upper.find("JOIN");
    if let Some(jp) = join_keyword {
        table = rest[..jp].trim().trim_matches('"').to_string();
        let after_join = rest[jp..].trim();

        // Determine join type
        let (join_type, after_type) = if after_join.to_uppercase().starts_with("LEFT") {
            let a = after_join.strip_prefix("LEFT").unwrap_or(after_join).trim();
            let a = a.strip_prefix("OUTER").unwrap_or(a).trim();
            (JoinType::Left, a.strip_prefix("JOIN").unwrap_or(a).trim())
        } else if after_join.to_uppercase().starts_with("INNER") {
            (JoinType::Inner, after_join.strip_prefix("INNER").unwrap_or(after_join).trim().strip_prefix("JOIN").unwrap_or(after_join).trim())
        } else {
            (JoinType::Inner, after_join.strip_prefix("JOIN").unwrap_or(after_join).trim())
        };

        // Parse right table and ON condition
        let after_upper = after_type.to_uppercase();
        if let Some(opos) = after_upper.find("ON ") {
            let right_table = after_type[..opos].trim().trim_matches('"').to_string();
            let on_clause = after_type[opos + 3..].trim();
            let parts: Vec<_> = on_clause.split_whitespace().collect();
            if parts.len() >= 3 {
                let op = match parts[1] { "=" => CompareOp::Eq, "!=" | "<>" => CompareOp::Neq, _ => CompareOp::Eq };
                join = Some(JoinClause {
                    join_type,
                    left_table: table.clone(),
                    right_table,
                    condition: JoinCondition { left: parts[0].to_string(), op, right: parts[2].to_string() },
                });
            }
        }
        rest = "";
    } else {
        // No JOIN — parse table name normally
        for keyword in &["WHERE", "GROUP", "ORDER", "LIMIT", ";"] {
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
    }

    // Parse WHERE
    if rest.to_uppercase().starts_with("WHERE") {
        let where_rest = rest.strip_prefix("WHERE").unwrap_or(rest).trim();
        if let Some(wc) = parse_where_clause(where_rest) {
            where_clause = Some(wc);
        }
        // Find next keyword position to advance rest
        let mut min_pos: Option<usize> = None;
        for kw in &["ORDER BY", "GROUP BY", "HAVING", "LIMIT", "OFFSET", ";"] {
            if let Some(p) = where_rest.to_uppercase().find(kw) {
                if min_pos.map_or(true, |m| p < m) { min_pos = Some(p); }
            }
        }
        rest = if let Some(p) = min_pos { &where_rest[p..] } else { "" };
    }

    // Parse GROUP BY
    if rest.to_uppercase().contains("GROUP") {
        let gb_pos = rest.to_uppercase().find("GROUP").unwrap_or(0);
        let group_rest = rest[gb_pos..].trim();
        let group_rest = if group_rest.to_uppercase().starts_with("GROUP BY") {
            group_rest[8..].trim()
        } else {
            group_rest[5..].trim()
        };
        let gb_col = group_rest.split_whitespace().next().unwrap_or("").trim_matches('"').to_string();
        group_by = Some(gb_col);
    }

    // Parse HAVING
    if rest.to_uppercase().contains("HAVING") {
        let having_pos = rest.to_uppercase().find("HAVING").unwrap();
        let having_rest = rest[having_pos + 6..].trim();
        if let Some(wc) = parse_where_clause(having_rest) {
            having = Some(wc);
        }
    }

    // Parse ORDER BY
    if rest.to_uppercase().contains("ORDER BY") {
        let ob_pos = rest.to_uppercase().find("ORDER BY").unwrap();
        let ob_rest = rest[ob_pos + 8..].trim();
        for pair in ob_rest.split(',') {
            let parts: Vec<_> = pair.trim().split_whitespace().collect();
            if !parts.is_empty() {
                let col = parts[0].trim_matches('"').to_string();
                let dir = if parts.len() > 1 && parts[1].to_uppercase() == "DESC" {
                    OrderDir::Desc
                } else {
                    OrderDir::Asc
                };
                order_by.push((col, dir));
            }
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

    // Parse OFFSET
    if rest.to_uppercase().contains("OFFSET") {
        let off_pos = rest.to_uppercase().find("OFFSET").unwrap();
        let off_rest = rest[off_pos + 6..].trim();
        if let Ok(n) = off_rest.split_whitespace().next().unwrap_or("0").parse() {
            offset = Some(n);
        }
    }

    Ok(Statement::Select { columns, table, where_clause, order_by, limit, offset, group_by, aggregates, join, distinct, having })
}

/// Parse aggregate expression like COUNT(*), SUM(price), AVG(amount)
fn parse_aggregate(s: &str) -> Option<AggregateExpr> {
    let upper = s.to_uppercase().trim().to_string();
    let func_pos = upper.find('(')?;
    let func = upper[..func_pos].trim().to_string();
    let inside = &s[func_pos + 1..].trim();
    let inside = inside.trim_end_matches(')').trim();
    let column = if inside == "*" { "*".to_string() } else { inside.trim_matches('"').to_string() };

    // Optional alias: COUNT(col) AS cnt
    let alias = if let Some(as_pos) = s[func_pos + inside.len() + 1..].to_uppercase().find(" AS ") {
        let after = &s[func_pos + inside.len() + 1 + as_pos + 4..].trim();
        Some(after.to_string())
    } else {
        None
    };

    match func.as_str() {
        "COUNT" | "SUM" | "AVG" | "MIN" | "MAX" => Some(AggregateExpr { func, column, alias }),
        _ => None,
    }
}

// ── Executor ─────────────────────────────────────────────────────

pub fn execute(router: &DeltaMainRouter, stmt: &Statement) -> Result<ExecuteResult> {
    match stmt {
        Statement::CreateTable { name, columns, column_defs, engine } => {
            router.catalog().create_table(name, *engine, columns.clone())?;
            // Initialize columnar store for this table
            router.columnar.write().create_table(name, columns);
            // Auto-create unique indexes for UNIQUE-constrained columns
            let table_meta = router.catalog().get_table(name).unwrap();
            for col_def in column_defs {
                if col_def.to_uppercase().contains("UNIQUE") {
                    let col_name = col_def.split_whitespace().next().unwrap_or("").trim_matches('"');
                    let idx_name = format!("{}_{}_unique", name, col_name);
                    let _ = router.indexes.create_index(&idx_name, name, vec![col_name.to_string()], true, crate::index::IndexType::BTree, &table_meta);
                }
            }
            Ok(ExecuteResult::CreateTable(name.clone()))
        }
        Statement::Insert { table, ref columns, values } => {
            let meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;

            // Build column mapping if explicit columns provided
            let col_indices: Vec<usize> = if let Some(cols) = columns {
                cols.iter().map(|c| meta.column_names.iter().position(|n| n == c).unwrap_or(0)).collect()
            } else {
                (0..meta.column_names.len()).collect()
            };

            let mut count = 0;
            for row_vals in values {
                let mut col_data: Vec<Option<Vec<u8>>> = vec![None; meta.column_names.len()];
                for (i, val) in row_vals.iter().enumerate() {
                    let target = col_indices.get(i).copied().unwrap_or(i);
                    if target < col_data.len() {
                        col_data[target] = Some(val.to_bytes());
                    }
                }
                let row = Row::new(col_data.clone());
                let row_idx = router.columnar.read().row_count(table);
                router.insert(table, &row)?;
                router.columnar.write().append_row(table, &col_data, &meta.column_types)?;
                router.undo_stack.write().push(UndoAction::Insert { table: table.clone(), row_idx });
                count += 1;
            }
            Ok(ExecuteResult::Insert(count))
        }
        Statement::Select { columns, table, where_clause, order_by, limit, offset, group_by, aggregates, join, distinct, having } => {
            let meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;

            // If aggregates or GROUP BY, use columnar aggregation engine
            if !aggregates.is_empty() || group_by.is_some() {
                let colar = router.columnar.read();
                let mut states: Vec<_> = aggregates.iter().map(|a| {
                    let func = match a.func.as_str() {
                        "COUNT" => ColAggFunc::Count,
                        "SUM" => ColAggFunc::Sum,
                        "AVG" => ColAggFunc::Avg,
                        "MIN" => ColAggFunc::Min,
                        "MAX" => ColAggFunc::Max,
                        _ => ColAggFunc::Count,
                    };
                    AggregateState::new(func, &a.column, a.alias.as_deref())
                }).collect();

                if let Some(gb_col) = group_by {
                    let mut rows = crate::columnar::execute_group_by(&colar, table, gb_col, &mut states)?;
                    let cols: Vec<String> = std::iter::once(gb_col.clone())
                        .chain(aggregates.iter().map(|a| a.alias.clone().unwrap_or_else(|| format!("{}({})", a.func, a.column))))
                        .collect();
                    if let Some(having_clause) = having {
                        apply_having(&cols, &mut rows, having_clause);
                    }
                    return Ok(ExecuteResult::Select { columns: cols, rows });
                } else {
                    let mut rows = vec![crate::columnar::execute_aggregate(&colar, table, &mut states)?];
                    let cols: Vec<String> = aggregates.iter().map(|a| a.alias.clone().unwrap_or_else(|| format!("{}({})", a.func, a.column))).collect();
                    if let Some(having_clause) = having {
                        apply_having(&cols, &mut rows, having_clause);
                    }
                    return Ok(ExecuteResult::Select { columns: cols, rows });
                }
            }

            // JOIN execution
            if let Some(jc) = join {
                let left_meta = meta;
                let right_meta = router.catalog().get_table(&jc.right_table)
                    .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", jc.right_table)))?;

                let col_store = router.columnar.read();

                // Resolve join condition columns
                let resolve_col = |name: &str, left_meta: &crate::catalog::TableMeta, right_meta: &crate::catalog::TableMeta| -> Option<(String, usize)> {
                    if let Some(pos) = name.find('.') {
                        let tbl = &name[..pos];
                        let col = &name[pos+1..];
                        if tbl == left_meta.name {
                            left_meta.column_names.iter().position(|c| c == col).map(|i| (format!("{}.{}", tbl, col), i))
                        } else if tbl == right_meta.name {
                            right_meta.column_names.iter().position(|c| c == col).map(|i| (format!("{}.{}", tbl, col), i))
                        } else { None }
                    } else {
                        left_meta.column_names.iter().position(|c| c == name).map(|i| (left_meta.column_names[i].clone(), i))
                            .or_else(|| right_meta.column_names.iter().position(|c| c == name).map(|i| (right_meta.column_names[i].clone(), i)))
                    }
                };

                let left_resolved = resolve_col(&jc.condition.left, &left_meta, &right_meta);
                let right_resolved = resolve_col(&jc.condition.right, &left_meta, &right_meta);
                let (left_col_idx, right_col_idx) = match (left_resolved, right_resolved) {
                    (Some((_, li)), Some((_, ri))) => {
                        // Ensure left col is from left table, right col from right table
                        if li < left_meta.column_names.len() && ri >= left_meta.column_names.len() {
                            (li, ri - left_meta.column_names.len())
                        } else if ri < left_meta.column_names.len() && li >= left_meta.column_names.len() {
                            (li - left_meta.column_names.len(), ri) // swapped: left col is in right, right col is in left
                        } else {
                            (li, ri) // both in same table? fallback
                        }
                    }
                    _ => (0, 0),
                };
                // Simpler approach: assume left col is in left table, right col is in right table
                let left_join_col = left_meta.column_names.iter().position(|c| {
                    let name = if let Some(pos) = jc.condition.left.find('.') { &jc.condition.left[pos+1..] } else { &jc.condition.left };
                    c == name
                }).unwrap_or(0);
                let right_join_col = right_meta.column_names.iter().position(|c| {
                    let name = if let Some(pos) = jc.condition.right.find('.') { &jc.condition.right[pos+1..] } else { &jc.condition.right };
                    c == name
                }).unwrap_or(0);

                // Load left table rows
                let left_rows: Vec<Vec<Option<Vec<u8>>>> = (0..col_store.row_count(table)).map(|ri| {
                    left_meta.column_names.iter().map(|cn| {
                        col_store.get_column(table, cn).ok().and_then(|col| col.get(ri).cloned()).flatten()
                    }).collect()
                }).collect();

                // Load right table rows
                let right_rows: Vec<Vec<Option<Vec<u8>>>> = (0..col_store.row_count(&jc.right_table)).map(|ri| {
                    right_meta.column_names.iter().map(|cn| {
                        col_store.get_column(&jc.right_table, cn).ok().and_then(|col| col.get(ri).cloned()).flatten()
                    }).collect()
                }).collect();

                drop(col_store);

                // Nested loop join
                let mut result_cols: Vec<String> = left_meta.column_names.iter()
                    .chain(right_meta.column_names.iter()).cloned().collect();
                let mut result_rows: Vec<Vec<String>> = Vec::new();

                for l_row in &left_rows {
                    let mut matched = false;
                    for r_row in &right_rows {
                    let l_val = l_row.get(left_join_col).and_then(|v| v.as_ref());
                    let r_val = r_row.get(right_join_col).and_then(|v| v.as_ref());

                    let matches = match jc.condition.op {
                        CompareOp::Eq => l_val == r_val,
                        CompareOp::Neq => l_val != r_val,
                        _ => l_val == r_val, // Gt/Lt/Gte/Lte default to Eq for simplicity
                    };

                        if matches {
                            matched = true;
                            let merged: Vec<String> = l_row.iter()
                                .map(|v| v.as_ref().map(|b| String::from_utf8_lossy(b).to_string()).unwrap_or("NULL".into()))
                                .chain(r_row.iter().map(|v| v.as_ref().map(|b| String::from_utf8_lossy(b).to_string()).unwrap_or("NULL".into())))
                                .collect();
                            result_rows.push(merged);
                        }
                    }
                    // LEFT JOIN: if no match, include left row with NULLs for right columns
                    if !matched && matches!(jc.join_type, JoinType::Left) {
                        let merged: Vec<String> = l_row.iter()
                            .map(|v| v.as_ref().map(|b| String::from_utf8_lossy(b).to_string()).unwrap_or("NULL".into()))
                            .chain(right_meta.column_names.iter().map(|_| "NULL".to_string()))
                            .collect();
                        result_rows.push(merged);
                    }
                }

                // Apply WHERE filter if present
                if let Some(wc) = where_clause {
                    let col_idx = result_cols.iter().position(|c| c == wc.column());
                    result_rows.retain(|row| {
                        if let Some(idx) = col_idx {
                            return wc.matches(&row[idx]);
                        }
                        false
                    });
                }

                // Apply ORDER BY (JOIN results use string-based columns)
                if !order_by.is_empty() {
                    result_rows.sort_by(|a, b| {
                        for (col, dir) in order_by {
                            if let Some(ci) = result_cols.iter().position(|c| c == col) {
                                let cmp = a.get(ci).cmp(&b.get(ci));
                                if cmp != std::cmp::Ordering::Equal {
                                    return match dir { OrderDir::Desc => cmp.reverse(), OrderDir::Asc => cmp };
                                }
                            }
                        }
                        std::cmp::Ordering::Equal
                    });
                }

                // Apply OFFSET
                if let Some(off) = offset {
                    if *off < result_rows.len() {
                        result_rows = result_rows.split_off(*off);
                    } else {
                        result_rows.clear();
                    }
                }

                // Apply LIMIT
                if let Some(n) = limit {
                    result_rows.truncate(*n);
                }

                if *distinct {
                    dedup_rows(&mut result_rows);
                }

                return Ok(ExecuteResult::Select { columns: result_cols, rows: result_rows });
            }

            // Standard SELECT (no aggregates)
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
                let col_idx = meta.column_names.iter().position(|c| c == wc.column());
                all_rows.into_iter().filter(|row| {
                    if let Some(idx) = col_idx {
                        if let Some(val) = row.columns.get(idx) {
                            return wc.matches_bytes(val, &meta.column_types[idx]);
                        }
                    }
                    false
                }).collect()
            } else {
                all_rows
            };

            // Sort with ORDER BY
            let mut sorted = if !order_by.is_empty() {
                let mut rows = filtered;
                rows.sort_by(|a, b| {
                    for (col, dir) in order_by {
                        if let Some(idx) = meta.column_names.iter().position(|c| c == col) {
                            let va = a.columns.get(idx).and_then(|c| c.as_ref());
                            let vb = b.columns.get(idx).and_then(|c| c.as_ref());
                            let cmp = va.cmp(&vb);
                            if cmp != std::cmp::Ordering::Equal {
                                return match dir { OrderDir::Desc => cmp.reverse(), OrderDir::Asc => cmp };
                            }
                        }
                    }
                    std::cmp::Ordering::Equal
                });
                rows
            } else {
                filtered
            };

            // Apply OFFSET
            if let Some(off) = offset {
                if *off < sorted.len() {
                    sorted = sorted.split_off(*off);
                } else {
                    sorted.clear();
                }
            }

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

            let mut result_rows: Vec<Vec<String>> = sorted.iter().map(|row| {
                col_indices.iter().map(|&i| {
                    row.columns.get(i)
                        .and_then(|c| c.as_ref())
                        .map(|b| String::from_utf8_lossy(b).to_string())
                        .unwrap_or_else(|| "NULL".to_string())
                }).collect()
            }).collect();

            if *distinct {
                dedup_rows(&mut result_rows);
            }

            Ok(ExecuteResult::Select { columns: result_cols, rows: result_rows })
        }
        Statement::AlterTable { table, action } => {
            match action {
                AlterAction::AddColumn { name, col_type, column_def } => {
                    router.catalog().add_column(table, name, col_type.clone())?;
                    router.columnar.write().add_column(table, name, col_type.clone());
                    // Auto-create unique index if column is UNIQUE
                    if column_def.to_uppercase().contains("UNIQUE") {
                        let table_meta = router.catalog().get_table(table).unwrap();
                        let idx_name = format!("{}_{}_unique", table, name);
                        let _ = router.indexes.create_index(&idx_name, table, vec![name.clone()], true, crate::index::IndexType::BTree, &table_meta);
                    }
                    Ok(ExecuteResult::AlterTable(table.clone()))
                }
                AlterAction::DropColumn { name } => {
                    router.catalog().drop_column(table, name)?;
                    router.columnar.write().drop_column(table, name);
                    Ok(ExecuteResult::AlterTable(table.clone()))
                }
                AlterAction::RenameTable { new_name } => {
                    router.catalog().rename_table(table, new_name)?;
                    router.columnar.write().rename_table(table, new_name);
                    Ok(ExecuteResult::AlterTable(new_name.clone()))
                }
                AlterAction::RenameColumn { old_name, new_name } => {
                    router.catalog().rename_column(table, old_name, new_name)?;
                    router.columnar.write().rename_column(table, old_name, new_name);
                    Ok(ExecuteResult::AlterTable(table.clone()))
                }
            }
        }
        Statement::Update { table, sets, where_clause } => {
            let meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;

            let col_store = router.columnar.read();
            let row_count = col_store.row_count(table);

            // Build rows from columnar data
            let mut all_rows: Vec<(usize, Vec<Option<Vec<u8>>>)> = Vec::new();
            for row_idx in 0..row_count {
                let mut col_values: Vec<Option<Vec<u8>>> = Vec::new();
                for col_name in &meta.column_names {
                    let val = col_store.get_column(table, col_name)
                        .ok()
                        .and_then(|col| col.get(row_idx).cloned())
                        .flatten();
                    col_values.push(val);
                }
                all_rows.push((row_idx, col_values));
            }
            drop(col_store);

            // Find matching rows (WHERE clause)
            let match_indices: Vec<usize> = if let Some(wc) = where_clause {
                let col_idx = meta.column_names.iter().position(|c| c == wc.column());
                all_rows.iter().filter(|(_, row)| {
                    if let Some(idx) = col_idx {
                        if let Some(val) = row.get(idx) {
                            return wc.matches_bytes(val, &meta.column_types[idx]);
                        }
                    }
                    false
                }).map(|(idx, _)| *idx).collect()
            } else {
                (0..row_count).collect()
            };

            let updated = match_indices.len();
            // Update matching rows
            for &row_idx in &match_indices {
                for (col, val) in sets {
                    let old_val = router.columnar.read().get_column(table, col)
                        .ok().and_then(|c| c.get(row_idx).cloned()).flatten();
                    router.undo_stack.write().push(UndoAction::Update {
                        table: table.clone(), row_idx, column: col.clone(), old_value: old_val,
                    });
                    let bytes = val.to_bytes();
                    let _ = router.columnar.write().update_row(table, col, row_idx, Some(bytes));
                }
            }

            Ok(ExecuteResult::Update(updated))
        }
        Statement::Delete { table, where_clause } => {
            let meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;

            let col_store = router.columnar.read();
            let row_count = col_store.row_count(table);

            // Build rows from columnar data
            let mut all_rows: Vec<(usize, Vec<Option<Vec<u8>>>)> = Vec::new();
            for row_idx in 0..row_count {
                let mut col_values: Vec<Option<Vec<u8>>> = Vec::new();
                for col_name in &meta.column_names {
                    let val = col_store.get_column(table, col_name)
                        .ok()
                        .and_then(|col| col.get(row_idx).cloned())
                        .flatten();
                    col_values.push(val);
                }
                all_rows.push((row_idx, col_values));
            }
            drop(col_store);

            // Find matching rows
            let match_indices: Vec<usize> = if let Some(wc) = where_clause {
                let col_idx = meta.column_names.iter().position(|c| c == wc.column());
                all_rows.iter().filter(|(_, row)| {
                    if let Some(idx) = col_idx {
                        if let Some(val) = row.get(idx) {
                            return wc.matches_bytes(val, &meta.column_types[idx]);
                        }
                    }
                    false
                }).map(|(idx, _)| *idx).collect()
            } else {
                (0..row_count).collect()
            };

            let deleted = match_indices.len();
            // Delete matching rows (reverse order to keep indices valid)
            for &row_idx in match_indices.iter().rev() {
                // Snapshot row data for undo
                let row_data: Vec<Option<Vec<u8>>> = meta.column_names.iter().map(|cn| {
                    router.columnar.read().get_column(table, cn)
                        .ok().and_then(|c| c.get(row_idx).cloned()).flatten()
                }).collect();
                router.undo_stack.write().push(UndoAction::Delete {
                    table: table.clone(), row_idx, data: row_data,
                });
                let _ = router.columnar.write().delete_row(table, row_idx);
            }

            Ok(ExecuteResult::Delete(deleted))
        }
        Statement::Truncate { table } => {
            // Snapshot all data for undo
            let colar = router.columnar.read();
            let row_count = colar.row_count(table);
            let meta = router.catalog().get_table(table);
            let mut all_data: Vec<Vec<Option<Vec<u8>>>> = Vec::new();
            if let Some(ref meta) = meta {
                for ri in 0..row_count {
                    let row: Vec<_> = meta.column_names.iter().map(|cn| {
                        colar.get_column(table, cn).ok().and_then(|c| c.get(ri).cloned()).flatten()
                    }).collect();
                    all_data.push(row);
                }
            }
            drop(colar);
            router.undo_stack.write().push(UndoAction::Truncate { table: table.clone(), data: all_data });
            router.columnar.write().truncate_table(table);
            Ok(ExecuteResult::Truncate(table.clone()))
        }
        Statement::DropTable { table, if_exists } => {
            if router.catalog().get_table(table).is_none() && *if_exists {
                return Ok(ExecuteResult::DropTable(table.clone()));
            }
            router.catalog().drop_table(table)?;
            router.columnar.write().drop_table(table);
            // Remove views referencing this table
            router.views.write().retain(|_, sql| !sql.to_uppercase().contains(&format!("FROM {}", table.to_uppercase())));
            Ok(ExecuteResult::DropTable(table.clone()))
        }
        Statement::DropIndex { name, if_exists } => {
            let all_indexes = router.indexes.list_all();
            if !all_indexes.iter().any(|m| m.name == *name) && *if_exists {
                return Ok(ExecuteResult::DropIndex(name.clone()));
            }
            router.indexes.drop_index(name)?;
            Ok(ExecuteResult::DropIndex(name.clone()))
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
    Truncate(String),
    AlterTable(String),
    DropTable(String),
    DropIndex(String),
}

impl ExecuteResult {
    pub fn to_pgwire_response(&self) -> (Vec<String>, Vec<Vec<String>>) {
        match self {
            Self::CreateTable(name) => (vec!["result".into()], vec![vec![format!("CREATE TABLE {}", name)]]),
            Self::Insert(count) => (vec!["rows".into()], vec![vec![count.to_string()]]),
            Self::Select { columns, rows } => (columns.clone(), rows.clone()),
            Self::Update(count) => (vec!["rows".into()], vec![vec![format!("UPDATE {}", count)]]),
            Self::Delete(count) => (vec!["rows".into()], vec![vec![format!("DELETE {}", count)]]),
            Self::Truncate(table) => (vec!["result".into()], vec![vec![format!("TRUNCATE TABLE {}", table)]]),
            Self::AlterTable(table) => (vec!["result".into()], vec![vec![format!("ALTER TABLE {}", table)]]),
            Self::DropTable(table) => (vec!["result".into()], vec![vec![format!("DROP TABLE {}", table)]]),
            Self::DropIndex(name) => (vec!["result".into()], vec![vec![format!("DROP INDEX {}", name)]]),
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

/// Apply HAVING filter to aggregated rows.
fn apply_having(cols: &[String], rows: &mut Vec<Vec<String>>, having_clause: &WhereClause) {
    let col_idx = cols.iter().position(|c| c == having_clause.column());
    rows.retain(|row| {
        if let Some(idx) = col_idx {
            if idx < row.len() {
                return having_clause.matches(&row[idx]);
            }
        }
        true
    });
}

/// Remove duplicate rows (keeps first occurrence).
fn dedup_rows(rows: &mut Vec<Vec<String>>) {
    let mut seen = std::collections::HashSet::new();
    rows.retain(|row| seen.insert(row.clone()));
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
            column_defs: vec!["id INTEGER".into(), "name TEXT".into()],
            engine: StorageEngine::Auto,
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
                assert_eq!(order_by[0].0, "id");
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

fn parse_truncate(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("TRUNCATE").unwrap_or(input).trim();
    let rest = rest.strip_prefix("TABLE").unwrap_or(rest).trim();
    let table = rest.trim_matches('"').trim_end_matches(';').to_string();
    Ok(Statement::Truncate { table })
}

fn parse_alter_table(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("ALTER TABLE").unwrap_or(input).trim();
    let (table, rest) = rest.split_once(' ').ok_or("Expected table name")?;
    let table = table.trim().trim_matches('"').to_string();
    let upper = rest.trim().to_uppercase();

    if upper.starts_with("ADD COLUMN") || upper.starts_with("ADD") {
        let def = if let Some(d) = upper.strip_prefix("ADD COLUMN") { d.trim() } else { upper.strip_prefix("ADD").unwrap_or("").trim() };
        let original_def = rest.trim();
        let def_pos = rest.trim().find(|c: char| c.is_alphabetic()).unwrap_or(0);
        let def_text = &rest.trim()[def_pos..].trim();
        let parts: Vec<_> = def_text.split_whitespace().collect();
        if parts.len() >= 2 {
            let name = parts[0].trim_matches('"').to_string();
            let col_type = ColumnType::from_str(parts[1]).unwrap_or(ColumnType::Text);
            return Ok(Statement::AlterTable {
                table,
                action: AlterAction::AddColumn { name, col_type, column_def: def_text.to_string() },
            });
        }
        return Err("Expected ADD COLUMN column_name TYPE".into());
    } else if upper.starts_with("DROP COLUMN") || upper.starts_with("DROP") {
        let col = if let Some(c) = upper.strip_prefix("DROP COLUMN") { c.trim() } else { upper.strip_prefix("DROP").unwrap_or("").trim() };
        let name = col.trim_matches('"').trim_end_matches(';').to_string();
        return Ok(Statement::AlterTable {
            table,
            action: AlterAction::DropColumn { name },
        });
    } else if upper.starts_with("RENAME TO") {
        let new_name = upper.strip_prefix("RENAME TO").unwrap_or("").trim().trim_matches('"').trim_end_matches(';').to_string();
        return Ok(Statement::AlterTable {
            table,
            action: AlterAction::RenameTable { new_name },
        });
    } else if upper.starts_with("RENAME COLUMN") {
        let rest = upper.strip_prefix("RENAME COLUMN").unwrap_or("").trim();
        let parts: Vec<_> = rest.split_whitespace().collect();
        if parts.len() >= 3 && parts[1].to_uppercase() == "TO" {
            return Ok(Statement::AlterTable {
                table,
                action: AlterAction::RenameColumn {
                    old_name: parts[0].trim_matches('"').to_string(),
                    new_name: parts[2].trim_matches('"').trim_end_matches(';').to_string(),
                },
            });
        }
    }
    Err(format!("Unsupported ALTER TABLE: {}", &upper[..upper.len().min(40)]))
}

fn parse_drop_table(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("DROP TABLE").unwrap_or(input).trim();
    let (rest, if_exists) = if rest.to_uppercase().starts_with("IF EXISTS") {
        (rest.strip_prefix("IF EXISTS").unwrap_or(rest).trim(), true)
    } else {
        (rest, false)
    };
    let table = rest.trim_matches('"').trim_end_matches(';').to_string();
    Ok(Statement::DropTable { table, if_exists })
}

fn parse_drop_index(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("DROP INDEX").unwrap_or(input).trim();
    let (rest, if_exists) = if rest.to_uppercase().starts_with("IF EXISTS") {
        (rest.strip_prefix("IF EXISTS").unwrap_or(rest).trim(), true)
    } else {
        (rest, false)
    };
    let name = rest.trim_matches('"').trim_end_matches(';').to_string();
    Ok(Statement::DropIndex { name, if_exists })
}

fn parse_where(input: &str) -> Option<WhereClause> {
    parse_where_clause(input)
}

fn parse_where_clause(input: &str) -> Option<WhereClause> {
    let trimmed = input.trim();
    let upper = trimmed.to_uppercase();

    // IS NULL / IS NOT NULL
    if let Some(pos) = upper.find("IS NULL") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::IsNull { column: col, not: false } });
    }
    if let Some(pos) = upper.find("IS NOT NULL") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::IsNull { column: col, not: true } });
    }

    // LIKE / NOT LIKE
    if let Some(pos) = upper.find("NOT LIKE") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let pattern = trimmed[pos + 8..].trim().trim_matches('\'').trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::Like { column: col, pattern, not: true } });
    }
    if let Some(pos) = upper.find("LIKE") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let pattern = trimmed[pos + 4..].trim().trim_matches('\'').trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::Like { column: col, pattern, not: false } });
    }

    // IN (...) / NOT IN (...)
    if let Some(pos) = upper.find("NOT IN") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let rest = trimmed[pos + 6..].trim();
        if let Some(values) = parse_in_values(rest) {
            return Some(WhereClause { predicate: WherePredicate::InList { column: col, values, not: true } });
        }
    }
    if let Some(pos) = upper.find("IN (") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let rest = trimmed[pos + 2..].trim();
        if let Some(values) = parse_in_values(rest) {
            return Some(WhereClause { predicate: WherePredicate::InList { column: col, values, not: false } });
        }
    }

    // BETWEEN / NOT BETWEEN
    if let Some(pos) = upper.find("NOT BETWEEN") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let rest = trimmed[pos + 11..].trim();
        let parts: Vec<_> = rest.split_whitespace().collect();
        if parts.len() >= 3 && parts[1].to_uppercase() == "AND" {
            return Some(WhereClause { predicate: WherePredicate::Between {
                column: col,
                low: parse_literal(parts[0].trim_matches('\'')),
                high: parse_literal(parts[2].trim_matches('\'')),
                not: true,
            }});
        }
    }
    if let Some(pos) = upper.find("BETWEEN") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let rest = trimmed[pos + 7..].trim();
        let parts: Vec<_> = rest.split_whitespace().collect();
        if parts.len() >= 3 && parts[1].to_uppercase() == "AND" {
            return Some(WhereClause { predicate: WherePredicate::Between {
                column: col,
                low: parse_literal(parts[0].trim_matches('\'')),
                high: parse_literal(parts[2].trim_matches('\'')),
                not: false,
            }});
        }
    }

    // Standard comparison: col OP value
    let parts: Vec<_> = trimmed.split_whitespace().collect();
    if parts.len() >= 3 {
        let col = parts[0].to_string();
        let op = match parts[1] { "=" => CompareOp::Eq, "!=" | "<>" => CompareOp::Neq, ">" => CompareOp::Gt, "<" => CompareOp::Lt, ">=" => CompareOp::Gte, "<=" => CompareOp::Lte, _ => CompareOp::Eq };
        let val = parse_literal(parts[2].trim_matches('\'').trim_matches('"'));
        return Some(WhereClause { predicate: WherePredicate::Compare { column: col, op, value: val } });
    }
    None
}

fn parse_in_values(input: &str) -> Option<Vec<Literal>> {
    let s = input.trim().trim_start_matches('(').trim_end_matches(')').trim_end_matches(';');
    let values: Vec<Literal> = s.split(',').map(|v| parse_literal(v.trim().trim_matches('\'').trim_matches('"'))).collect();
    if values.is_empty() { None } else { Some(values) }
}
