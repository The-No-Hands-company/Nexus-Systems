//! Columnar Analytics Engine — OLAP queries with GROUP BY and aggregates.
//!
//! Stores data by column (not by row) for efficient analytical scans.
//! Supports COUNT, SUM, AVG, MIN, MAX with GROUP BY clauses.
//!
//! Columnar format advantages:
//!   - Cache locality: scan one column's data contiguously
//!   - Compression: run-length encoding, dictionary encoding
//!   - Vectorized: process 1024 values at a time (SIMD-ready)
//!   - Late materialization: only fetch columns actually needed

use std::collections::HashMap;
use parking_lot::RwLock;

use crate::catalog::ColumnType;
use crate::{EngineError, Result};

// ── Column Store ────────────────────────────────────────────────

/// Stores table data in columnar format for analytical queries.
pub struct ColumnStore {
    /// table_name → list of column chunks
    tables: RwLock<HashMap<String, Vec<ColumnChunk>>>,
    /// table_name → number of rows
    row_counts: RwLock<HashMap<String, usize>>,
}

/// A compressed chunk of column data with metadata.
#[derive(Debug, Clone)]
pub struct ColumnChunk {
    pub column_name: String,
    pub column_type: ColumnType,
    /// All values for this column (one per row)
    pub data: Vec<Option<Vec<u8>>>,
    /// Minimum value in this chunk
    pub min: Option<Vec<u8>>,
    /// Maximum value in this chunk
    pub max: Option<Vec<u8>>,
    /// Number of distinct values
    pub distinct_count: usize,
    /// Whether this chunk is compressed
    pub compressed: bool,
}

impl ColumnStore {
    pub fn new() -> Self {
        Self {
            tables: RwLock::new(HashMap::new()),
            row_counts: RwLock::new(HashMap::new()),
        }
    }

    /// Initialize columnar storage for a table.
    pub fn create_table(&self, name: &str, columns: &[(String, ColumnType)]) {
        let chunks: Vec<ColumnChunk> = columns.iter().map(|(col_name, col_type)| {
            ColumnChunk {
                column_name: col_name.clone(),
                column_type: col_type.clone(),
                data: Vec::new(),
                min: None,
                max: None,
                distinct_count: 0,
                compressed: false,
            }
        }).collect();
        self.tables.write().insert(name.to_string(), chunks);
        self.row_counts.write().insert(name.to_string(), 0);
    }

    /// Add a column to an existing table (filled with NULLs for existing rows).
    pub fn add_column(&self, table: &str, col_name: &str, col_type: ColumnType) {
        let mut tables = self.tables.write();
        if let Some(chunks) = tables.get_mut(table) {
            let row_count = self.row_counts.read().get(table).copied().unwrap_or(0);
            let data = vec![None; row_count];
            chunks.push(ColumnChunk {
                column_name: col_name.to_string(),
                column_type: col_type,
                data,
                min: None,
                max: None,
                distinct_count: 0,
                compressed: false,
            });
        }
    }

    /// Drop a column from a table.
    pub fn drop_column(&self, table: &str, col_name: &str) {
        let mut tables = self.tables.write();
        if let Some(chunks) = tables.get_mut(table) {
            if let Some(pos) = chunks.iter().position(|c| c.column_name == col_name) {
                chunks.remove(pos);
            }
        }
    }

    /// Drop an entire table from the columnar store.
    pub fn drop_table(&self, table: &str) {
        self.tables.write().remove(table);
        self.row_counts.write().remove(table);
    }

    /// Append a row to the columnar store.
    pub fn append_row(&self, table: &str, values: &[Option<Vec<u8>>], column_types: &[ColumnType]) -> Result<()> {
        let mut tables = self.tables.write();
        let chunks = tables.get_mut(table)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not in column store", table)))?;

        for (i, val) in values.iter().enumerate() {
            if i >= chunks.len() { break; }
            let chunk = &mut chunks[i];
            chunk.data.push(val.clone());

            // Update min/max
            if let Some(ref v) = val {
                match (&chunk.min, &chunk.max) {
                    (None, None) => {
                        chunk.min = Some(v.clone());
                        chunk.max = Some(v.clone());
                    }
                    (Some(min), Some(max)) => {
                        if v < min { chunk.min = Some(v.clone()); }
                        if v > max { chunk.max = Some(v.clone()); }
                    }
                    _ => {}
                }
            }
        }

        let mut counts = self.row_counts.write();
        *counts.entry(table.to_string()).or_default() += 1;

        Ok(())
    }

