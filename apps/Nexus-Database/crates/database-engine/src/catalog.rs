//! Delta-Main Bridge — polymorphic storage virtualization.
//!
//! Routes writes to the Delta store (LSM-Tree, write-optimized)
//! and reads to the Main store (B-Tree, read-optimized).
//!
//! Tables declare their engine at creation time or use AUTO mode
//! which detects access patterns and migrates between engines.

use std::collections::HashMap;
use std::sync::Arc;
use std::sync::atomic::{AtomicU64, Ordering};
use parking_lot::RwLock;

use crate::btree::BTree;
use crate::buffer::BufferPool;
use crate::columnar::ColumnStore;
use crate::lsm::LsmTree;
use crate::page::PAGE_SIZE;
use crate::row::Row;
use crate::{EngineError, Result};

// ── Storage Engine Declaration ──────────────────────────────────

/// Which storage engine a table uses.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum StorageEngine {
    /// B-Tree — optimized for point reads and updates (Main store)
    BTree,
    /// LSM-Tree — optimized for heavy writes (Delta store)
    Lsm,
    /// Auto — engine chosen based on access pattern detection
    Auto,
}

impl StorageEngine {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_uppercase().as_str() {
            "BTREE" | "B-TREE" => Some(Self::BTree),
            "LSM" | "LSM-TREE" => Some(Self::Lsm),
            "AUTO" => Some(Self::Auto),
            _ => None,
        }
    }
}

// ── Table Metadata ──────────────────────────────────────────────

/// Metadata for a single table.
#[derive(Debug, Clone)]
pub struct TableMeta {
    pub name: String,
    pub engine: StorageEngine,
    pub column_names: Vec<String>,
    pub column_types: Vec<ColumnType>,
    pub column_constraints: Vec<Vec<ColumnConstraint>>,
    pub created_at: std::time::SystemTime,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ColumnConstraint {
    NotNull,
    Default(String),
}

impl ColumnConstraint {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_uppercase().as_str() {
            "NOT NULL" => Some(Self::NotNull),
            other if other.starts_with("DEFAULT ") => {
                Some(Self::Default(other[8..].trim().trim_matches('\'').trim_matches('"').to_string()))
            }
            _ => None,
        }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub enum ColumnType {
    Text,
    Integer,
    Float,
    Boolean,
    Blob,
    Jsonb,
}

impl ColumnType {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_uppercase().as_str() {
            "TEXT" | "VARCHAR" | "STRING" => Some(Self::Text),
            "INTEGER" | "INT" | "BIGINT" => Some(Self::Integer),
            "FLOAT" | "REAL" | "DOUBLE" => Some(Self::Float),
            "BOOLEAN" | "BOOL" => Some(Self::Boolean),
            "BLOB" | "BYTEA" => Some(Self::Blob),
            "JSONB" | "JSON" => Some(Self::Jsonb),
            _ => None,
        }
    }
}

// ── Access Pattern Tracking ─────────────────────────────────────

/// Per-table access pattern counters.
#[derive(Debug, Default)]
pub struct AccessPattern {
    /// Number of read operations since last check
    pub reads: AtomicU64,
    /// Number of write operations since last check
    pub writes: AtomicU64,
    /// Last time a migration was considered
    pub last_check: AtomicU64,
}

impl AccessPattern {
    pub fn record_read(&self) {
        self.reads.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_write(&self) {
        self.writes.fetch_add(1, Ordering::Relaxed);
    }

    /// Return the read-to-write ratio. > 1.0 means read-heavy, < 1.0 means write-heavy.
    pub fn ratio(&self) -> f64 {
        let r = self.reads.load(Ordering::Relaxed) as f64;
        let w = self.writes.load(Ordering::Relaxed) as f64;
        if w == 0.0 { return 100.0; } // pure reads
        r / w
    }

    /// Reset counters (called after migration check).
    pub fn reset(&self) {
        self.reads.store(0, Ordering::Relaxed);
        self.writes.store(0, Ordering::Relaxed);
        self.last_check.store(std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap_or_default().as_secs(), Ordering::Relaxed);
    }
}

// ── Table Catalog ───────────────────────────────────────────────

/// Central registry of all tables and their storage configuration.
pub struct TableCatalog {
    tables: RwLock<HashMap<String, TableMeta>>,
    patterns: RwLock<HashMap<String, Arc<AccessPattern>>>,
    /// AUTO migration threshold: read/write ratio triggers
    pub migration_read_threshold: f64,
    pub migration_write_threshold: f64,
}

impl TableCatalog {
    pub fn new() -> Self {
        Self {
            tables: RwLock::new(HashMap::new()),
            patterns: RwLock::new(HashMap::new()),
            migration_read_threshold: 10.0,  // >10x reads → migrate to BTree
            migration_write_threshold: 0.1,  // <0.1x reads → migrate to LSM
        }
    }

