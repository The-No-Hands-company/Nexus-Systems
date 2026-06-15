//! Transaction Manager — ACID-compliant BEGIN/COMMIT/ROLLBACK.
//!
//! Architecture:
//!   BEGIN  → create txn, record in WAL
//!   DML    → buffer changes in txn (not visible to others)
//!   COMMIT → flush all changes, mark committed in WAL
//!   ROLLBACK → discard buffered changes
//!
//! Isolation: Read Committed (reads see only committed data)
//! Durability: WAL flushes on every COMMIT

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;
use parking_lot::RwLock;

use crate::wal::{WalEntryType, WriteAheadLog};
use crate::{EngineError, Result};

// ── Transaction ────────────────────────────────────────────────

/// Unique transaction identifier.
pub type TxnId = u64;

/// Transaction status.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TxnStatus {
    Active,
    Committed,
    RolledBack,
}

/// A single operation within a transaction.
#[derive(Debug, Clone)]
pub struct TxnOperation {
    pub page_id: u64,
    pub old_data: Vec<u8>,  // before-image (for rollback)
    pub new_data: Vec<u8>,  // after-image (for commit)
}

/// A database transaction.
#[derive(Debug, Clone)]
pub struct Transaction {
    pub id: TxnId,
    pub status: TxnStatus,
    pub started_at: std::time::SystemTime,
    /// Buffered operations — applied only on COMMIT
    pub operations: Vec<TxnOperation>,
    /// Whether this transaction has written any data
    pub dirty: bool,
}

impl Transaction {
    pub fn new(id: TxnId) -> Self {
        Self {
            id,
            status: TxnStatus::Active,
            started_at: std::time::SystemTime::now(),
            operations: Vec::new(),
            dirty: false,
        }
    }

    /// Record a page modification within this transaction.
    pub fn record_change(&mut self, page_id: u64, old_data: &[u8], new_data: &[u8]) {
        self.operations.push(TxnOperation {
            page_id,
            old_data: old_data.to_vec(),
            new_data: new_data.to_vec(),
        });
        self.dirty = true;
    }

    /// Number of buffered operations.
    pub fn operation_count(&self) -> usize {
        self.operations.len()
    }
}

// ── Transaction Manager ────────────────────────────────────────

/// Manages all active transactions and WAL integration.
pub struct TransactionManager {
    /// Active transactions by ID
    active: RwLock<HashMap<TxnId, Transaction>>,
    /// Next transaction ID
    next_id: AtomicU64,
    /// Write-ahead log for durability
    wal: Arc<RwLock<WriteAheadLog>>,
    /// Last committed transaction ID (for MVCC read visibility)
    last_committed_id: AtomicU64,
    /// Statistics
    commits: AtomicU64,
    rollbacks: AtomicU64,
}

impl TransactionManager {
    /// Create a new transaction manager with a WAL.
    pub fn new(wal: Arc<RwLock<WriteAheadLog>>) -> Self {
        Self {
            active: RwLock::new(HashMap::new()),
            next_id: AtomicU64::new(1),
            wal,
            last_committed_id: AtomicU64::new(0),
            commits: AtomicU64::new(0),
            rollbacks: AtomicU64::new(0),
        }
    }

    /// Begin a new transaction. Returns the transaction ID.
    pub fn begin(&self) -> Result<TxnId> {
        let id = self.next_id.fetch_add(1, Ordering::SeqCst);
        let txn = Transaction::new(id);

        // Write BEGIN record to WAL
        {
            let mut wal = self.wal.write();
            let record = id.to_le_bytes().to_vec();
            wal.append(WalEntryType::Insert, 0, &record)?; // page 0 = control page
        }

        self.active.write().insert(id, txn);
        log::debug!("BEGIN txn {}", id);
        Ok(id)
    }

    /// Commit a transaction. Applies all buffered changes.
    pub fn commit(&self, txn_id: TxnId) -> Result<usize> {
        let mut active = self.active.write();
        let mut txn = active.remove(&txn_id)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Transaction {} not active", txn_id)))?;

        if txn.status != TxnStatus::Active {
            return Err(EngineError::WalCorrupted(format!("Transaction {} already {}", txn_id,
                match txn.status { TxnStatus::Committed => "committed", _ => "rolled back" })));
        }

        let op_count = txn.operation_count();

        // Apply all buffered changes
        {
            let mut wal = self.wal.write();
            for op in &txn.operations {
                // Write each change to WAL for durability
                wal.append(WalEntryType::Update, op.page_id, &op.new_data)?;
            }
            // Write COMMIT marker
            wal.append(WalEntryType::Update, 0, &txn_id.to_le_bytes())?;
        }

        txn.status = TxnStatus::Committed;
        self.last_committed_id.store(txn_id, Ordering::SeqCst);
        self.commits.fetch_add(1, Ordering::Relaxed);

        log::debug!("COMMIT txn {} ({} operations)", txn_id, op_count);
        Ok(op_count)
    }

    /// Rollback a transaction. Discards all buffered changes.
    pub fn rollback(&self, txn_id: TxnId) -> Result<usize> {
        let mut active = self.active.write();
        let mut txn = active.remove(&txn_id)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Transaction {} not active", txn_id)))?;

        if txn.status != TxnStatus::Active {
            return Err(EngineError::WalCorrupted(format!("Transaction {} already final", txn_id)));
        }

        let op_count = txn.operation_count();
        txn.status = TxnStatus::RolledBack;

        // Write ROLLBACK marker to WAL
        {
            let mut wal = self.wal.write();
            wal.append(WalEntryType::Delete, 0, &txn_id.to_le_bytes())?;
        }

        self.rollbacks.fetch_add(1, Ordering::Relaxed);
        log::debug!("ROLLBACK txn {} ({} operations discarded)", txn_id, op_count);
        Ok(op_count)
    }