    /// Get a column's data for scanning.
    pub fn get_column(&self, table: &str, column: &str) -> Result<Vec<Option<Vec<u8>>>> {
        let tables = self.tables.read();
        let chunks = tables.get(table)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", table)))?;
        for chunk in chunks {
            if chunk.column_name == column {
                return Ok(chunk.data.clone());
            }
        }
        Err(EngineError::KeyNotFound(format!("Column {} not found in {}", column, table)))
    }

    /// Row count for a table.
    pub fn row_count(&self, table: &str) -> usize {
        self.row_counts.read().get(table).copied().unwrap_or(0)
    }

    /// Get chunk metadata for query planning.
    pub fn chunk_metadata(&self, table: &str) -> Option<Vec<ColumnChunk>> {
        self.tables.read().get(table).cloned()
    }

    /// Update a single cell in the columnar store.
    pub fn update_row(&self, table: &str, column: &str, row_idx: usize, new_value: Option<Vec<u8>>) -> Result<()> {
        let mut tables = self.tables.write();
        let chunks = tables.get_mut(table)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", table)))?;
        for chunk in chunks.iter_mut() {
            if chunk.column_name == column {
                if row_idx < chunk.data.len() {
                    chunk.data[row_idx] = new_value;
                    return Ok(());
                }
            }
        }
        Err(EngineError::KeyNotFound(format!("Column {} not found", column)))
    }

    /// Delete a row (mark as NULL in all columns, skip during SELECT).
    pub fn delete_row(&self, table: &str, row_idx: usize) -> Result<()> {
        let mut tables = self.tables.write();
        let chunks = tables.get_mut(table)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", table)))?;
        for chunk in chunks.iter_mut() {
            if row_idx < chunk.data.len() {
                chunk.data[row_idx] = None;
            }
        }
        Ok(())
    }

    pub fn truncate_table(&self, table: &str) {
        if let Some(chunks) = self.tables.write().get_mut(table) {
            for chunk in chunks.iter_mut() {
                chunk.data.clear();
            }
        }
    }
}

// ── Aggregation Functions ───────────────────────────────────────

#[derive(Debug, Clone, PartialEq)]
pub enum AggregateFunc {
    Count,
    Sum,
    Avg,
    Min,
    Max,
    CountDistinct,
}

impl AggregateFunc {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_uppercase().as_str() {
            "COUNT" => Some(Self::Count),
            "SUM" => Some(Self::Sum),
            "AVG" => Some(Self::Avg),
            "MIN" => Some(Self::Min),
            "MAX" => Some(Self::Max),
            "COUNT DISTINCT" | "COUNT_DISTINCT" => Some(Self::CountDistinct),
            _ => None,
        }
    }
}

/// A single aggregate computation.
#[derive(Debug, Clone)]
pub struct AggregateState {
    pub func: AggregateFunc,
    pub column: String,
    pub alias: Option<String>,
    /// Running state
    count: u64,
    sum: f64,
    min: Option<String>,
    max: Option<String>,
    distinct_values: HashMap<String, bool>,
}

impl AggregateState {
    pub fn new(func: AggregateFunc, column: &str, alias: Option<&str>) -> Self {
        Self {
            func,
            column: column.to_string(),
            alias: alias.map(|s| s.to_string()),
            count: 0,
            sum: 0.0,
            min: None,
            max: None,
            distinct_values: HashMap::new(),
        }
    }

    /// Feed a value into the aggregate.
    pub fn accumulate(&mut self, value: Option<&str>) {
        match self.func {
            AggregateFunc::Count => {
                if value.is_some() { self.count += 1; }
            }
            AggregateFunc::Sum | AggregateFunc::Avg => {
                if let Some(v) = value {
                    if let Ok(n) = v.parse::<f64>() {
                        self.sum += n;
                        self.count += 1;
                    }
                }
            }
            AggregateFunc::Min => {
                if let Some(v) = value {
                    match &self.min {
                        None => self.min = Some(v.to_string()),
                        Some(m) if v.as_bytes() < m.as_bytes() => self.min = Some(v.to_string()),
                        _ => {}
                    }
                }
            }
            AggregateFunc::Max => {
                if let Some(v) = value {
                    match &self.max {
                        None => self.max = Some(v.to_string()),
                        Some(m) if v.as_bytes() > m.as_bytes() => self.max = Some(v.to_string()),
                        _ => {}
                    }
                }
            }
            AggregateFunc::CountDistinct => {
                if let Some(v) = value {
                    self.distinct_values.insert(v.to_string(), true);
                }
            }
        }
    }

