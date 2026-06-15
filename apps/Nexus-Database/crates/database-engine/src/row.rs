//! Row / Tuple layout — how data is physically stored in pages.
//!
//! Each row is a sequence of columns stored in a compact binary format.
//! Row layout within a page:
//!   [row_count:u16][row_offsets:u16 × N][row_data...]
//!
//! Each row in data region:
//!   [null_bitmap:u8][col0_len:u16][col0_data][col1_len:u16][col1_data]...

use crate::{EngineError, Result};

/// Maximum columns per row.
const MAX_COLUMNS: usize = 64;

/// A row of data (tuple).
#[derive(Debug, Clone, PartialEq)]
pub struct Row {
    pub columns: Vec<Option<Vec<u8>>>,
}

impl Row {
    pub fn new(columns: Vec<Option<Vec<u8>>>) -> Self {
        assert!(columns.len() <= MAX_COLUMNS);
        Self { columns }
    }

    /// Serialize a row into bytes for storage.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut buf = Vec::new();
        // Null bitmap: one bit per column
        let mut null_bitmap = 0u64;
        for (i, col) in self.columns.iter().enumerate() {
            if col.is_none() {
                null_bitmap |= 1u64 << i;
            }
        }
        buf.extend_from_slice(&null_bitmap.to_le_bytes());

        // Column data
        for col in &self.columns {
            match col {
                Some(data) => {
                    buf.extend_from_slice(&(data.len() as u16).to_le_bytes());
                    buf.extend_from_slice(data);
                }
                None => {
                    buf.extend_from_slice(&0u16.to_le_bytes());
                }
            }
        }
        buf
    }

    /// Deserialize a row from bytes.
    pub fn from_bytes(data: &[u8], num_columns: usize) -> Result<Self> {
        if data.len() < 8 { return Err(EngineError::Serialization("Row too short".into())); }
        let null_bitmap = u64::from_le_bytes(data[0..8].try_into().unwrap());
        let mut columns = Vec::with_capacity(num_columns);
        let mut pos = 8usize;

        for i in 0..num_columns {
            if pos + 2 > data.len() { break; }
            let len = u16::from_le_bytes([data[pos], data[pos + 1]]) as usize;
            pos += 2;

            if (null_bitmap >> i) & 1 == 1 {
                columns.push(None);
            } else {
                if pos + len > data.len() { break; }
                columns.push(Some(data[pos..pos + len].to_vec()));
                pos += len;
            }
        }

        Ok(Self { columns })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_row_roundtrip() {
        let row = Row::new(vec![
            Some(b"alice".to_vec()),
            Some(b"engineer".to_vec()),
            None,
            Some(b"42".to_vec()),
        ]);
        let bytes = row.to_bytes();
        let decoded = Row::from_bytes(&bytes, 4).unwrap();
        assert_eq!(row, decoded);
    }

    #[test]
    fn test_row_all_nulls() {
        let row = Row::new(vec![None, None, None]);
        let bytes = row.to_bytes();
        let decoded = Row::from_bytes(&bytes, 3).unwrap();
        assert_eq!(row, decoded);
    }
}
