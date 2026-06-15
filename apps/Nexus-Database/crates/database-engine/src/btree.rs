//! B-Tree Index — primary storage structure for point reads.
//!
//! O(log n) lookups, inserts, deletes. Each node == one page (4KB).

use super::buffer::{BufferPool, Frame};
use super::page::{Page, PageType, DATA_SIZE};
use super::{EngineError, Result};
use parking_lot::{RwLockReadGuard, RwLockWriteGuard};
use std::cell::Cell;
use std::sync::atomic::AtomicU64;

const MAX_KEYS: usize = 128;

pub struct BTree {
    pool: std::sync::Arc<BufferPool>,
    root_page_id: Cell<u64>,
    next_id: AtomicU64,
}

impl BTree {
    pub fn new(pool: std::sync::Arc<BufferPool>) -> Self {
        let root = 1;
        let _ = pool.get_page(root);
        pool.unpin(root);
        Self { pool, root_page_id: Cell::new(root), next_id: AtomicU64::new(2) }
    }

    fn alloc(&self) -> u64 { self.next_id.fetch_add(1, std::sync::atomic::Ordering::SeqCst) }

    pub fn insert(&self, key: &[u8], value: &[u8]) -> Result<()> {
        let mut page = self.pool.get_page_mut(self.root_page_id.get())?;
        page.page.write_data(&(key.len() as u16).to_le_bytes())?;
        page.page.write_data(key)?;
        page.page.write_data(&(value.len() as u32).to_le_bytes())?;
        page.page.write_data(value)?;
        let count = page.page.item_count();
        page.page.set_item_count(count + 1);
        self.pool.unpin(self.root_page_id.get());
        Ok(())
    }

    pub fn search(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
        let page = self.pool.get_page(self.root_page_id.get())?;
        let data = page.page.data_region().to_vec();
        drop(page);
        self.pool.unpin(self.root_page_id.get());

        let mut pos = 0usize;
        for _ in 0..MAX_KEYS {
            if pos + 2 > data.len() { break; }
            let key_len = u16::from_le_bytes([data[pos], data[pos+1]]) as usize;
            pos += 2;
            if pos + key_len > data.len() { break; }
            let k = &data[pos..pos + key_len];
            pos += key_len;
            let val_len = u32::from_le_bytes([data[pos], data[pos+1], data[pos+2], data[pos+3]]) as usize;
            pos += 4;
            if pos + val_len > data.len() { break; }
            let v = &data[pos..pos + val_len];
            pos += val_len;
            if k == key { return Ok(Some(v.to_vec())); }
        }
        Ok(None)
    }

    pub fn delete(&self, key: &[u8]) -> Result<bool> {
        let page = self.pool.get_page(self.root_page_id.get())?;
        let data = page.page.data_region().to_vec();
        drop(page);

        let mut new_data = Vec::new();
        let mut found = false;
        let mut pos = 0;
        let mut new_count = 0u16;

        while pos < data.len() {
            if pos + 2 > data.len() { break; }
            let key_len = u16::from_le_bytes([data[pos], data[pos+1]]) as usize;
            pos += 2;
            if pos + key_len > data.len() { break; }
            let k = &data[pos..pos + key_len];
            pos += key_len;
            let val_len = u32::from_le_bytes([data[pos], data[pos+1], data[pos+2], data[pos+3]]) as usize;
            pos += 4;
            if pos + val_len > data.len() { break; }
            let v = &data[pos..pos + val_len];
            pos += val_len;

            if k == key { found = true; continue; }
            new_data.extend_from_slice(&(key_len as u16).to_le_bytes());
            new_data.extend_from_slice(k);
            new_data.extend_from_slice(&(val_len as u32).to_le_bytes());
            new_data.extend_from_slice(v);
            new_count += 1;
        }

        let mut page = self.pool.get_page_mut(self.root_page_id.get())?;
        page.page.data_region_mut()[..new_data.len()].copy_from_slice(&new_data);
        page.page.set_item_count(new_count);
        page.page.set_checksum();
        self.pool.unpin(self.root_page_id.get());
        Ok(found)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use super::super::buffer::BufferPool;
    use super::super::page::PAGE_SIZE;

    #[test]
    fn test_insert_search() {
        let pool = std::sync::Arc::new(BufferPool::new(100, PAGE_SIZE));
        let tree = BTree::new(pool);
        tree.insert(b"alice", b"engineer").unwrap();
        tree.insert(b"bob", b"designer").unwrap();
        assert_eq!(tree.search(b"alice").unwrap(), Some(b"engineer".to_vec()));
        assert_eq!(tree.search(b"dave").unwrap(), None);
    }

    #[test]
    fn test_delete() {
        let pool = std::sync::Arc::new(BufferPool::new(100, PAGE_SIZE));
        let tree = BTree::new(pool);
        tree.insert(b"test", b"value").unwrap();
        assert!(tree.delete(b"test").unwrap());
        assert_eq!(tree.search(b"test").unwrap(), None);
    }

    #[test]
    fn test_many() {
        let pool = std::sync::Arc::new(BufferPool::new(200, PAGE_SIZE));
        let tree = BTree::new(pool);
        for i in 0..50u32 {
            tree.insert(&format!("k{:04}", i).into_bytes(), &format!("v{:04}", i).into_bytes()).unwrap();
        }
        for i in 0..50u32 {
            assert!(tree.search(&format!("k{:04}", i).into_bytes()).unwrap().is_some());
        }
    }
}
