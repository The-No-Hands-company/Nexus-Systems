//! SQL Parser + Query Executor.
//!
//! Parses a subset of SQL and executes against the Delta-Main router.
//! Supported: CREATE TABLE, INSERT, SELECT (WHERE, ORDER BY, LIMIT).

use std::collections::BTreeMap;

use crate::catalog::{ColumnType, DeltaMainRouter, StorageEngine, UndoAction};
use crate::wal::{WalEntryType, WriteAheadLog};
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
        returning: Option<Vec<SelectColumn>>,
        on_conflict: Option<OnConflict>,
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
        cascade: bool,
    },
    DropIndex {
        name: String,
        if_exists: bool,
    },
    CreateIndex {
        name: Option<String>,
        table: String,
        columns: Vec<String>,
        unique: bool,
    },
    Union {
        left: Box<Statement>,
        right: Box<Statement>,
        all: bool,
    },
    Intersect {
        left: Box<Statement>,
        right: Box<Statement>,
    },
    Except {
        left: Box<Statement>,
        right: Box<Statement>,
    },
    DropView {
        name: String,
    },
    CreateTableAs {
        name: String,
        query: Box<Statement>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub struct OnConflict {
    pub column: String,
    pub updates: Vec<(String, Literal)>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum AlterAction {
    AddColumn { name: String, col_type: ColumnType, column_def: String },
    DropColumn { name: String },
    RenameTable { new_name: String },
    RenameColumn { old_name: String, new_name: String },
    AlterColumnType { name: String, new_type: ColumnType },
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
    CaseExpr { cases: Vec<CaseWhen>, else_result: Option<String>, alias: Option<String> },
    Coalesce { columns: Vec<String>, alias: Option<String> },
    FnCall { func: String, column: String, alias: Option<String> },
    WindowFn { func: String, over_order: Vec<(String, OrderDir)>, over_partition: Option<String>, alias: Option<String> },
}

#[derive(Debug, Clone, PartialEq)]
pub struct CaseWhen {
    pub when_col: String,
    pub when_op: CompareOp,
    pub when_val: String,
    pub then_val: String,
}

#[derive(Debug, Clone, PartialEq)]
pub struct WhereClause {
    pub predicate: WherePredicate,
}

#[derive(Debug, Clone, PartialEq)]
pub enum WherePredicate {
    Compare { column: String, op: CompareOp, value: Literal },
    IsNull { column: String, not: bool },
    Like { column: String, pattern: String, not: bool, case_insensitive: bool },
    InList { column: String, values: Vec<Literal>, not: bool },
    Between { column: String, low: Literal, high: Literal, not: bool },
    Subquery { column: String, op: CompareOp, subquery: Box<Statement> },
    Exists { subquery: Box<Statement>, not: bool },
    InSubquery { column: String, subquery: Box<Statement>, not: bool },
    IsDistinct { column: String, value: Literal, not: bool },
    And(Vec<WhereClause>),
    Or(Vec<WhereClause>),
    Not(Box<WhereClause>),
}

impl WhereClause {
    pub fn column(&self) -> &str {
        match &self.predicate {
            WherePredicate::Compare { column, .. } | WherePredicate::IsNull { column, .. } | WherePredicate::Like { column, .. } | WherePredicate::InList { column, .. } | WherePredicate::Between { column, .. } | WherePredicate::Subquery { column, .. } | WherePredicate::InSubquery { column, .. } | WherePredicate::IsDistinct { column, .. } => column,
            WherePredicate::And(clauses) => clauses.first().map(|c| c.column()).unwrap_or(""),
            WherePredicate::Or(clauses) => clauses.first().map(|c| c.column()).unwrap_or(""),
            WherePredicate::Not(inner) => inner.column(),
            WherePredicate::Exists { .. } => "__exists__",
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
            WherePredicate::Like { pattern, not, case_insensitive, .. } => {
                let matched = if *case_insensitive {
                    like_match(&row_val.to_lowercase(), &pattern.to_lowercase())
                } else {
                    like_match(row_val, pattern)
                };
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
            WherePredicate::And(clauses) => clauses.iter().all(|c| c.matches(row_val)),
            WherePredicate::Or(clauses) => clauses.iter().any(|c| c.matches(row_val)),
            WherePredicate::Not(inner) => !inner.matches(row_val),
            WherePredicate::IsDistinct { value, not, .. } => {
                let either_null = row_val == "NULL" || row_val.is_empty();
                let val_is_null = match value { Literal::Null => true, _ => false };
                if either_null || val_is_null {
                    let distinct = either_null != val_is_null;
                    if *not { !distinct } else { distinct }
                } else {
                    let lit = Literal::Text(row_val.to_string());
                    let eq = lit.compare(value, &CompareOp::Eq);
                    if *not { eq } else { !eq }
                }
            }
            WherePredicate::Subquery { .. } | WherePredicate::Exists { .. } | WherePredicate::InSubquery { .. } => {
                true // Handled externally via router
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
        if let WherePredicate::And(clauses) = &self.predicate {
            return clauses.iter().all(|c| c.matches_bytes(val, col_type));
        }
        if let WherePredicate::Or(clauses) = &self.predicate {
            return clauses.iter().any(|c| c.matches_bytes(val, col_type));
        }
        if let WherePredicate::Not(inner) = &self.predicate {
            return !inner.matches_bytes(val, col_type);
        }
        let s = val.as_ref().map(|b| String::from_utf8_lossy(b).to_string()).unwrap_or("NULL".into());
        self.matches(&s)
    }

    /// Get the lookup key bytes for index searches (for Compare Eq only).
    pub fn lookup_value_bytes(&self) -> Vec<u8> {
        if let WherePredicate::Compare { value, .. } = &self.predicate {
            value.to_bytes()
        } else {
            vec![]
        }
    }

    /// Check if this is a comparison predicate.
    pub fn is_compare(&self) -> bool {
        matches!(&self.predicate, WherePredicate::Compare { .. })
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
    Timestamp(i64),
}

impl Literal {
    fn to_bytes(&self) -> Vec<u8> {
        match self {
            Self::Text(s) => s.clone().into_bytes(),
            Self::Integer(i) => i.to_string().into_bytes(),
            Self::Float(f) => f.to_string().into_bytes(),
            Self::Boolean(b) => b.to_string().into_bytes(),
            Self::Null => vec![],
            Self::Timestamp(ts) => ts.to_string().into_bytes(),
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
            ColumnType::Timestamp => {
                let s = String::from_utf8_lossy(bytes);
                Self::Timestamp(s.parse().unwrap_or(0))
            }
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
            (Self::Timestamp(a), Self::Timestamp(b)) => a.cmp(b),
            (Self::Text(a), Self::Timestamp(b)) => a.parse::<i64>().unwrap_or(0).cmp(b),
            (Self::Timestamp(a), Self::Text(b)) => a.cmp(&b.parse::<i64>().unwrap_or(0)),
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
        // Check for CREATE TABLE ... AS SELECT
        if let Some(as_pos) = upper.find(" AS SELECT") {
            let table_name = trimmed[12..as_pos].trim().trim_matches('"').to_string();
            let select_sql = &trimmed[as_pos + 4..].trim();
            let query = Box::new(parse_select(select_sql)?);
            return Ok(Statement::CreateTableAs { name: table_name, query });
        }
        parse_create_table(trimmed)
    } else if upper.starts_with("CREATE INDEX") || upper.starts_with("CREATE UNIQUE INDEX") {
        parse_create_index(trimmed)
    } else if upper.starts_with("INSERT INTO") {
        parse_insert(trimmed)
    } else if upper.starts_with("UPDATE") {
        parse_update(trimmed)
    } else if upper.starts_with("DELETE") {
        parse_delete(trimmed)
    } else if upper.starts_with("SELECT") {
        // Check for UNION/INTERSECT/EXCEPT
        if let Some(pos) = upper.find(" UNION ") {
            let (left, right, all) = parse_set_op(trimmed, pos, "UNION")?;
            return Ok(Statement::Union { left, right, all });
        }
        if let Some(pos) = upper.find(" INTERSECT ") {
            let (left, right, _) = parse_set_op(trimmed, pos, "INTERSECT")?;
            return Ok(Statement::Intersect { left, right });
        }
        if let Some(pos) = upper.find(" EXCEPT ") {
            let (left, right, _) = parse_set_op(trimmed, pos, "EXCEPT")?;
            return Ok(Statement::Except { left, right });
        }
        parse_select(trimmed)
    } else if upper.starts_with("TRUNCATE") {
        parse_truncate(trimmed)
    } else if upper.starts_with("ALTER TABLE") {
        parse_alter_table(trimmed)
    } else if upper.starts_with("DROP TABLE") {
        parse_drop_table(trimmed)
    } else if upper.starts_with("DROP VIEW") {
        parse_drop_view(trimmed)
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

    // Parse ON CONFLICT clause
    let on_conflict = if let Some(pos) = input.to_uppercase().find("ON CONFLICT") {
        let rest = input[pos + 11..].trim();
        if let Some(paren_open) = rest.find('(') {
            let col = rest[paren_open + 1..].split(')').next().unwrap_or("").trim().to_string();
            let after_paren = &rest[paren_open + col.len() + 2..].trim();
            let updates = if let Some(do_pos) = after_paren.to_uppercase().find("DO UPDATE SET") {
                let set_part = &after_paren[do_pos + 13..].trim();
                parse_set_pairs(set_part)
            } else { Vec::new() };
            Some(OnConflict { column: col, updates })
        } else { None }
    } else { None };

    // Parse RETURNING clause
    let returning = if let Some(pos) = input.to_uppercase().find("RETURNING") {
        let ret = input[pos + 9..].trim().trim_end_matches(';');
        if ret.trim() == "*" {
            Some(vec![SelectColumn::All])
        } else {
            Some(ret.split(',').map(|c| SelectColumn::Named(c.trim().trim_matches('"').to_string())).collect())
        }
    } else { None };

    Ok(Statement::Insert { table: name, columns, values, returning, on_conflict })
}

fn parse_set_pairs(s: &str) -> Vec<(String, Literal)> {
    let s = s.trim().trim_end_matches(';');
    s.split(',').filter_map(|pair| {
        let parts: Vec<_> = pair.trim().splitn(2, '=').collect();
        if parts.len() == 2 {
            Some((parts[0].trim().to_string(), parse_literal(parts[1].trim())))
        } else { None }
    }).collect()
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
        } else if let Some(case) = parse_case_expr(col_part) {
            columns.push(SelectColumn::CaseExpr { cases: case.0, else_result: case.1, alias: case.2 });
        } else if let Some(coalesce) = parse_coalesce(col_part) {
            columns.push(SelectColumn::Coalesce { columns: coalesce.0, alias: coalesce.1 });
        } else if let Some(fncall) = parse_fn_call(col_part) {
            columns.push(SelectColumn::FnCall { func: fncall.0, column: fncall.1, alias: fncall.2 });
        } else if let Some(winfn) = parse_window_fn(col_part) {
            columns.push(SelectColumn::WindowFn { func: winfn.0, over_order: winfn.1, over_partition: winfn.2, alias: winfn.3 });
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

fn parse_case_expr(s: &str) -> Option<(Vec<CaseWhen>, Option<String>, Option<String>)> {
    // CASE WHEN col = 'val' THEN 'result' ELSE 'default' END [AS alias]
    let upper = s.to_uppercase().trim().to_string();
    if !upper.starts_with("CASE") { return None; }
    let end_pos = upper.find("END")?;
    let body = &s[4..end_pos].trim(); // skip "CASE"
    let after = s[end_pos + 3..].trim(); // after END

    let alias = if let Some(as_pos) = after.to_uppercase().find(" AS ") {
        Some(after[as_pos + 4..].trim().to_string())
    } else { None };

    let mut cases = Vec::new();
    let mut else_result: Option<String> = None;

    // Split into WHEN ... THEN ... ELSE segments
    let mut remaining: &str = body;
    while let Some(when_pos) = remaining.to_uppercase().find("WHEN") {
        let when_slice: &str = &remaining[when_pos + 4..];
        let when_part = when_slice.trim();
        if let Some(then_pos) = when_part.to_uppercase().find("THEN") {
            let cond = when_part[..then_pos].trim();
            let rest = when_part[then_pos + 4..].trim();

            // Parse condition: col OP 'value'
            let cond_parts: Vec<_> = cond.split_whitespace().collect();
            if cond_parts.len() >= 3 {
                let when_col = cond_parts[0].to_string();
                let when_op = match cond_parts[1] { "=" => CompareOp::Eq, "!=" | "<>" => CompareOp::Neq, ">" => CompareOp::Gt, "<" => CompareOp::Lt, _ => CompareOp::Eq };
                let when_val = cond_parts[2].trim_matches('\'').trim_matches('"').to_string();

                // Extract then_val (up to next WHEN or ELSE or END)
                let then_val = if let Some(next) = rest.to_uppercase().find("WHEN") {
                    rest[..next].trim().trim_matches('\'').trim_matches('"').to_string()
                } else if let Some(next) = rest.to_uppercase().find("ELSE") {
                    rest[..next].trim().trim_matches('\'').trim_matches('"').to_string()
                } else {
                    rest.trim().trim_matches('\'').trim_matches('"').to_string()
                };

                cases.push(CaseWhen { when_col, when_op, when_val, then_val });

                // Advance remaining
                remaining = if let Some(next) = rest.to_uppercase().find("WHEN") {
                    &rest[next..]
                } else {
                    ""
                };
            } else { break; }
        } else { break; }
    }

    // Parse ELSE
    if let Some(else_pos) = remaining.to_uppercase().find("ELSE") {
        let else_val = remaining[else_pos + 4..].trim().trim_matches('\'').trim_matches('"').to_string();
        else_result = Some(else_val);
    }

    Some((cases, else_result, alias))
}

fn parse_coalesce(s: &str) -> Option<(Vec<String>, Option<String>)> {
    // COALESCE(col1, col2, 'default') [AS alias]
    let upper = s.to_uppercase().trim().to_string();
    if !upper.starts_with("COALESCE(") { return None; }
    let inside = &s[9..].trim();
    let paren_end = inside.find(')')?;
    let args = inside[..paren_end].trim();

    let alias = if let Some(as_pos) = inside[paren_end + 1..].to_uppercase().find(" AS ") {
        Some(inside[paren_end + 1 + as_pos + 4..].trim().to_string())
    } else { None };

    let columns: Vec<String> = args.split(',').map(|c| c.trim().trim_matches('\'').trim_matches('"').to_string()).collect();
    Some((columns, alias))
}

fn parse_fn_call(s: &str) -> Option<(String, String, Option<String>)> {
    // UPPER(col), LOWER(col), LENGTH(col) [AS alias]
    let upper = s.to_uppercase().trim().to_string();
    if let Some(paren_pos) = upper.find('(') {
        let func = upper[..paren_pos].trim().to_string();
        let inside = &s[paren_pos + 1..].trim();
        let end = inside.find(')')?;
        let col = inside[..end].trim().trim_matches('\'').trim_matches('"').to_string();

        let alias = if let Some(as_pos) = inside[end + 1..].to_uppercase().find(" AS ") {
            Some(inside[end + 1 + as_pos + 4..].trim().to_string())
        } else { None };

        match func.as_str() {
            "UPPER" | "LOWER" | "LENGTH" | "SUBSTRING" => Some((func, col, alias)),
            _ => None,
        }
    } else { None }
}

fn parse_window_fn(s: &str) -> Option<(String, Vec<(String, OrderDir)>, Option<String>, Option<String>)> {
    // ROW_NUMBER() OVER (ORDER BY col), RANK() OVER (...)
    let upper = s.to_uppercase().trim().to_string();
    let over_pos = upper.find("OVER")?;
    let func_part = &s[..over_pos].trim();
    let (func, _) = func_part.split_once('(')?;
    let func = func.trim();
    if !matches!(func.to_uppercase().as_str(), "ROW_NUMBER" | "RANK" | "DENSE_RANK") {
        return None;
    }

    let over_rest = &s[over_pos + 4..].trim();
    let paren_open = over_rest.find('(')?;
    let paren_close = over_rest[paren_open..].find(')')?;
    let over_body = &over_rest[paren_open + 1..paren_open + paren_close].trim();

    // Parse PARTITION BY and ORDER BY inside OVER
    let mut over_order: Vec<(String, OrderDir)> = Vec::new();
    let mut over_partition: Option<String> = None;

    let upper_over = over_body.to_uppercase();
    if let Some(pb_pos) = upper_over.find("PARTITION BY") {
        let pb_rest = &over_body[pb_pos + 12..].trim();
        let part_col = pb_rest.split_whitespace().next().unwrap_or("").trim_matches('"').to_string();
        over_partition = Some(part_col);
    }

    if let Some(ob_pos) = upper_over.find("ORDER BY") {
        let ob_rest = &over_body[ob_pos + 8..].trim();
        for pair in ob_rest.split(',') {
            let parts: Vec<_> = pair.trim().split_whitespace().collect();
            if !parts.is_empty() {
                let col = parts[0].trim_matches('"').to_string();
                let dir = if parts.len() > 1 && parts[1].to_uppercase() == "DESC" { OrderDir::Desc } else { OrderDir::Asc };
                over_order.push((col, dir));
            }
        }
    }

    // Parse alias after OVER (...)
    let after_over = over_rest[paren_open + paren_close + 1..].trim();
    let alias = if let Some(as_pos) = after_over.to_uppercase().find(" AS ") {
        Some(after_over[as_pos + 4..].trim().to_string())
    } else { None };

    Some((func.to_string(), over_order, over_partition, alias))
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
        Statement::Insert { table, ref columns, values, returning, on_conflict } => {
            let meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;

            let col_indices: Vec<usize> = if let Some(cols) = columns {
                cols.iter().map(|c| meta.column_names.iter().position(|n| n == c).unwrap_or(0)).collect()
            } else {
                (0..meta.column_names.len()).collect()
            };

            let mut last_row: Option<Vec<Option<Vec<u8>>>> = None;
            let mut count = 0;
            for row_vals in values {
                let mut col_data: Vec<Option<Vec<u8>>> = vec![None; meta.column_names.len()];
                for (i, val) in row_vals.iter().enumerate() {
                    let target = col_indices.get(i).copied().unwrap_or(i);
                    if target < col_data.len() {
                        col_data[target] = Some(val.to_bytes());
                    }
                }

                // Check ON CONFLICT — find existing row with matching unique value
                if let Some(ref oc) = on_conflict {
                    if let Some(conflict_idx) = meta.column_names.iter().position(|n| n == &oc.column) {
                        let conflict_val = col_data.get(conflict_idx).and_then(|v| v.as_ref());
                        if let Some(ref cv) = conflict_val {
                            let colar = router.columnar.read();
                            if let Ok(col_data_all) = colar.get_column(table, &oc.column) {
                                if let Some(existing_idx) = col_data_all.iter().position(|v| v.as_deref() == Some(cv.as_slice())) {
                                    drop(colar);
                                    // UPDATE existing row
                                    for (upd_col, upd_val) in &oc.updates {
                                        let bytes = upd_val.to_bytes();
                                        let _ = router.columnar.write().update_row(table, upd_col, existing_idx, Some(bytes.clone()));
                                    }
                                    last_row = Some(col_data); // approximate
                                    count += 1;
                                    continue;
                                }
                            }
                            drop(colar);
                        }
                    }
                }

                last_row = Some(col_data.clone());
                let row = Row::new(col_data.clone());
                let row_idx = router.columnar.read().row_count(table);
                router.insert(table, &row)?;
                router.columnar.write().append_row(table, &col_data, &meta.column_types)?;
                // Index the new row
                let pk = row_idx.to_le_bytes().to_vec();
                let _ = router.indexes.index_row(table, &pk, &row, &meta);
                // WAL: log insert for crash recovery
                let mut wal_data = Vec::new();
                wal_data.extend_from_slice(&(table.len() as u16).to_le_bytes());
                wal_data.extend_from_slice(table.as_bytes());
                wal_data.extend_from_slice(&(row_idx as u64).to_le_bytes());
                let row_bytes = row.to_bytes();
                wal_data.extend_from_slice(&(row_bytes.len() as u32).to_le_bytes());
                wal_data.extend_from_slice(&row_bytes);
                wal_log(router, WalEntryType::InsertRow, 0, &wal_data);
                router.undo_stack.write().push(UndoAction::Insert { table: table.clone(), row_idx });
                count += 1;
            }
            // Handle RETURNING
            if let Some(ret_cols) = returning {
                if let Some(row_data) = last_row {
                    let out_cols: Vec<String> = match ret_cols.first() {
                        Some(SelectColumn::All) | None => meta.column_names.clone(),
                        _ => ret_cols.iter().filter_map(|c| match c {
                            SelectColumn::Named(n) => Some(n.clone()),
                            _ => None,
                        }).collect(),
                    };
                    let out_row: Vec<String> = out_cols.iter().map(|cn| {
                        if let Some(idx) = meta.column_names.iter().position(|n| n == cn) {
                            row_data.get(idx).and_then(|v| v.as_ref())
                                .map(|b| format_value(b, &meta.column_types[idx]))
                                .unwrap_or_else(|| "NULL".into())
                        } else { "NULL".into() }
                    }).collect();
                    return Ok(ExecuteResult::Select { columns: out_cols, rows: vec![out_row] });
                }
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
                                let va = a.get(ci).map(|s| s.as_str()).unwrap_or("NULL");
                                let vb = b.get(ci).map(|s| s.as_str()).unwrap_or("NULL");
                                let cmp = if va == "NULL" || vb == "NULL" {
                                    match (va == "NULL", vb == "NULL") {
                                        (true, true) => std::cmp::Ordering::Equal,
                                        (true, false) => match dir { OrderDir::Asc => std::cmp::Ordering::Greater, OrderDir::Desc => std::cmp::Ordering::Less },
                                        (false, true) => match dir { OrderDir::Asc => std::cmp::Ordering::Less, OrderDir::Desc => std::cmp::Ordering::Greater },
                                        _ => std::cmp::Ordering::Equal,
                                    }
                                } else { va.cmp(vb) };
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
            let col_store = router.columnar.read();
            let row_count = col_store.row_count(table);

            // Check for index scan optimization
            let mut index_used = false;
            let all_rows: Vec<Row> = if let Some(wc) = where_clause {
                if wc.is_compare() && matches!(&wc.predicate, WherePredicate::Compare { op: CompareOp::Eq, .. }) {
                    if let Some(idx_meta) = router.indexes.find_best_index(table, wc.column()) {
                        if let Ok(pks) = router.indexes.lookup(&idx_meta.name, &wc.lookup_value_bytes()) {
                            index_used = !pks.is_empty();
                            let mut rows = Vec::new();
                            for pk_bytes in &pks {
                                if pk_bytes.len() == 8 {
                                    if let Ok(row_idx) = usize::try_from(u64::from_le_bytes(pk_bytes[..8].try_into().unwrap_or([0; 8]))) {
                                        if row_idx < row_count as usize {
                                            let mut col_values: Vec<Option<Vec<u8>>> = Vec::new();
                                            for col_name in &meta.column_names {
                                                let val = col_store.get_column(table, col_name)
                                                    .ok().and_then(|col| col.get(row_idx).cloned()).flatten();
                                                col_values.push(val);
                                            }
                                            rows.push(Row::new(col_values));
                                        }
                                    }
                                }
                            }
                            rows
                        } else {
                            Vec::new()
                        }
                    } else { Vec::new() }
                } else { Vec::new() }
            } else { Vec::new() };

            // Fallback: full scan if not using index
            let mut all_rows = if !all_rows.is_empty() {
                all_rows
            } else {
                let mut rows: Vec<Row> = Vec::new();
                for row_idx in 0..row_count {
                    let mut col_values: Vec<Option<Vec<u8>>> = Vec::new();
                    for col_name in &meta.column_names {
                        let val = col_store.get_column(table, col_name)
                            .ok()
                            .and_then(|col| col.get(row_idx).cloned())
                            .flatten();
                        col_values.push(val);
                    }
                    rows.push(Row::new(col_values));
                }
                rows
            };
            drop(col_store);

            // Filter with WHERE
            let has_subquery = where_clause.as_ref().map(|wc| 
                matches!(&wc.predicate, WherePredicate::Subquery { .. } | WherePredicate::Exists { .. } | WherePredicate::InSubquery { .. })
                || matches!(&wc.predicate, WherePredicate::And(_) | WherePredicate::Or(_) | WherePredicate::Not(_))
            ).unwrap_or(false);
            let filtered: Vec<Row> = if let Some(wc) = where_clause {
                let col_idx = meta.column_names.iter().position(|c| c == wc.column());
                if has_subquery {
                    all_rows.into_iter().filter(|row| {
                        if let Some(idx) = col_idx {
                            if let Some(val) = row.columns.get(idx) {
                                return evaluate_where(wc, val, &meta.column_types[idx], router);
                            }
                        }
                        false
                    }).collect()
                } else {
                    all_rows.into_iter().filter(|row| {
                        if let Some(idx) = col_idx {
                            if let Some(val) = row.columns.get(idx) {
                                return wc.matches_bytes(val, &meta.column_types[idx]);
                            }
                        }
                        false
                    }).collect()
                }
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
                            let cmp = match (va, vb) {
                                (None, None) => std::cmp::Ordering::Equal,
                                (None, Some(_)) => match dir { OrderDir::Asc => std::cmp::Ordering::Greater, OrderDir::Desc => std::cmp::Ordering::Less },
                                (Some(_), None) => match dir { OrderDir::Asc => std::cmp::Ordering::Less, OrderDir::Desc => std::cmp::Ordering::Greater },
                                (Some(a), Some(b)) => a.cmp(b),
                            };
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

            // Build result columns and rows with CASE/COALESCE support
            let select_cols: Vec<SelectColumn> = match columns.first() {
                Some(SelectColumn::All) | None => meta.column_names.iter()
                    .map(|n| SelectColumn::Named(n.clone())).collect(),
                _ => columns.clone(),
            };

            let result_cols: Vec<String> = select_cols.iter().map(|sc| match sc {
                SelectColumn::Named(n) => n.clone(),
                SelectColumn::CaseExpr { alias, .. } => alias.clone().unwrap_or_else(|| "case".into()),
                SelectColumn::Coalesce { alias, columns: cols } => alias.clone().unwrap_or_else(|| cols.first().cloned().unwrap_or_else(|| "coalesce".into())),
                SelectColumn::FnCall { func, column, alias } => alias.clone().unwrap_or_else(|| format!("{}({})", func, column)),
                SelectColumn::WindowFn { func, over_partition, alias, .. } => alias.clone().unwrap_or_else(|| format!("{}()", func)),
                SelectColumn::All => "?".into(),
            }).collect();

            // Compute window function values for each row (post-sort)
            let mut window_values: Vec<Vec<String>> = Vec::new();
            for wf_col in select_cols.iter() {
                if let SelectColumn::WindowFn { func, over_order, over_partition, .. } = wf_col {
                    if !over_order.is_empty() || over_partition.is_some() {
                        // Sort a copy for window ordering (if different from main ORDER BY)
                        let mut sorted_rows: Vec<&Row> = sorted.iter().collect();
                        sorted_rows.sort_by(|a, b| {
                            for (col, dir) in over_order {
                                if let Some(idx) = meta.column_names.iter().position(|c| c == col) {
                                    let va = a.columns.get(idx).and_then(|c| c.as_ref());
                                    let vb = b.columns.get(idx).and_then(|c| c.as_ref());
                                    let cmp = match (va, vb) {
                                        (None, None) => std::cmp::Ordering::Equal,
                                        (None, Some(_)) => match dir { OrderDir::Asc => std::cmp::Ordering::Greater, OrderDir::Desc => std::cmp::Ordering::Less },
                                        (Some(_), None) => match dir { OrderDir::Asc => std::cmp::Ordering::Less, OrderDir::Desc => std::cmp::Ordering::Greater },
                                        (Some(a), Some(b)) => a.cmp(b),
                                    };
                                    if cmp != std::cmp::Ordering::Equal {
                                        return match dir { OrderDir::Desc => cmp.reverse(), OrderDir::Asc => cmp };
                                    }
                                }
                            }
                            std::cmp::Ordering::Equal
                        });
                    } else {
                        // No explicit OVER ORDER BY → use built-in ORDER BY
                    }
                }
                window_values.push(Vec::new());
            }

            // Compute window functions
            for (wi, wf_col) in select_cols.iter().enumerate() {
                if let SelectColumn::WindowFn { func, over_order, .. } = wf_col {
                    let order_by_cols: Vec<(String, OrderDir)> = if !over_order.is_empty() { over_order.clone() } else { order_by.clone() };
                    let col_indices: Vec<usize> = order_by_cols.iter().filter_map(|(c, _)| meta.column_names.iter().position(|n| n == c)).collect();

                    let mut vals: Vec<String> = Vec::new();
                    let mut rank = 0u64;
                    let mut dense_rank = 0u64;
                    let mut prev_key: Option<Vec<Option<&[u8]>>> = None;

                    for (ri, row) in sorted.iter().enumerate() {
                        let current_key: Vec<Option<&[u8]>> = col_indices.iter().map(|&i| {
                            row.columns.get(i).and_then(|c: &Option<Vec<u8>>| c.as_deref())
                        }).collect();

                        match func.as_str() {
                            "ROW_NUMBER" => vals.push((ri + 1).to_string()),
                            "RANK" => {
                                if prev_key.as_ref() != Some(&current_key) {
                                    rank = ri as u64 + 1;
                                }
                                vals.push(rank.to_string());
                            }
                            "DENSE_RANK" => {
                                if prev_key.as_ref() != Some(&current_key) {
                                    dense_rank += 1;
                                }
                                vals.push(dense_rank.to_string());
                            }
                            _ => vals.push("0".into()),
                        }
                        prev_key = Some(current_key);
                    }
                    if wi < window_values.len() { window_values[wi] = vals; }
                }
            }

            let mut result_rows: Vec<Vec<String>> = sorted.iter().enumerate().map(|(ri, row)| {
                select_cols.iter().enumerate().map(|(si, sc)| {
                    match sc {
                        SelectColumn::WindowFn { .. } => window_values.get(si).and_then(|wv| wv.get(ri)).cloned().unwrap_or_else(|| "0".into()),
                        _ => evaluate_select_column(sc, row, &meta),
                    }
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
                AlterAction::AlterColumnType { name, new_type } => {
                    router.catalog().alter_column_type(table, name, new_type.clone())?;
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
            let has_sq = where_clause.as_ref().map(|wc| matches!(&wc.predicate, WherePredicate::Subquery { .. } | WherePredicate::Exists { .. } | WherePredicate::InSubquery { .. })).unwrap_or(false);
            let match_indices: Vec<usize> = if let Some(wc) = where_clause {
                let col_idx = meta.column_names.iter().position(|c| c == wc.column());
                all_rows.iter().filter(|(_, row)| {
                    if let Some(idx) = col_idx {
                        if let Some(val) = row.get(idx) {
                            if has_sq {
                                return evaluate_where(wc, val, &meta.column_types[idx], router);
                            }
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
                    // WAL log
                    let mut wal_data = Vec::new();
                    wal_data.extend_from_slice(&(table.len() as u16).to_le_bytes());
                    wal_data.extend_from_slice(table.as_bytes());
                    wal_data.extend_from_slice(&(row_idx as u64).to_le_bytes());
                    wal_data.extend_from_slice(&(col.len() as u16).to_le_bytes());
                    wal_data.extend_from_slice(col.as_bytes());
                    wal_data.extend_from_slice(&(bytes.len() as u32).to_le_bytes());
                    wal_data.extend_from_slice(&bytes);
                    wal_log(router, WalEntryType::UpdateRow, 0, &wal_data);
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
            let has_sq = where_clause.as_ref().map(|wc| matches!(&wc.predicate, WherePredicate::Subquery { .. } | WherePredicate::Exists { .. } | WherePredicate::InSubquery { .. })).unwrap_or(false);
            let match_indices: Vec<usize> = if let Some(wc) = where_clause {
                let col_idx = meta.column_names.iter().position(|c| c == wc.column());
                all_rows.iter().filter(|(_, row)| {
                    if let Some(idx) = col_idx {
                        if let Some(val) = row.get(idx) {
                            if has_sq {
                                return evaluate_where(wc, val, &meta.column_types[idx], router);
                            }
                            return wc.matches_bytes(val, &meta.column_types[idx]);
                        }
                    }
                    false
                }).map(|(idx, _)| *idx).collect()
            } else {
                (0..row_count).collect()
            };

            let mut total_deleted = 0;
            // Delete matching rows (reverse order to keep indices valid)
            for &row_idx in match_indices.iter().rev() {
                // Cascade: delete referencing rows in child tables
                for (col_name, col_type) in meta.column_names.iter().zip(meta.column_types.iter()) {
                    if let Ok(col_data) = router.columnar.read().get_column(table, col_name) {
                        if let Some(Some(val)) = col_data.get(row_idx) {
                            let cascade_targets = router.foreign_keys.cascade_rows(table, col_name, val, &router.columnar.read());
                            for (child_table, child_ri) in cascade_targets {
                                let _ = router.columnar.write().delete_row(&child_table, child_ri);
                                total_deleted += 1;
                            }
                        }
                    }
                }
                total_deleted += 1;
                // Snapshot row data for undo
                let row_data: Vec<Option<Vec<u8>>> = meta.column_names.iter().map(|cn| {
                    router.columnar.read().get_column(table, cn)
                        .ok().and_then(|c| c.get(row_idx).cloned()).flatten()
                }).collect();
                let del_row = Row::new(row_data.clone());
                let _ = router.indexes.deindex_row(table, &del_row, &meta);
                // WAL log
                let mut wal_data = Vec::new();
                wal_data.extend_from_slice(&(table.len() as u16).to_le_bytes());
                wal_data.extend_from_slice(table.as_bytes());
                wal_data.extend_from_slice(&(row_idx as u64).to_le_bytes());
                wal_log(router, WalEntryType::DeleteRow, 0, &wal_data);
                router.undo_stack.write().push(UndoAction::Delete {
                    table: table.clone(), row_idx, data: row_data,
                });
                let _ = router.columnar.write().delete_row(table, row_idx);
            }

            Ok(ExecuteResult::Delete(total_deleted))
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
            // WAL log
            let mut wal_data = Vec::new();
            wal_data.extend_from_slice(&(table.len() as u16).to_le_bytes());
            wal_data.extend_from_slice(table.as_bytes());
            wal_log(router, WalEntryType::TruncateTable, 0, &wal_data);
            router.undo_stack.write().push(UndoAction::Truncate { table: table.clone(), data: all_data });
            router.columnar.write().truncate_table(table);
            Ok(ExecuteResult::Truncate(table.clone()))
        }
        Statement::DropTable { table, if_exists, cascade } => {
            if router.catalog().get_table(table).is_none() && *if_exists {
                return Ok(ExecuteResult::DropTable(table.clone()));
            }
            if *cascade {
                // Drop indexes on this table
                for idx_meta in router.indexes.list_indexes(table) {
                    let _ = router.indexes.drop_index(&idx_meta.name);
                }
                // Drop views referencing this table
                let upper_table = table.to_uppercase();
                router.views.write().retain(|_, sql| !sql.to_uppercase().contains(&format!("FROM {}", upper_table)));
            }
            router.catalog().drop_table(table)?;
            router.columnar.write().drop_table(table);
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
        Statement::CreateIndex { name, table, columns, unique } => {
            let idx_name = name.clone().unwrap_or_else(|| format!("{}_{}_idx", table, columns.join("_")));
            let table_meta = router.catalog().get_table(table)
                .ok_or_else(|| crate::EngineError::KeyNotFound(format!("Table {} not found", table)))?;
            router.indexes.create_index(&idx_name, table, columns.clone(), *unique, crate::index::IndexType::BTree, &table_meta)?;
            Ok(ExecuteResult::CreateTable(table.clone()))
        }
        Statement::Union { left, right, all } => {
            let l_result = execute(router, left)?;
            let r_result = execute(router, right)?;
            let (l_cols, l_rows) = l_result.to_pgwire_response();
            let (_r_cols, r_rows) = r_result.to_pgwire_response();
            let mut result_rows = l_rows;
            result_rows.extend(r_rows);
            if !*all {
                dedup_rows(&mut result_rows);
            }
            Ok(ExecuteResult::Select { columns: l_cols, rows: result_rows })
        }
        Statement::Intersect { left, right } => {
            let l_result = execute(router, left)?;
            let r_result = execute(router, right)?;
            let (l_cols, l_rows) = l_result.to_pgwire_response();
            let (_r_cols, r_rows) = r_result.to_pgwire_response();
            let mut result_rows = Vec::new();
            for lr in &l_rows {
                if r_rows.iter().any(|rr| rr == lr) {
                    result_rows.push(lr.clone());
                }
            }
            Ok(ExecuteResult::Select { columns: l_cols, rows: result_rows })
        }
        Statement::Except { left, right } => {
            let l_result = execute(router, left)?;
            let r_result = execute(router, right)?;
            let (l_cols, l_rows) = l_result.to_pgwire_response();
            let (_r_cols, r_rows) = r_result.to_pgwire_response();
            let mut result_rows = Vec::new();
            for lr in &l_rows {
                if !r_rows.iter().any(|rr| rr == lr) {
                    result_rows.push(lr.clone());
                }
            }
            Ok(ExecuteResult::Select { columns: l_cols, rows: result_rows })
        }
        Statement::DropView { name } => {
            router.views.write().remove(name);
            Ok(ExecuteResult::DropTable(name.clone()))
        }
        Statement::CreateTableAs { name, query } => {
            let result = execute(router, query)?;
            let (cols, rows) = result.to_pgwire_response();
            let columns: Vec<(String, ColumnType)> = cols.iter().map(|c| (c.clone(), ColumnType::Text)).collect();
            router.catalog().create_table(name, StorageEngine::Auto, columns.clone())?;
            router.columnar.write().create_table(name, &columns);
            // Insert each row
            let meta = router.catalog().get_table(name).unwrap();
            for row_vals in &rows {
                let col_data: Vec<Option<Vec<u8>>> = row_vals.iter().map(|s| {
                    if s == "NULL" { None } else { Some(s.clone().into_bytes()) }
                }).collect();
                let row = Row::new(col_data.clone());
                router.insert(name, &row)?;
                router.columnar.write().append_row(name, &col_data, &meta.column_types)?;
            }
            Ok(ExecuteResult::CreateTable(name.clone()))
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

/// Evaluate a WhereClause against a value, supporting subqueries.
fn evaluate_where(wc: &WhereClause, val: &Option<Vec<u8>>, col_type: &ColumnType, router: &DeltaMainRouter) -> bool {
    match &wc.predicate {
        WherePredicate::Subquery { .. } | WherePredicate::Exists { .. } | WherePredicate::InSubquery { .. } => {
            eval_subquery(router, &wc.predicate, val.as_deref(), col_type)
        }
        _ => wc.matches_bytes(val, col_type),
    }
}

fn eval_subquery(router: &DeltaMainRouter, predicate: &WherePredicate, val: Option<&[u8]>, col_type: &ColumnType) -> bool {
    match predicate {
        WherePredicate::Subquery { op, subquery, .. } => {
            if let Ok(result) = execute(router, subquery) {
                match result {
                    ExecuteResult::Select { rows, .. } => {
                        if let Some(first_row) = rows.first() {
                            if let Some(first_cell) = first_row.first() {
                                let lit = match val {
                                    Some(b) => Literal::from_bytes(b, col_type),
                                    None => Literal::Null,
                                };
                                let sq_val = Literal::Text(first_cell.clone());
                                return lit.compare(&sq_val, op);
                            }
                        }
                    }
                    _ => {}
                }
            }
            false
        }
        WherePredicate::Exists { subquery, not } => {
            if let Ok(result) = execute(router, subquery) {
                match result {
                    ExecuteResult::Select { rows, .. } => {
                        let exists = !rows.is_empty();
                        return if *not { !exists } else { exists };
                    }
                    _ => {}
                }
            }
            *not // not EXISTS with error → true
        }
        WherePredicate::InSubquery { subquery, not, .. } => {
            if let Ok(result) = execute(router, subquery) {
                match result {
                    ExecuteResult::Select { rows, .. } => {
                        let val_str = val.map(|b| String::from_utf8_lossy(b).to_string()).unwrap_or("NULL".into());
                        let found = rows.iter().any(|r| r.first().map(|c| c == &val_str).unwrap_or(false));
                        return if *not { !found } else { found };
                    }
                    _ => {}
                }
            }
            false
        }
        _ => true,
    }
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

/// Format a byte value for display according to column type.
fn format_value(bytes: &[u8], col_type: &ColumnType) -> String {
    match col_type {
        ColumnType::Timestamp => {
            let s = String::from_utf8_lossy(bytes);
            if let Ok(ts) = s.parse::<i64>() {
                let secs = ts % 60; let total_mins = ts / 60;
                let mins = total_mins % 60; let total_hours = total_mins / 60;
                let hours = total_hours % 24; let days = total_hours / 24;
                let year = 1970 + (days / 365) as i64;
                let doy = days % 365;
                let month = (doy / 30 + 1).min(12).max(1);
                let day = (doy % 30 + 1).min(28).max(1);
                format!("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", year, month, day, hours, mins, secs)
            } else {
                s.to_string()
            }
        }
        _ => String::from_utf8_lossy(bytes).to_string(),
    }
}

/// Evaluate a SelectColumn against a row.
fn evaluate_select_column(sc: &SelectColumn, row: &Row, meta: &crate::catalog::TableMeta) -> String {
    match sc {
        SelectColumn::All => "?".into(),
        SelectColumn::WindowFn { .. } => "0".into(),
        SelectColumn::Named(name) => {
            if let Some(idx) = meta.column_names.iter().position(|n| n == name) {
                row.columns.get(idx)
                    .and_then(|c| c.as_ref())
                    .map(|b| format_value(b, &meta.column_types[idx]))
                    .unwrap_or_else(|| "NULL".to_string())
            } else { "NULL".into() }
        }
        SelectColumn::CaseExpr { cases, else_result, .. } => {
            for cw in cases {
                if let Some(idx) = meta.column_names.iter().position(|n| n == &cw.when_col) {
                    if let Some(Some(val)) = row.columns.get(idx) {
                        let s = String::from_utf8_lossy(val).to_string();
                        let matches = match cw.when_op {
                            CompareOp::Eq => s == cw.when_val,
                            CompareOp::Neq => s != cw.when_val,
                            _ => s == cw.when_val,
                        };
                        if matches { return cw.then_val.clone(); }
                    }
                }
            }
            else_result.clone().unwrap_or_else(|| "NULL".into())
        }
        SelectColumn::Coalesce { columns, .. } => {
            for cn in columns {
                if let Some(idx) = meta.column_names.iter().position(|n| n == cn) {
                    if let Some(Some(val)) = row.columns.get(idx) {
                        let s = String::from_utf8_lossy(val).to_string();
                        if s != "NULL" && !s.is_empty() {
                            return format_value(val, &meta.column_types[idx]);
                        }
                    }
                }
            }
            "NULL".into()
        }
        SelectColumn::FnCall { func, column, .. } => {
            if let Some(idx) = meta.column_names.iter().position(|n| n == column) {
                if let Some(Some(val)) = row.columns.get(idx) {
                    let s = String::from_utf8_lossy(val).to_string();
                    return match func.as_str() {
                        "UPPER" => s.to_uppercase(),
                        "LOWER" => s.to_lowercase(),
                        "LENGTH" => s.len().to_string(),
                        "SUBSTRING" => s.chars().take(10).collect(), // simplified
                        _ => s,
                    };
                }
            }
            "NULL".into()
        }
    }
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
    let s = s.trim_matches('\'').trim_matches('"');
    // Try ISO 8601 timestamp
    if let Some(ts) = parse_timestamp(s) { return Literal::Timestamp(ts); }
    Literal::Text(s.to_string())
}

fn parse_timestamp(s: &str) -> Option<i64> {
    // Supports: 2024-01-15, 2024-01-15T14:30:00, 2024-01-15 14:30:00
    let s = s.trim();
    if s.len() < 10 { return None; }
    let year: i32 = s[0..4].parse().ok()?;
    if s.as_bytes().get(4) != Some(&b'-') { return None; }
    let month: i32 = s[5..7].parse().ok()?;
    if s.as_bytes().get(7) != Some(&b'-') { return None; }
    let day: i32 = s[8..10].parse().ok()?;
    if month < 1 || month > 12 || day < 1 || day > 31 { return None; }
    // Unix timestamp approximation (seconds since epoch)
    let days = (year as i64 - 1970) * 365 + (month as i64 - 1) * 30 + day as i64;
    let secs = if s.len() >= 19 && s.as_bytes().get(10) == Some(&b' ') {
        let h: i64 = s[11..13].parse().unwrap_or(0);
        let m: i64 = s[14..16].parse().unwrap_or(0);
        let sec: i64 = s[17..19].parse().unwrap_or(0);
        h * 3600 + m * 60 + sec
    } else { 0 };
    Some(days * 86400 + secs)
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

fn parse_set_op(input: &str, pos: usize, keyword: &str) -> std::result::Result<(Box<Statement>, Box<Statement>, bool), String> {
    let left_sql = &input[..pos].trim();
    let right_sql = &input[pos + keyword.len() + 2..].trim();
    let all = right_sql.to_uppercase().starts_with("ALL ");
    let right_sql = if all { &right_sql[4..] } else { right_sql };
    let left = Box::new(parse_select(left_sql)?);
    let right = Box::new(parse_select(right_sql)?);
    Ok((left, right, all))
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
    } else if upper.starts_with("ALTER COLUMN") {
        let rest = upper.strip_prefix("ALTER COLUMN").unwrap_or("").trim();
        let parts: Vec<_> = rest.split_whitespace().collect();
        if parts.len() >= 3 && parts[1].to_uppercase() == "TYPE" {
            let col_name = parts[0].trim_matches('"').to_string();
            let new_type = ColumnType::from_str(parts[2]).unwrap_or(ColumnType::Text);
            return Ok(Statement::AlterTable {
                table,
                action: AlterAction::AlterColumnType { name: col_name, new_type },
            });
        }
    }
    Err(format!("Unsupported ALTER TABLE: {}", &upper[..upper.len().min(40)]))
}

fn parse_drop_table(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("DROP TABLE").unwrap_or(input).trim();
    let (rest, if_exists) = if rest.to_uppercase().starts_with("IF EXISTS") {
        (rest.strip_prefix("IF EXISTS").unwrap_or(rest).trim(), true)
    } else { (rest, false) };
    let cascade = rest.to_uppercase().ends_with("CASCADE");
    let table = rest.trim_end_matches("CASCADE").trim().trim_matches('"').trim_end_matches(';').to_string();
    Ok(Statement::DropTable { table, if_exists, cascade })
}

fn parse_drop_view(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("DROP VIEW").unwrap_or(input).trim();
    let rest = rest.strip_prefix("IF EXISTS").unwrap_or(rest).trim();
    let name = rest.trim_matches('"').trim_end_matches(';').to_string();
    Ok(Statement::DropView { name })
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

fn parse_create_index(input: &str) -> std::result::Result<Statement, String> {
    let rest = input.strip_prefix("CREATE").unwrap_or(input).trim();
    let (rest, unique) = if rest.to_uppercase().starts_with("UNIQUE") {
        (rest.strip_prefix("UNIQUE").unwrap_or(rest).trim(), true)
    } else { (rest.strip_prefix("INDEX").unwrap_or(rest).trim(), false) };
    let rest = if !unique && rest.to_uppercase().starts_with("INDEX") {
        rest.strip_prefix("INDEX").unwrap_or(rest).trim()
    } else { rest.trim() };

    // Optional name: CREATE INDEX idx_name ON table (...)
    let (name, rest) = if let Some(on_pos) = rest.to_uppercase().find(" ON ") {
        let idx_name = rest[..on_pos].trim().trim_matches('"').to_string();
        let idx_name = if idx_name.is_empty() { None } else { Some(idx_name) };
        (idx_name, &rest[on_pos + 4..])
    } else {
        (None, rest)
    };

    let rest = rest.trim();
    if let Some(paren_open) = rest.find('(') {
        let table = rest[..paren_open].trim().trim_matches('"').to_string();
        let cols_str = rest[paren_open + 1..].split(')').next().unwrap_or("").trim();
        let columns: Vec<String> = cols_str.split(',').map(|c| c.trim().trim_matches('"').to_string()).collect();
        return Ok(Statement::CreateIndex { name, table, columns, unique });
    }
    Err("Expected CREATE INDEX ON table (col1, col2)".into())
}

/// Log a columnar mutation to the WAL for crash recovery.
fn wal_log(router: &DeltaMainRouter, entry_type: WalEntryType, page_id: u64, data: &[u8]) {
    if let Some(ref wal) = router.wal {
        let _ = wal.write().append(entry_type, page_id, data);
    }
}

fn parse_where(input: &str) -> Option<WhereClause> {
    parse_where_clause(input)
}

fn parse_where_clause(input: &str) -> Option<WhereClause> {
    let trimmed = input.trim();
    let upper = trimmed.to_uppercase();

    // NOT (condition)
    if upper.starts_with("NOT ") {
        let inner = &trimmed[4..].trim();
        if let Some(cond) = parse_where_clause(inner) {
            return Some(WhereClause { predicate: WherePredicate::Not(Box::new(cond)) });
        }
    }

    // AND / OR — split and combine
    let and_pos = find_top_level_keyword(upper.as_str(), "AND");
    let or_pos = find_top_level_keyword(upper.as_str(), "OR");
    if let Some(pos) = and_pos {
        let left = parse_where_clause(&trimmed[..pos].trim())?;
        let right = parse_where_clause(&trimmed[pos + 3..].trim())?;
        // Check if we already have an Or — if so, nest properly
        let combined = match &left.predicate {
            WherePredicate::And(existing) => {
                let mut clauses = existing.clone();
                clauses.push(right);
                WhereClause { predicate: WherePredicate::And(clauses) }
            }
            _ => WhereClause { predicate: WherePredicate::And(vec![left, right]) }
        };
        return Some(combined);
    }
    if let Some(pos) = or_pos {
        let left = parse_where_clause(&trimmed[..pos].trim())?;
        let right = parse_where_clause(&trimmed[pos + 2..].trim())?;
        let combined = match &left.predicate {
            WherePredicate::Or(existing) => {
                let mut clauses = existing.clone();
                clauses.push(right);
                WhereClause { predicate: WherePredicate::Or(clauses) }
            }
            _ => WhereClause { predicate: WherePredicate::Or(vec![left, right]) }
        };
        return Some(combined);
    }

    // IS [NOT] DISTINCT FROM value
    if let Some(pos) = upper.find("IS NOT DISTINCT FROM") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let rest = &trimmed[pos + 20..].trim();
        let val = parse_literal(rest.trim_matches('\'').trim_matches('"'));
        return Some(WhereClause { predicate: WherePredicate::IsDistinct { column: col, value: val, not: false } });
    }
    if let Some(pos) = upper.find("IS DISTINCT FROM") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let rest = &trimmed[pos + 15..].trim();
        let val = parse_literal(rest.trim_matches('\'').trim_matches('"'));
        return Some(WhereClause { predicate: WherePredicate::IsDistinct { column: col, value: val, not: true } });
    }

    // IS NULL / IS NOT NULL
    if let Some(pos) = upper.find("IS NULL") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::IsNull { column: col, not: false } });
    }
    if let Some(pos) = upper.find("IS NOT NULL") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::IsNull { column: col, not: true } });
    }

    // LIKE / ILIKE / NOT LIKE / NOT ILIKE
    if let Some(pos) = upper.find("NOT ILIKE") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let pattern = trimmed[pos + 9..].trim().trim_matches('\'').trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::Like { column: col, pattern, not: true, case_insensitive: true } });
    }
    if let Some(pos) = upper.find("NOT LIKE") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let pattern = trimmed[pos + 8..].trim().trim_matches('\'').trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::Like { column: col, pattern, not: true, case_insensitive: false } });
    }
    if let Some(pos) = upper.find("ILIKE") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let pattern = trimmed[pos + 5..].trim().trim_matches('\'').trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::Like { column: col, pattern, not: false, case_insensitive: true } });
    }
    if let Some(pos) = upper.find("LIKE") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let pattern = trimmed[pos + 4..].trim().trim_matches('\'').trim_matches('"').to_string();
        return Some(WhereClause { predicate: WherePredicate::Like { column: col, pattern, not: false, case_insensitive: false } });
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

    // EXISTS (SELECT ...) / NOT EXISTS (SELECT ...)
    if let Some(pos) = upper.find("EXISTS (") {
        let sq = &trimmed[pos + 7..].trim();
        if let Ok(stmt) = parse_sql(sq) {
            return Some(WhereClause { predicate: WherePredicate::Exists { subquery: Box::new(stmt), not: false } });
        }
    }
    if let Some(pos) = upper.find("NOT EXISTS (") {
        let sq = &trimmed[pos + 11..].trim();
        if let Ok(stmt) = parse_sql(sq) {
            return Some(WhereClause { predicate: WherePredicate::Exists { subquery: Box::new(stmt), not: true } });
        }
    }

    // col IN (SELECT ...) — detect subquery inside IN parentheses
    if let Some(pos) = upper.find("IN (SELECT") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let sq = &trimmed[pos + 2..].trim();
        let sq = sq.trim_start_matches('(').trim();
        if let Ok(stmt) = parse_sql(sq) {
            return Some(WhereClause { predicate: WherePredicate::InSubquery { column: col, subquery: Box::new(stmt), not: false } });
        }
    }
    if let Some(pos) = upper.find("NOT IN (SELECT") {
        let col = trimmed[..pos].trim().trim_matches('"').to_string();
        let sq = &trimmed[pos + 8..].trim();
        let sq = sq.trim_start_matches('(').trim();
        if let Ok(stmt) = parse_sql(sq) {
            return Some(WhereClause { predicate: WherePredicate::InSubquery { column: col, subquery: Box::new(stmt), not: true } });
        }
    }

    // col OP (SELECT ...) — scalar subquery
    let parts: Vec<_> = trimmed.split_whitespace().collect();
    if parts.len() >= 3 && parts[2].starts_with("(SELECT") {
        let col = parts[0].to_string();
        let op = match parts[1] { "=" => CompareOp::Eq, "!=" | "<>" => CompareOp::Neq, ">" => CompareOp::Gt, "<" => CompareOp::Lt, ">=" => CompareOp::Gte, "<=" => CompareOp::Lte, _ => CompareOp::Eq };
        let sq_start = trimmed.find('(').unwrap_or(0);
        let sq_str = &trimmed[sq_start..].trim();
        if let Ok(stmt) = parse_sql(sq_str) {
            return Some(WhereClause { predicate: WherePredicate::Subquery { column: col, op, subquery: Box::new(stmt) } });
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

/// Find a keyword (like AND/OR) at the top level (not inside parentheses).
fn find_top_level_keyword(s: &str, keyword: &str) -> Option<usize> {
    let mut depth = 0;
    let mut i = 0;
    let bytes = s.as_bytes();
    while i < bytes.len() {
        if bytes[i] == b'(' { depth += 1; i += 1; continue; }
        if bytes[i] == b')' { depth -= 1; i += 1; continue; }
        if depth == 0 && bytes[i] == b' ' {
            // Check if next chars match keyword followed by space or end
            let rest = &s[i+1..];
            if rest.starts_with(keyword) {
                let after = rest[keyword.len()..].as_bytes();
                if after.is_empty() || after[0] == b' ' || after[0] == b'(' {
                    return Some(i + 1);
                }
            }
        }
        i += 1;
    }
    None
}

fn parse_in_values(input: &str) -> Option<Vec<Literal>> {
    let s = input.trim().trim_start_matches('(').trim_end_matches(')').trim_end_matches(';');
    let values: Vec<Literal> = s.split(',').map(|v| parse_literal(v.trim().trim_matches('\'').trim_matches('"'))).collect();
    if values.is_empty() { None } else { Some(values) }
}
