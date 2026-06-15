//! Nexus Database Engine — Pluggable storage for the sovereign database.
//!
//! Architecture:
//!   Page (4KB) → Buffer Pool (LRU) → WAL → B-Tree / LSM-Tree → Table
//!
//! Trade-offs addressed:
//!   - B-Tree for point reads (O(log n) lookup)
//!   - LSM-Tree for heavy writes (sequential append)
//!   - Columnar paths for analytical scans
//!   - WAL for crash-safe durability (write-ahead logging)
//!   - Buffer pool for hot data (in-memory LRU cache)

pub mod buffer;
pub mod btree;
pub mod catalog;
pub mod index;
pub mod lsm;
pub mod page;
pub mod pgwire;
pub mod row;
pub mod sql;
pub mod storage;
pub mod transaction;
pub mod wal;

use std::sync::Arc;
use parking_lot::RwLock;

/// Database engine instance. Contains all subsystems.
pub struct Engine {
    pub buffer_pool: Arc<buffer::BufferPool>,
    pub wal: Arc<RwLock<wal::WriteAheadLog>>,
    pub config: EngineConfig,
    pub started_at: std::time::Instant,
}

#[derive(Clone, Debug)]
pub struct EngineConfig {
    /// Page size in bytes (default 4096)
    pub page_size: usize,
    /// Buffer pool capacity in pages (default 1000)
    pub buffer_pool_pages: usize,
    /// WAL file path
    pub wal_path: String,
    /// Data directory
    pub data_dir: String,
    /// Whether to fsync after every write (disable for benchmarks)
    pub fsync: bool,
}

impl Default for EngineConfig {
    fn default() -> Self {
        Self {
            page_size: 4096,
            buffer_pool_pages: 1000,
            wal_path: "nexus_wal.log".into(),
            data_dir: "nexus_data".into(),
            fsync: true,
        }
    }
}

impl Engine {
    /// Initialize a new database engine.
    pub fn open(config: EngineConfig) -> Result<Self> {
        let wal = wal::WriteAheadLog::open(&config.wal_path)?;
        let buffer_pool = buffer::BufferPool::new(config.buffer_pool_pages, config.page_size);

        // Recovery: replay WAL into buffer pool
        wal.replay(&buffer_pool)?;

        Ok(Self {
            buffer_pool: Arc::new(buffer_pool),
            wal: Arc::new(RwLock::new(wal)),
            config,
            started_at: std::time::Instant::now(),
        })
    }

    /// Get engine stats.
    pub fn stats(&self) -> EngineStats {
        EngineStats {
            uptime_secs: self.started_at.elapsed().as_secs(),
            pages_cached: self.buffer_pool.cached_pages(),
            pages_total: self.buffer_pool.total_pages(),
            wal_bytes: self.wal.read().size_bytes(),
        }
    }
}

#[derive(Debug, Clone)]
pub struct EngineStats {
    pub uptime_secs: u64,
    pub pages_cached: usize,
    pub pages_total: usize,
    pub wal_bytes: u64,
}

#[derive(Debug, thiserror::Error)]
pub enum EngineError {
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),
    #[error("Page not found: {0}")]
    PageNotFound(u64),
    #[error("Buffer full (capacity {0} pages)")]
    BufferFull(usize),
    #[error("WAL corrupted: {0}")]
    WalCorrupted(String),
    #[error("Key not found: {0}")]
    KeyNotFound(String),
    #[error("Serialization error: {0}")]
    Serialization(String),
}

pub type Result<T> = std::result::Result<T, EngineError>;
