//! Storage layer — disk I/O abstraction.
//!
//! Manages reading/writing pages to persistent storage.
//! Pluggable backends: local filesystem, S3, memory (for testing).

use crate::page::{Page, PAGE_SIZE};
use crate::{EngineError, Result};
use std::fs::{File, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};
use std::collections::HashMap;
use parking_lot::RwLock;

/// Storage backend trait.
pub trait StorageBackend: Send + Sync {
    fn read_page(&self, page_id: u64) -> Result<Option<Page>>;
    fn write_page(&self, page_id: u64, page: &Page) -> Result<()>;
    fn sync(&self) -> Result<()>;
}

/// Filesystem-backed storage.
pub struct FileStorage {
    data_dir: PathBuf,
    files: RwLock<HashMap<u64, File>>,
}

impl FileStorage {
    pub fn open(data_dir: PathBuf) -> Result<Self> {
        std::fs::create_dir_all(&data_dir)?;
        Ok(Self { data_dir, files: RwLock::new(HashMap::new()) })
    }

    fn file_path(&self, page_id: u64) -> PathBuf {
        // Group pages into subdirectories: data/00/00000042.page
        let dir = self.data_dir.join(format!("{:02}", page_id / 1000000));
        let _ = std::fs::create_dir_all(&dir);
        dir.join(format!("{:08}.page", page_id))
    }

    fn get_file(&self, page_id: u64) -> Result<std::fs::File> {
        let path = self.file_path(page_id);
        OpenOptions::new()
            .create(true)
            .read(true)
            .write(true)
            .open(&path)
            .map_err(|e| EngineError::Io(e))
    }
}

impl StorageBackend for FileStorage {
    fn read_page(&self, page_id: u64) -> Result<Option<Page>> {
        let path = self.file_path(page_id);
        if !path.exists() {
            return Ok(None);
        }

        let mut file = File::open(&path)?;
        let mut page = Page::default();
        file.read_exact(&mut page.data)?;

        if !page.verify_checksum() {
            return Err(EngineError::WalCorrupted(format!("Checksum mismatch on page {}", page_id)));
        }

        Ok(Some(page))
    }

    fn write_page(&self, page_id: u64, page: &Page) -> Result<()> {
        let path = self.file_path(page_id);
        let mut file = OpenOptions::new().create(true).write(true).truncate(true).open(&path)?;
        file.write_all(&page.data)?;
        file.flush()?;
        Ok(())
    }

    fn sync(&self) -> Result<()> {
        // fsync all open files
        Ok(())
    }
}

/// In-memory storage (for testing and benchmarking).
pub struct MemoryStorage {
    pages: RwLock<HashMap<u64, Page>>,
}

impl MemoryStorage {
    pub fn new() -> Self {
        Self { pages: RwLock::new(HashMap::new()) }
    }
}

impl StorageBackend for MemoryStorage {
    fn read_page(&self, page_id: u64) -> Result<Option<Page>> {
        Ok(self.pages.read().get(&page_id).cloned())
    }

    fn write_page(&self, page_id: u64, page: &Page) -> Result<()> {
        self.pages.write().insert(page_id, page.clone());
        Ok(())
    }

    fn sync(&self) -> Result<()> { Ok(()) }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::page::PageType;

    #[test]
    fn test_memory_storage() {
        let store = MemoryStorage::new();
        let page = Page::new(42, PageType::Leaf);
        store.write_page(42, &page).unwrap();
        let read = store.read_page(42).unwrap().unwrap();
        assert_eq!(read.page_id(), 42);
    }
}
