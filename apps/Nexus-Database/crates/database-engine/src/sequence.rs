//! Sequences — auto-incrementing counters for primary keys.
//!
//! Provides `CREATE SEQUENCE`, `nextval()`, and the `SERIAL` type.
//! SERIAL is syntactic sugar: `id SERIAL` creates a sequence and
//! sets the default to `nextval('seq_name')`.
//!
//! SQL:
//!   CREATE TABLE users (id SERIAL, name TEXT);
//!   INSERT INTO users (name) VALUES ('alice');  -- id auto-assigned 1
//!   INSERT INTO users (name) VALUES ('bob');    -- id auto-assigned 2

use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use parking_lot::RwLock;

/// Manages all sequences in the database.
pub struct SequenceStore {
    sequences: RwLock<HashMap<String, AtomicU64>>,
}

impl SequenceStore {
    pub fn new() -> Self {
        Self { sequences: RwLock::new(HashMap::new()) }
    }

    /// Create a new sequence starting at 1.
    pub fn create_sequence(&self, name: &str) {
        self.sequences.write().entry(name.to_string())
            .or_insert_with(|| AtomicU64::new(1));
    }

    /// Create a sequence with a custom start value.
    pub fn create_sequence_start(&self, name: &str, start: u64) {
        self.sequences.write().insert(name.to_string(), AtomicU64::new(start));
    }

    /// Drop a sequence.
    pub fn drop_sequence(&self, name: &str) {
        self.sequences.write().remove(name);
    }

    /// Get the next value from a sequence (auto-creates if missing).
    pub fn nextval(&self, name: &str) -> u64 {
        let seqs = self.sequences.read();
        if let Some(seq) = seqs.get(name) {
            seq.fetch_add(1, Ordering::SeqCst)
        } else {
            drop(seqs);
            // fetch_add returns old value: store 2 so first call returns 1, second returns 2, etc.
            self.sequences.write().insert(name.to_string(), AtomicU64::new(2));
            1
        }
    }

    /// Get the current value (stored value minus 1 = last returned value).
    pub fn currval(&self, name: &str) -> Option<u64> {
        self.sequences.read().get(name)
            .map(|s| {
                let v = s.load(Ordering::SeqCst);
                if v > 0 { v.saturating_sub(1) } else { 0 }
            })
    }

    /// Set a sequence to a specific value.
    pub fn setval(&self, name: &str, value: u64) {
        if let Some(seq) = self.sequences.read().get(name) {
            seq.store(value, Ordering::SeqCst);
        } else {
            self.sequences.write().insert(name.to_string(), AtomicU64::new(value));
        }
    }

    /// Check if a sequence exists.
    pub fn exists(&self, name: &str) -> bool {
        self.sequences.read().contains_key(name)
    }

    /// List all sequences.
    pub fn list(&self) -> Vec<(String, u64)> {
        self.sequences.read().iter()
            .map(|(name, seq)| (name.clone(), seq.load(Ordering::SeqCst)))
            .collect()
    }

    /// Auto-generate sequence name for a table column.
    pub fn serial_sequence_name(table: &str, column: &str) -> String {
        format!("{}_seq", table)
    }
}

/// Parse SERIAL in CREATE TABLE and register the sequence + constraint.
/// Returns the column type to use (Integer) and the default constraint.
pub fn handle_serial_column(store: &SequenceStore, table: &str, column: &str) -> (crate::catalog::ColumnType, crate::constraints::ColumnConstraint) {
    let seq_name = SequenceStore::serial_sequence_name(table, column);
    store.create_sequence(&seq_name);
    (crate::catalog::ColumnType::Integer, crate::constraints::ColumnConstraint::DefaultValue(seq_name))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_sequence_nextval() {
        let store = SequenceStore::new();
        assert_eq!(store.nextval("users_seq"), 1);
        assert_eq!(store.nextval("users_seq"), 2);
        assert_eq!(store.nextval("users_seq"), 3);
    }

    #[test]
    fn test_sequence_currval() {
        let store = SequenceStore::new();
        store.nextval("test_seq");
        assert_eq!(store.currval("test_seq"), Some(1));
    }

    #[test]
    fn test_sequence_setval() {
        let store = SequenceStore::new();
        store.setval("test_seq", 100);
        assert_eq!(store.nextval("test_seq"), 100);
    }

    #[test]
    fn test_sequence_drop() {
        let store = SequenceStore::new();
        store.create_sequence("temp");
        assert!(store.exists("temp"));
        store.drop_sequence("temp");
        assert!(!store.exists("temp"));
    }

    #[test]
    fn test_serial_sequence_name() {
        let name = SequenceStore::serial_sequence_name("users", "id");
        assert_eq!(name, "users_seq");
    }

    #[test]
    fn test_handle_serial() {
        let store = SequenceStore::new();
        let (col_type, constraint) = handle_serial_column(&store, "users", "id");
        assert_eq!(col_type, crate::catalog::ColumnType::Integer);
        assert!(matches!(constraint, crate::constraints::ColumnConstraint::DefaultValue(_)));
        assert!(store.exists("users_seq"));
        assert_eq!(store.nextval("users_seq"), 1);
    }
}
