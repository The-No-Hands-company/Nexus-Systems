//! Write-Ahead Log (WAL) — crash recovery for the database engine.
//!
//! Every modification is written to the WAL before the data file.
//! On crash, the WAL is replayed to restore consistency.
//!
//! WAL entry format (variable length):
//!   [0..8]   LSN: u64 LE (log sequence number)
//!   [8..10]  entry_type: u16 LE (0=insert, 1=delete, 2=update)
//!   [10..14] page_id: u32 LE
//!   [14..18] data_len: u32 LE
//!   [18..]  data: entry-specific payload

use super::buffer::BufferPool;
use super::page::PAGE_SIZE;
use super::{EngineError, Result};
use std::fs::{File, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::Path;

const ENTRY_HEADER_SIZE: usize = 22; // lsn:8 + type:2 + page_id:8 + data_len:4

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u16)]
pub enum WalEntryType {
    Insert = 0,
    Delete = 1,
    Update = 2,
    Checkpoint = 3,
}

impl WalEntryType {
    fn from_u16(v: u16) -> Option<Self> {
        match v {
            0 => Some(Self::Insert),
            1 => Some(Self::Delete),
            2 => Some(Self::Update),
            3 => Some(Self::Checkpoint),
            _ => None,
        }
    }
}

#[derive(Debug, Clone)]
pub struct WalEntry {
    pub lsn: u64,
    pub entry_type: WalEntryType,
    pub page_id: u64,
    pub data: Vec<u8>,
}

pub struct WriteAheadLog {
    file: File,
    next_lsn: u64,
    path: String,
}

impl WriteAheadLog {
    pub fn open(path: &str) -> Result<Self> {
        let file = OpenOptions::new()
            .create(true)
            .append(true)
            .read(true)
            .open(path)?;

        // Find the last LSN in the file
        let metadata = file.metadata()?;
        let next_lsn = if metadata.len() > 0 {
            let mut buf = [0u8; 8];
            let mut f = OpenOptions::new().read(true).open(path)?;
            f.seek(SeekFrom::End(-8))?;
            f.read_exact(&mut buf)?;
            u64::from_le_bytes(buf) + 1
        } else {
            1
        };

        Ok(Self {
            file,
            next_lsn,
            path: path.to_string(),
        })
    }

    /// Append a modification entry to the WAL.
    pub fn append(&mut self, entry_type: WalEntryType, page_id: u64, data: &[u8]) -> Result<u64> {
        let lsn = self.next_lsn;
        let mut buf = Vec::with_capacity(ENTRY_HEADER_SIZE + data.len());

        buf.extend_from_slice(&lsn.to_le_bytes());
        buf.extend_from_slice(&(entry_type as u16).to_le_bytes());
        buf.extend_from_slice(&page_id.to_le_bytes());
        buf.extend_from_slice(&(data.len() as u32).to_le_bytes());
        buf.extend_from_slice(data);

        self.file.write_all(&buf)?;
        self.file.flush()?;
        self.next_lsn += 1;
        Ok(lsn)
    }

    /// Write a full page snapshot (for checkpoints).
    pub fn checkpoint_page(&mut self, page_id: u64, page_data: &[u8; PAGE_SIZE]) -> Result<u64> {
        self.append(WalEntryType::Checkpoint, page_id, page_data)
    }

    /// Replay all entries from the WAL into the buffer pool.
    pub fn replay(&self, _pool: &BufferPool) -> Result<usize> {
        let mut count = 0;
        let mut file = OpenOptions::new().read(true).open(&self.path)?;

        loop {
            let mut header = [0u8; ENTRY_HEADER_SIZE];
            match file.read_exact(&mut header) {
                Ok(_) => {},
                Err(e) if e.kind() == std::io::ErrorKind::UnexpectedEof => break,
                Err(e) => return Err(e.into()),
            }

            let lsn = u64::from_le_bytes(header[0..8].try_into().unwrap());
            let entry_type = WalEntryType::from_u16(u16::from_le_bytes(header[8..10].try_into().unwrap()))
                .ok_or(EngineError::WalCorrupted(format!("Unknown entry type at LSN {}", lsn)))?;
            let page_id = u64::from_le_bytes(header[10..18].try_into().unwrap());
            let data_len = u32::from_le_bytes(header[18..22].try_into().unwrap()) as usize;
            let mut data = vec![0u8; data_len];
            file.read_exact(&mut data)?;

            match entry_type {
                WalEntryType::Checkpoint => {
                    // Full page restore
                    if let Ok(mut frame) = _pool.get_page_mut(page_id) {
                        frame.page.data.copy_from_slice(&data[..PAGE_SIZE.min(data_len)]);
                    }
                }
                _ => {
                    // Apply incremental change
                    if let Ok(mut frame) = _pool.get_page_mut(page_id) {
                        let offset = frame.page.free_offset() as usize;
                        let end = (offset + data.len()).min(PAGE_SIZE);
                        frame.page.data[offset..end].copy_from_slice(&data);
                    }
                }
            }
            _pool.unpin(page_id);
            count += 1;
        }

        log::info!("WAL replay: {} entries recovered", count);
        Ok(count)
    }

    /// Truncate the WAL (after a successful checkpoint).
    pub fn truncate(&mut self) -> Result<()> {
        self.file.set_len(0)?;
        self.file.seek(SeekFrom::Start(0))?;
        self.next_lsn = 1;
        Ok(())
    }

    pub fn size_bytes(&self) -> u64 {
        self.file.metadata().map(|m| m.len()).unwrap_or(0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn test_wal_append_and_replay() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("test.wal");
        let path_str = path.to_str().unwrap();

        let mut wal = WriteAheadLog::open(path_str).unwrap();
        wal.append(WalEntryType::Insert, 1, b"hello").unwrap();
        wal.append(WalEntryType::Update, 1, b"world").unwrap();

        // Create a pool and replay
        let pool = BufferPool::new(10, PAGE_SIZE);
        let replayed = wal.replay(&pool).unwrap();
        assert!(replayed > 0);
    }

    #[test]
    fn test_wal_truncate() {
        let dir = tempdir().unwrap();
        let path = dir.path().join("trunc.wal");
        let mut wal = WriteAheadLog::open(path.to_str().unwrap()).unwrap();
        wal.append(WalEntryType::Insert, 1, b"data").unwrap();
        wal.truncate().unwrap();
        assert_eq!(wal.size_bytes(), 0);
    }
}
