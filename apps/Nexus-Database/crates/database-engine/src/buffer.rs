//! Buffer Pool — LRU page cache for hot data.
//!
//! Keeps frequently accessed pages in memory. When full, evicts
//! the least-recently-used page to disk. Dirty pages are flushed
//! to storage before eviction.
//!
//! Architecture:
//!   Frame[0..N] → pages + LRU list + dirty flags
//!   Page table: page_id → frame index (HashMap for O(1) lookup)

use super::page::{Page, PageType, PAGE_SIZE};
use super::{EngineError, Result};
use parking_lot::RwLock;
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};

/// A frame in the buffer pool holding one page.
pub(crate) struct Frame {
    pub(crate) page: Page,
    pub(crate) page_id: u64,
    pub(crate) pin_count: u32,
    pub(crate) dirty: bool,
    pub(crate) last_access: u64, // logical timestamp for LRU
}

pub struct BufferPool {
    frames: Vec<RwLock<Frame>>,
    page_table: RwLock<HashMap<u64, usize>>, // page_id → frame_idx
    capacity: usize,
    clock: AtomicU64,
    page_size: usize,
    hits: AtomicU64,
    misses: AtomicU64,
}

impl BufferPool {
    pub fn new(num_frames: usize, page_size: usize) -> Self {
        let mut frames = Vec::with_capacity(num_frames);
        for _ in 0..num_frames {
            frames.push(RwLock::new(Frame {
                page: Page::new(0, PageType::Leaf),
                page_id: 0,
                pin_count: 0,
                dirty: false,
                last_access: 0,
            }));
        }
        Self {
            frames,
            page_table: RwLock::new(HashMap::new()),
            capacity: num_frames,
            clock: AtomicU64::new(0),
            page_size,
            hits: AtomicU64::new(0),
            misses: AtomicU64::new(0),
        }
    }

    fn tick(&self) -> u64 {
        self.clock.fetch_add(1, Ordering::SeqCst)
    }

    /// Fetch a page. Returns a read lock on the frame.
    pub fn get_page(&self, page_id: u64) -> Result<parking_lot::RwLockReadGuard<Frame>> {
        // Check page table
        {
            let pt = self.page_table.read();
            if let Some(&idx) = pt.get(&page_id) {
                self.hits.fetch_add(1, Ordering::Relaxed);
                let mut frame = self.frames[idx].write();
                frame.last_access = self.tick();
                frame.pin_count += 1;
                drop(frame);
                return Ok(self.frames[idx].read());
            }
        }

        // Page not in memory — find or evict a frame
        self.misses.fetch_add(1, Ordering::Relaxed);
        let idx = self.evict_frame()?;

        // Load page from "disk" (zeroed for new pages)
        let mut frame = self.frames[idx].write();
        frame.page = Page::new(page_id, PageType::Leaf);
        frame.page_id = page_id;
        frame.pin_count = 1;
        frame.dirty = false;
        frame.last_access = self.tick();
        drop(frame);

        // Update page table
        self.page_table.write().insert(page_id, idx);

        Ok(self.frames[idx].read())
    }

    /// Get mutable access to a page.
    pub fn get_page_mut(&self, page_id: u64) -> Result<parking_lot::RwLockWriteGuard<Frame>> {
        // First acquire read, then upgrade
        let _read = self.get_page(page_id)?;
        drop(_read);

        let pt = self.page_table.read();
        if let Some(&idx) = pt.get(&page_id) {
            let mut frame = self.frames[idx].write();
            frame.dirty = true;
            frame.last_access = self.tick();
            return Ok(frame);
        }
        Err(EngineError::PageNotFound(page_id))
    }

    /// Unpin a page (decrement pin count).
    pub fn unpin(&self, page_id: u64) {
        let pt = self.page_table.read();
        if let Some(&idx) = pt.get(&page_id) {
            let mut frame = self.frames[idx].write();
            if frame.pin_count > 0 {
                frame.pin_count -= 1;
            }
        }
    }

    /// Find a free or evictable frame using LRU.
    fn evict_frame(&self) -> Result<usize> {
        // First: find a free frame (page_id == 0, never used)
        for i in 0..self.capacity {
            let frame = self.frames[i].read();
            if frame.page_id == 0 && frame.pin_count == 0 {
                return Ok(i);
            }
        }

        // Second: LRU eviction — find the unpinned frame with oldest access
        let mut oldest_idx = None;
        let mut oldest_time = u64::MAX;

        for i in 0..self.capacity {
            let frame = self.frames[i].read();
            if frame.pin_count == 0 && frame.last_access < oldest_time {
                oldest_time = frame.last_access;
                oldest_idx = Some(i);
            }
        }

        if let Some(idx) = oldest_idx {
            let frame = self.frames[idx].read();
            let old_id = frame.page_id;
            drop(frame);

            // Flush dirty page to "disk" before evicting
            let frame = self.frames[idx].read();
            if frame.dirty {
                // TODO: write to storage layer
                log::debug!("Flushing dirty page {} to disk", old_id);
            }
            drop(frame);

            // Remove from page table
            self.page_table.write().remove(&old_id);
            return Ok(idx);
        }

        Err(EngineError::BufferFull(self.capacity))
    }

    /// Stats for monitoring.
    pub fn cached_pages(&self) -> usize {
        self.page_table.read().len()
    }

    pub fn total_pages(&self) -> usize {
        self.capacity
    }

    pub fn hit_ratio(&self) -> f64 {
        let hits = self.hits.load(Ordering::Relaxed) as f64;
        let total = hits + self.misses.load(Ordering::Relaxed) as f64;
        if total == 0.0 { 1.0 } else { hits / total }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_buffer_pool_get_new_page() {
        let pool = BufferPool::new(10, PAGE_SIZE);
        let _page = pool.get_page(1).unwrap();
        assert_eq!(pool.cached_pages(), 1);
        pool.unpin(1);
    }

    #[test]
    fn test_buffer_pool_reuse_page() {
        let pool = BufferPool::new(10, PAGE_SIZE);
        {
            let _p = pool.get_page(42).unwrap();
        }
        pool.unpin(42);
        {
            let _p2 = pool.get_page(42).unwrap();
        }
        // Should be a cache hit
        assert!(pool.hit_ratio() > 0.0);
    }

    #[test]
    fn test_buffer_pool_eviction() {
        let pool = BufferPool::new(2, PAGE_SIZE);
        // Fill pool
        pool.get_page(1).unwrap(); pool.unpin(1);
        pool.get_page(2).unwrap(); pool.unpin(2);
        // This should evict page 1 (oldest)
        pool.get_page(3).unwrap(); pool.unpin(3);
        assert_eq!(pool.cached_pages(), 2);
    }
}
