//! Foreign Keys — REFERENCES constraint with referential integrity.
//!
//! Ensures that values in a column reference existing values in another table.
//! Enforces ON DELETE RESTRICT (default) — referenced rows cannot be deleted.
//!
//! SQL:
//!   CREATE TABLE orders (id INTEGER, user_id INTEGER REFERENCES users(id));
//!   INSERT INTO orders VALUES (1, 5);  -- ERROR if user 5 doesn't exist

use std::collections::HashMap;
use parking_lot::RwLock;

use crate::columnar::ColumnStore;

/// A foreign key relationship between two tables.
#[derive(Debug, Clone)]
pub struct ForeignKey {
    pub source_table: String,
    pub source_column: String,
    pub target_table: String,
    pub target_column: String,
    pub on_delete: FkAction,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FkAction {
    Restrict,
    Cascade,
    SetNull,
}

impl FkAction {
    pub fn from_str(s: &str) -> Self {
        match s.to_uppercase().as_str() {
            "CASCADE" => Self::Cascade,
            "SET NULL" | "SETNULL" => Self::SetNull,
            _ => Self::Restrict,
        }
    }
}

/// Registry of all foreign key relationships.
pub struct ForeignKeyStore {
    fks: RwLock<Vec<ForeignKey>>,
}

impl ForeignKeyStore {
    pub fn new() -> Self {
        Self { fks: RwLock::new(Vec::new()) }
    }

    /// Register a foreign key relationship.
    pub fn add(&self, fk: ForeignKey) {
        self.fks.write().push(fk);
    }

    /// Get all foreign keys where source_table is the given table.
    pub fn get_for_source(&self, table: &str) -> Vec<ForeignKey> {
        self.fks.read().iter()
            .filter(|fk| fk.source_table == table)
            .cloned()
            .collect()
    }

    /// Get all foreign keys where target_table is the given table.
    pub fn get_for_target(&self, table: &str) -> Vec<ForeignKey> {
        self.fks.read().iter()
            .filter(|fk| fk.target_table == table)
            .cloned()
            .collect()
    }

    /// Validate an INSERT/UPDATE: check that referenced values exist.
    /// Returns error message if constraint is violated.
    pub fn validate_reference(&self, table: &str, column: &str, value: &[u8], columnar: &ColumnStore) -> Option<String> {
        let fks = self.fks.read();
        for fk in fks.iter() {
            if fk.source_table == table && fk.source_column == column {
                // Check if value exists in target table
                let target_data = columnar.get_column(&fk.target_table, &fk.target_column).ok()?;
                let exists = target_data.iter().any(|v| v.as_deref() == Some(value));
                if !exists {
                    return Some(format!(
                        "insert or update on table \"{}\" violates foreign key constraint: key ({}) is not present in table \"{}\"",
                        table, String::from_utf8_lossy(value), fk.target_table
                    ));
                }
            }
        }
        None
    }

    /// Check if a DELETE would violate referential integrity.
    /// Returns list of tables that reference this row.
    pub fn check_delete(&self, table: &str, column: &str, value: &[u8], columnar: &ColumnStore) -> Vec<String> {
        let fks = self.fks.read();
        let mut violations = Vec::new();

        for fk in fks.iter() {
            if fk.target_table == table && fk.target_column == column {
                match fk.on_delete {
                    FkAction::Restrict => {
                        // Check if any rows in source table reference this value
                        if let Ok(source_data) = columnar.get_column(&fk.source_table, &fk.source_column) {
                            if source_data.iter().any(|v| v.as_deref() == Some(value)) {
                                violations.push(fk.source_table.clone());
                            }
                        }
                    }
                    FkAction::Cascade => {
                        // Would delete referencing rows — not implemented yet
                    }
                    FkAction::SetNull => {
                        // Would set referencing columns to NULL — not implemented yet
                    }
                }
            }
        }
        violations
    }

    /// Parse REFERENCES clause from CREATE TABLE.
    /// e.g., "REFERENCES users(id)" → ForeignKey
    pub fn parse_references(source_table: &str, source_column: &str, clause: &str) -> Option<ForeignKey> {
        let rest = clause.trim().strip_prefix("REFERENCES")?.trim();
        let (target_table, rest) = rest.split_once('(')?;
        let target_table = target_table.trim().to_string();
        let target_column = rest.trim_end_matches(')').trim().to_string();

        Some(ForeignKey {
            source_table: source_table.to_string(),
            source_column: source_column.to_string(),
            target_table,
            target_column,
            on_delete: FkAction::Restrict,
        })
    }

    /// List all foreign keys.
    pub fn list_all(&self) -> Vec<ForeignKey> {
        self.fks.read().clone()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::catalog::ColumnType;

    #[test]
    fn test_fk_parse() {
        let fk = ForeignKeyStore::parse_references("orders", "user_id", "REFERENCES users(id)").unwrap();
        assert_eq!(fk.source_table, "orders");
        assert_eq!(fk.source_column, "user_id");
        assert_eq!(fk.target_table, "users");
        assert_eq!(fk.target_column, "id");
    }

    #[test]
    fn test_fk_validation_passes() {
        let store = ForeignKeyStore::new();
        let col_store = ColumnStore::new();

        // Create target table with data
        col_store.create_table("users", &[("id".into(), ColumnType::Integer)]);
        col_store.append_row("users", &[Some(b"1".to_vec())], &[ColumnType::Integer]).unwrap();

        store.add(ForeignKey {
            source_table: "orders".into(), source_column: "user_id".into(),
            target_table: "users".into(), target_column: "id".into(),
            on_delete: FkAction::Restrict,
        });

        // Valid: user_id=1 exists in users
        assert!(store.validate_reference("orders", "user_id", b"1", &col_store).is_none());
    }

    #[test]
    fn test_fk_validation_fails() {
        let store = ForeignKeyStore::new();
        let col_store = ColumnStore::new();

        col_store.create_table("users", &[("id".into(), ColumnType::Integer)]);
        col_store.append_row("users", &[Some(b"1".to_vec())], &[ColumnType::Integer]).unwrap();

        store.add(ForeignKey {
            source_table: "orders".into(), source_column: "user_id".into(),
            target_table: "users".into(), target_column: "id".into(),
            on_delete: FkAction::Restrict,
        });

        // Invalid: user_id=99 does NOT exist
        let err = store.validate_reference("orders", "user_id", b"99", &col_store);
        assert!(err.is_some());
        assert!(err.unwrap().contains("violates foreign key"));
    }

    #[test]
    fn test_fk_delete_restrict() {
        let store = ForeignKeyStore::new();
        let col_store = ColumnStore::new();

        col_store.create_table("users", &[("id".into(), ColumnType::Integer)]);
        col_store.create_table("orders", &[("user_id".into(), ColumnType::Integer)]);
        col_store.append_row("users", &[Some(b"1".to_vec())], &[ColumnType::Integer]).unwrap();
        col_store.append_row("orders", &[Some(b"1".to_vec())], &[ColumnType::Integer]).unwrap();

        store.add(ForeignKey {
            source_table: "orders".into(), source_column: "user_id".into(),
            target_table: "users".into(), target_column: "id".into(),
            on_delete: FkAction::Restrict,
        });

        // Deleting user 1 should be restricted because orders reference it
        let violations = store.check_delete("users", "id", b"1", &col_store);
        assert!(!violations.is_empty());
        assert!(violations.contains(&"orders".to_string()));
    }
}