    /// Get the final result.
    pub fn result(&self) -> String {
        match self.func {
            AggregateFunc::Count | AggregateFunc::CountDistinct => {
                if self.func == AggregateFunc::CountDistinct {
                    self.distinct_values.len().to_string()
                } else {
                    self.count.to_string()
                }
            }
            AggregateFunc::Sum => format!("{:.2}", self.sum),
            AggregateFunc::Avg => {
                if self.count > 0 {
                    format!("{:.2}", self.sum / self.count as f64)
                } else {
                    "0".to_string()
                }
            }
            AggregateFunc::Min => self.min.clone().unwrap_or_else(|| "NULL".to_string()),
            AggregateFunc::Max => self.max.clone().unwrap_or_else(|| "NULL".to_string()),
        }
    }

    /// Reset the aggregate state.
    pub fn reset(&mut self) {
        self.count = 0;
        self.sum = 0.0;
        self.min = None;
        self.max = None;
        self.distinct_values.clear();
    }
}

// ── GROUP BY Engine ─────────────────────────────────────────────

/// Execute a GROUP BY query against columnar data.
pub fn execute_group_by(
    store: &ColumnStore,
    table: &str,
    group_column: &str,
    aggregates: &mut [AggregateState],
) -> Result<Vec<Vec<String>>> {
    let group_data = store.get_column(table, group_column)?;
    let row_count = store.row_count(table);

    // Collect all needed columns
    let mut agg_data: Vec<Vec<Option<Vec<u8>>>> = Vec::new();
    for agg in aggregates.iter() {
        agg_data.push(store.get_column(table, &agg.column)?);
    }

    // Hash map: group_key → aggregate states
    type GroupKey = String;
    let mut groups: HashMap<GroupKey, Vec<AggregateState>> = HashMap::new();

    for row_idx in 0..row_count {
        let group_val = group_data.get(row_idx)
            .and_then(|v| v.as_ref())
            .map(|b| String::from_utf8_lossy(b).to_string())
            .unwrap_or_else(|| "NULL".to_string());

        let entry = groups.entry(group_val.clone()).or_insert_with(|| {
            aggregates.iter().map(|a| AggregateState::new(
                a.func.clone(), &a.column, a.alias.as_deref()
            )).collect()
        });

        for (i, state) in entry.iter_mut().enumerate() {
            let val = agg_data.get(i)
                .and_then(|col| col.get(row_idx))
                .and_then(|v| v.as_ref())
                .map(|b| String::from_utf8_lossy(b).to_string());
            state.accumulate(val.as_deref());
        }
    }

    // Build result rows
    let mut result: Vec<Vec<String>> = Vec::new();
    for (group_key, states) in groups.iter() {
        let mut row = vec![group_key.clone()];
        for state in states {
            row.push(state.result());
        }
        result.push(row);
    }

    // Sort by group key
    result.sort_by(|a, b| a[0].cmp(&b[0]));
    Ok(result)
}

/// Execute aggregate query (no GROUP BY — single row result).
pub fn execute_aggregate(
    store: &ColumnStore,
    table: &str,
    aggregates: &mut [AggregateState],
) -> Result<Vec<String>> {
    let row_count = store.row_count(table);
    let mut agg_data: Vec<Vec<Option<Vec<u8>>>> = Vec::new();

    for agg in aggregates.iter() {
        agg_data.push(store.get_column(table, &agg.column)?);
    }

    // Scan all rows once
    for row_idx in 0..row_count {
        for (i, state) in aggregates.iter_mut().enumerate() {
            let val = agg_data.get(i)
                .and_then(|col| col.get(row_idx))
                .and_then(|v| v.as_ref())
                .map(|b| String::from_utf8_lossy(b).to_string());
            state.accumulate(val.as_deref());
        }
    }

    Ok(aggregates.iter().map(|a| a.result()).collect())
}