    /// Register a new table.
    pub fn create_table(&self, name: &str, engine: StorageEngine, columns: Vec<(String, ColumnType)>) -> Result<()> {
        let mut tables = self.tables.write();
        if tables.contains_key(name) {
            return Err(EngineError::KeyNotFound(format!("Table {} already exists", name)));
        }

        let mut col_names = Vec::new(); let mut col_types = Vec::new(); for (n, t) in columns { col_names.push(n); col_types.push(t); }

        tables.insert(name.to_string(), TableMeta {
            column_constraints: vec![vec![]; col_names.len()],
            name: name.to_string(),
            engine,
            column_names: col_names,
            column_types: col_types,
            created_at: std::time::SystemTime::now(),
        });

        self.patterns.write().insert(name.to_string(), Arc::new(AccessPattern::default()));
        Ok(())
    }

    /// Get table metadata.
    pub fn get_table(&self, name: &str) -> Option<TableMeta> {
        self.tables.read().get(name).cloned()
    }

    /// Add a column to an existing table.
    pub fn add_column(&self, table: &str, col_name: &str, col_type: ColumnType) -> Result<()> {
        let mut tables = self.tables.write();
        let meta = tables.get_mut(table)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", table)))?;
        meta.column_names.push(col_name.to_string());
        meta.column_types.push(col_type);
        meta.column_constraints.push(vec![]);
        Ok(())
    }

    /// Drop a column from a table.
    pub fn drop_column(&self, table: &str, col_name: &str) -> Result<()> {
        let mut tables = self.tables.write();
        let meta = tables.get_mut(table)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", table)))?;
        if let Some(pos) = meta.column_names.iter().position(|c| c == col_name) {
            meta.column_names.remove(pos);
            meta.column_types.remove(pos);
            meta.column_constraints.remove(pos);
        }
        Ok(())
    }

    /// Drop a table entirely.
    pub fn drop_table(&self, name: &str) -> Result<()> {
        let mut tables = self.tables.write();
        tables.remove(name)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", name)))?;
        self.patterns.write().remove(name);
        Ok(())
    }

    /// Rename a table.
    pub fn rename_table(&self, old_name: &str, new_name: &str) -> Result<()> {
        let mut tables = self.tables.write();
        let mut meta = tables.remove(old_name)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", old_name)))?;
        meta.name = new_name.to_string();
        tables.insert(new_name.to_string(), meta);
        Ok(())
    }

    /// Rename a column.
    pub fn rename_column(&self, table: &str, old_name: &str, new_name: &str) -> Result<()> {
        let mut tables = self.tables.write();
        let meta = tables.get_mut(table)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Table {} not found", table)))?;
        if let Some(pos) = meta.column_names.iter().position(|c| c == old_name) {
            meta.column_names[pos] = new_name.to_string();
        }
        Ok(())
    }

    /// Record a read operation on a table.
    pub fn record_read(&self, table: &str) {
        if let Some(pattern) = self.patterns.read().get(table) {
            pattern.record_read();
        }
    }