    /// Get the current transaction for a session (if active).
    pub fn get_txn(&self, txn_id: TxnId) -> Option<Transaction> {
        self.active.read().get(&txn_id).cloned()
    }

    /// Check if a transaction is still active.
    pub fn is_active(&self, txn_id: TxnId) -> bool {
        self.active.read().contains_key(&txn_id)
    }

    /// Record a page change within an active transaction.
    pub fn record_change(&self, txn_id: TxnId, page_id: u64, old_data: &[u8], new_data: &[u8]) -> Result<()> {
        let mut active = self.active.write();
        let txn = active.get_mut(&txn_id)
            .ok_or_else(|| EngineError::KeyNotFound(format!("Transaction {} not active", txn_id)))?;
        txn.record_change(page_id, old_data, new_data);
        Ok(())
    }

    /// Get the last committed transaction ID (for MVCC read visibility).
    pub fn last_committed(&self) -> TxnId {
        self.last_committed_id.load(Ordering::SeqCst)
    }

    /// Check if a transaction ID represents committed data (for MVCC read visibility).
    pub fn is_visible(&self, txn_id: TxnId) -> bool {
        txn_id <= self.last_committed()
    }

    /// Stats for monitoring.
    pub fn stats(&self) -> TxnStats {
        TxnStats {
            active_count: self.active.read().len(),
            last_committed: self.last_committed(),
            total_commits: self.commits.load(Ordering::Relaxed),
            total_rollbacks: self.rollbacks.load(Ordering::Relaxed),
        }
    }

    /// Rollback all active transactions (emergency cleanup, e.g. on shutdown).
    pub fn rollback_all(&self) -> usize {
        let ids: Vec<TxnId> = self.active.read().keys().copied().collect();
        let mut count = 0;
        for id in ids {
            if self.rollback(id).is_ok() { count += 1; }
        }
        count
    }
}

#[derive(Debug, Clone)]
pub struct TxnStats {
    pub active_count: usize,
    pub last_committed: TxnId,
    pub total_commits: u64,
    pub total_rollbacks: u64,
}

// ── SQL transaction commands parser ────────────────────────────

/// Parse BEGIN / COMMIT / ROLLBACK SQL commands.
pub fn parse_txn_command(input: &str) -> Option<TxnCommand> {
    let upper = input.trim().to_uppercase();
    if upper == "BEGIN" || upper == "BEGIN WORK" || upper == "BEGIN TRANSACTION" || upper.starts_with("START TRANSACTION") {
        Some(TxnCommand::Begin)
    } else if upper == "COMMIT" || upper == "COMMIT WORK" || upper == "COMMIT TRANSACTION" {
        Some(TxnCommand::Commit)
    } else if upper == "ROLLBACK" || upper == "ROLLBACK WORK" || upper == "ROLLBACK TRANSACTION" {
        Some(TxnCommand::Rollback)
    } else {
        None
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TxnCommand {
    Begin,
    Commit,
    Rollback,
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::path::PathBuf;
    use tempfile::tempdir;

    fn setup() -> (TransactionManager, PathBuf) {
        let dir = tempdir().unwrap();
        let wal_path = dir.path().join("txn_test.wal");
        let wal = WriteAheadLog::open(wal_path.to_str().unwrap()).unwrap();
        let mgr = TransactionManager::new(Arc::new(RwLock::new(wal)));
        (mgr, dir.path().to_path_buf())
    }

    #[test]
    fn test_begin_commit() {
        let (mgr, _dir) = setup();
        let id = mgr.begin().unwrap();
        assert!(id > 0);
        assert!(mgr.is_active(id));

        mgr.record_change(id, 1, b"old", b"new").unwrap();
        let count = mgr.commit(id).unwrap();
        assert_eq!(count, 1);
        assert!(!mgr.is_active(id));
        assert_eq!(mgr.last_committed(), id);
    }

    #[test]
    fn test_begin_rollback() {
        let (mgr, _dir) = setup();
        let id = mgr.begin().unwrap();
        mgr.record_change(id, 1, b"old", b"new").unwrap();
        let count = mgr.rollback(id).unwrap();
        assert_eq!(count, 1);
        assert!(!mgr.is_active(id));
    }

    #[test]
    fn test_multiple_transactions() {
        let (mgr, _dir) = setup();
        let a = mgr.begin().unwrap();
        let b = mgr.begin().unwrap();
        assert_ne!(a, b);

        mgr.commit(a).unwrap();
        mgr.rollback(b).unwrap();

        assert_eq!(mgr.last_committed(), a);
    }

    #[test]
    fn test_double_commit_fails() {
        let (mgr, _dir) = setup();
        let id = mgr.begin().unwrap();
        mgr.commit(id).unwrap();
        assert!(mgr.commit(id).is_err());
    }

    #[test]
    fn test_parse_txn_commands() {
        assert_eq!(parse_txn_command("BEGIN"), Some(TxnCommand::Begin));
        assert_eq!(parse_txn_command("COMMIT"), Some(TxnCommand::Commit));
        assert_eq!(parse_txn_command("ROLLBACK"), Some(TxnCommand::Rollback));
        assert_eq!(parse_txn_command("BEGIN TRANSACTION"), Some(TxnCommand::Begin));
        assert_eq!(parse_txn_command("SELECT * FROM users"), None);
    }

    #[test]
    fn test_rollback_all() {
        let (mgr, _dir) = setup();
        mgr.begin().unwrap();
        mgr.begin().unwrap();
        assert_eq!(mgr.rollback_all(), 2);
        assert_eq!(mgr.stats().active_count, 0);
    }
}