// ── Columnar scan with filtering ────────────────────────────────

/// Scan a column with a predicate, returning matching row indices.
pub fn column_scan_filter(
    store: &ColumnStore,
    table: &str,
    column: &str,
    predicate: &dyn Fn(Option<&[u8]>) -> bool,
) -> Result<Vec<usize>> {
    let data = store.get_column(table, column)?;
    let mut matches = Vec::new();
    for (idx, val) in data.iter().enumerate() {
        if predicate(val.as_deref()) {
            matches.push(idx);
        }
    }
    Ok(matches)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_column_store_append_and_read() {
        let store = ColumnStore::new();
        store.create_table("users", &[
            ("name".into(), ColumnType::Text),
            ("age".into(), ColumnType::Integer),
        ]);

        store.append_row("users", &[
            Some(b"alice".to_vec()), Some(b"30".to_vec()),
        ], &[ColumnType::Text, ColumnType::Integer]).unwrap();

        store.append_row("users", &[
            Some(b"bob".to_vec()), Some(b"25".to_vec()),
        ], &[ColumnType::Text, ColumnType::Integer]).unwrap();

        assert_eq!(store.row_count("users"), 2);
        let names = store.get_column("users", "name").unwrap();
        assert_eq!(names.len(), 2);
    }

    #[test]
    fn test_aggregate_count() {
        let mut agg = AggregateState::new(AggregateFunc::Count, "name", None);
        agg.accumulate(Some("alice"));
        agg.accumulate(Some("bob"));
        agg.accumulate(None);
        assert_eq!(agg.result(), "2"); // NULL not counted
    }

    #[test]
    fn test_aggregate_sum() {
        let mut agg = AggregateState::new(AggregateFunc::Sum, "price", None);
        agg.accumulate(Some("10"));
        agg.accumulate(Some("20"));
        agg.accumulate(Some("30"));
        assert_eq!(agg.result(), "60.00");
    }

    #[test]
    fn test_aggregate_min_max() {
        let mut min_agg = AggregateState::new(AggregateFunc::Min, "val", None);
        let mut max_agg = AggregateState::new(AggregateFunc::Max, "val", None);

        for v in &["5", "42", "3", "99"] {
            min_agg.accumulate(Some(v));
            max_agg.accumulate(Some(v));
        }

        assert_eq!(min_agg.result(), "3");
        assert_eq!(max_agg.result(), "99");
    }

    #[test]
    fn test_group_by() {
        let store = ColumnStore::new();
        store.create_table("sales", &[
            ("region".into(), ColumnType::Text),
            ("amount".into(), ColumnType::Integer),
        ]);

        store.append_row("sales", &[Some(b"EU".to_vec()), Some(b"100".to_vec())], &[ColumnType::Text, ColumnType::Integer]).unwrap();
        store.append_row("sales", &[Some(b"US".to_vec()), Some(b"200".to_vec())], &[ColumnType::Text, ColumnType::Integer]).unwrap();
        store.append_row("sales", &[Some(b"EU".to_vec()), Some(b"150".to_vec())], &[ColumnType::Text, ColumnType::Integer]).unwrap();

        let mut aggs = vec![AggregateState::new(AggregateFunc::Sum, "amount", Some("total"))];
        let result = execute_group_by(&store, "sales", "region", &mut aggs).unwrap();
        // EU: 250, US: 200
        assert_eq!(result.len(), 2);
    }

    #[test]
    fn test_column_scan_filter() {
        let store = ColumnStore::new();
        store.create_table("t", &[("x".into(), ColumnType::Integer)]);
        store.append_row("t", &[Some(b"10".to_vec())], &[ColumnType::Integer]).unwrap();
        store.append_row("t", &[Some(b"20".to_vec())], &[ColumnType::Integer]).unwrap();
        store.append_row("t", &[Some(b"30".to_vec())], &[ColumnType::Integer]).unwrap();

        let matches = column_scan_filter(&store, "t", "x",
            &|v| v.and_then(|b| String::from_utf8_lossy(b).parse::<i32>().ok()).map(|n| n > 15).unwrap_or(false)
        ).unwrap();
        assert_eq!(matches.len(), 2); // 20 and 30
    }
}