    /// Record a write operation on a table.
    pub fn record_write(&self, table: &str) {
        if let Some(pattern) = self.patterns.read().get(table) {
            pattern.record_write();
        }
    }

    /// Get access pattern for a table.
    pub fn get_pattern(&self, table: &str) -> Option<Arc<AccessPattern>> {
        self.patterns.read().get(table).cloned()
    }

    /// Check if a table's access pattern warrants migration.
    pub fn check_migration(&self, table: &str) -> Option<StorageEngine> {
        let meta = self.tables.read().get(table)?.clone();
        if meta.engine != StorageEngine::Auto {
            return None;
        }

        let pattern = self.patterns.read().get(table)?.clone();

        // Don't check too frequently — minimum 60s between checks
        {
            let last_secs = pattern.last_check.load(Ordering::Relaxed);
            if std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap_or_default().as_secs() - last_secs < 60 {
                return Some(meta.engine); // keep current
            }
        }

        let ratio = pattern.ratio();
        let total = pattern.reads.load(Ordering::Relaxed) + pattern.writes.load(Ordering::Relaxed);

        // Only consider migration after enough data (> 100 operations)
        if total < 100 {
            return Some(meta.engine);
        }

        let recommendation = if ratio > self.migration_read_threshold {
            // Read-heavy → use B-Tree
            StorageEngine::BTree
        } else if ratio < self.migration_write_threshold {
            // Write-heavy → use LSM
            StorageEngine::Lsm
        } else {
            return Some(meta.engine); // balanced — keep current
        };

        pattern.reset();

        // Actually migrate
        if recommendation != meta.engine {
            if let Some(mut meta) = self.tables.write().get_mut(table) {
                meta.engine = recommendation;
                log::info!("AUTO migration: {} → {:?} (ratio: {:.2})", table, recommendation, ratio);
            }
        }

        Some(recommendation)
    }

    /// List all tables.
    pub fn list_tables(&self) -> Vec<TableMeta> {
        self.tables.read().values().cloned().collect()
    }
}

// ── Delta-Main Router ───────────────────────────────────────────

/// Routes operations to the correct storage engine.
/// The core of the Delta-Main architecture.
pub struct DeltaMainRouter {
    pub catalog: Arc<TableCatalog>,
    btree: Arc<BTree>,
    lsm: RwLock<LsmTree>,
    pub columnar: RwLock<ColumnStore>,
    pub views: RwLock<HashMap<String, String>>,
    pub indexes: Arc<crate::index::IndexManager>,
}

impl DeltaMainRouter {
    /// Create a new router backed by a shared buffer pool.
    pub fn new(pool: Arc<BufferPool>, lsm_dir: std::path::PathBuf) -> Result<Self> {
        let btree = Arc::new(BTree::new(pool.clone()));
        let lsm = LsmTree::open(lsm_dir)?;
        let columnar = ColumnStore::new();
        Ok(Self {
            catalog: Arc::new(TableCatalog::new()),
            btree,
            lsm: RwLock::new(lsm),
            columnar: RwLock::new(columnar),
            views: RwLock::new(HashMap::new()),
            indexes: Arc::new(crate::index::IndexManager::new(pool.clone())),
        })
    }

    pub fn catalog(&self) -> &Arc<TableCatalog> { &self.catalog }

    /// Insert a row into a table. Always routes to LSM (Delta store) for write speed.
    pub fn insert(&self, table: &str, row: &Row) -> Result<()> {
        self.catalog.record_write(table);

        // 1. Write to LSM (Delta store — fast append)
        let key = self.make_key(table, &row);
        let value = row.to_bytes();
        self.lsm.write().put(&key, &value)?;

        // 2. Also write to B-Tree (Main store) for immediate read consistency
        //    In full Delta-Main, this is done by the compactor, not inline.
        self.btree.insert(&key, &value)?;

        Ok(())
    }

    /// Query rows from a table.
    pub fn select(&self, table: &str, _columns: &[String]) -> Result<Vec<Row>> {
        self.catalog.record_read(table);

        // Check for migration recommendation
        let _ = self.catalog.check_migration(table);

        // Read from B-Tree (Main store — fast point queries)
        // For range scans, we'd use LSM for columnar efficiency.
        // This is simplified — real impl uses the query planner.
        let prefix = format!("{}:", table).into_bytes();

        // Scan B-Tree for matching keys
        let mut rows: Vec<Row> = Vec::new();
        // In full impl: use btree.scan_range(prefix..prefix_end)
        // For now: return empty (query planner integration pending)
        let _ = prefix;
        let _ = rows;

        Ok(vec![])
    }

    /// Force compaction of the Delta store into the Main store.
    pub fn compact_delta(&self) -> Result<()> {
        let mut lsm = self.lsm.write();
        lsm.flush()?;
        lsm.compact()?;
        log::info!("Delta store compacted: {} SSTables", lsm.sstable_count());
        Ok(())
    }

    /// Get engine stats for monitoring.
    pub fn stats(&self) -> DeltaMainStats {
        let lsm = self.lsm.read();
        DeltaMainStats {
            tables: self.catalog.list_tables().len(),
            lsm_memtable_size: lsm.memtable_size(),
            lsm_sstables: lsm.sstable_count(),
            patterns: self.catalog.list_tables().iter().map(|t| {
                let p = self.catalog.get_pattern(&t.name);
                let ratio = p.map(|p| p.ratio()).unwrap_or(0.0);
                (t.name.clone(), t.engine, ratio)
            }).collect(),
        }
    }

    fn make_key(&self, table: &str, _row: &Row) -> Vec<u8> {
        // Key format: table_name:row_hash
        let mut key = table.as_bytes().to_vec();
        key.push(b':');
        // In production: use primary key or UUID
        key.extend_from_slice(&uuid::Uuid::new_v4().as_bytes()[..8]);
        key
    }
}

#[derive(Debug, Clone)]
pub struct DeltaMainStats {
    pub tables: usize,
    pub lsm_memtable_size: usize,
    pub lsm_sstables: usize,
    pub patterns: Vec<(String, StorageEngine, f64)>,
}

// ── Tests ───────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_create_table() {
        let catalog = TableCatalog::new();
        catalog.create_table("users", StorageEngine::BTree, vec![
            ("id".into(), ColumnType::Integer),
            ("name".into(), ColumnType::Text),
        ]).unwrap();
        assert!(catalog.get_table("users").is_some());
    }

    #[test]
    fn test_access_pattern() {
        let pattern = AccessPattern::default();
        for _ in 0..100 { pattern.record_read(); }
        for _ in 0..10 { pattern.record_write(); }
        assert!(pattern.ratio() > 5.0); // read-heavy
    }

    #[test]
    fn test_auto_migration() {
        let catalog = TableCatalog::new();
        catalog.create_table("logs", StorageEngine::Auto, vec![
            ("msg".into(), ColumnType::Text),
        ]).unwrap();

        // Simulate heavy write workload
        for _ in 0..200 {
            catalog.record_write("logs");
        }
        catalog.record_read("logs"); // single read

        // Force the time check
        if let Some(p) = catalog.get_pattern("logs") {
            p.last_check.store(std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).unwrap_or_default().as_secs() - 61, Ordering::Relaxed);
        }

        let rec = catalog.check_migration("logs");
        assert_eq!(rec, Some(StorageEngine::Lsm)); // should migrate to LSM
    }

    #[test]
    fn test_storage_engine_parsing() {
        assert_eq!(StorageEngine::from_str("BTREE"), Some(StorageEngine::BTree));
        assert_eq!(StorageEngine::from_str("lsm"), Some(StorageEngine::Lsm));
        assert_eq!(StorageEngine::from_str("Auto"), Some(StorageEngine::Auto));
        assert_eq!(StorageEngine::from_str("cassandra"), None);
    }
}
