//! Page — the fundamental unit of storage (4KB fixed size).
//!
//! Every page has a 16-byte header followed by payload data.
//! Pages are the unit of I/O in the buffer pool and WAL.
//!
//! Page layout:
//!   [0..8]   page_id: u64 LE
//!   [8..12]  free_offset: u32 LE (next write position)
//!   [12..14] item_count: u16 LE
//!   [14]     page_type: u8 (0=leaf, 1=internal, 2=overflow)
//!   [15]     checksum: u8 (XOR of all bytes)
//!   [16..4096] data region

use super::{EngineError, Result};

pub const PAGE_SIZE: usize = 4096;
pub const HEADER_SIZE: usize = 16;
pub const DATA_SIZE: usize = PAGE_SIZE - HEADER_SIZE;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum PageType {
    Leaf = 0,
    Internal = 1,
    Overflow = 2,
    FreeList = 3,
}

impl PageType {
    pub fn from_u8(v: u8) -> Option<Self> {
        match v {
            0 => Some(Self::Leaf),
            1 => Some(Self::Internal),
            2 => Some(Self::Overflow),
            3 => Some(Self::FreeList),
            _ => None,
        }
    }
}

/// A fixed-size database page (4KB).
#[derive(Clone)]
pub struct Page {
    pub data: [u8; PAGE_SIZE],
}

impl Default for Page {
    fn default() -> Self {
        Self { data: [0u8; PAGE_SIZE] }
    }
}

impl Page {
    pub fn new(page_id: u64, page_type: PageType) -> Self {
        let mut page = Self::default();
        page.set_page_id(page_id);
        page.set_free_offset(HEADER_SIZE as u32);
        page.set_item_count(0);
        page.set_page_type(page_type);
        page.set_checksum();
        page
    }

    // ── Header accessors ────────────────────────────────────

    pub fn page_id(&self) -> u64 {
        u64::from_le_bytes(self.data[0..8].try_into().unwrap())
    }

    pub fn set_page_id(&mut self, id: u64) {
        self.data[0..8].copy_from_slice(&id.to_le_bytes());
    }

    pub fn free_offset(&self) -> u32 {
        u32::from_le_bytes(self.data[8..12].try_into().unwrap())
    }

    pub fn set_free_offset(&mut self, offset: u32) {
        self.data[8..12].copy_from_slice(&offset.to_le_bytes());
    }

    pub fn item_count(&self) -> u16 {
        u16::from_le_bytes(self.data[12..14].try_into().unwrap())
    }

    pub fn set_item_count(&mut self, count: u16) {
        self.data[12..14].copy_from_slice(&count.to_le_bytes());
    }

    pub fn page_type(&self) -> Option<PageType> {
        PageType::from_u8(self.data[14])
    }

    pub fn set_page_type(&mut self, pt: PageType) {
        self.data[14] = pt as u8;
    }

    pub fn checksum(&self) -> u8 {
        self.data[15]
    }

    pub fn set_checksum(&mut self) {
        let chk = self.data[..15].iter().fold(0u8, |a, b| a ^ b);
        self.data[15] = chk;
    }

    pub fn verify_checksum(&self) -> bool {
        self.checksum() == self.data[..15].iter().fold(0u8, |a, b| a ^ b)
    }

    /// Available space in the data region.
    pub fn free_space(&self) -> u32 {
        (DATA_SIZE as u32).saturating_sub(self.free_offset())
    }

    // ── Data access ─────────────────────────────────────────

    /// Write bytes at the current free_offset, advancing it.
    pub fn write_data(&mut self, bytes: &[u8]) -> Result<()> {
        let offset = self.free_offset() as usize;
        let end = offset + bytes.len();
        if end > PAGE_SIZE {
            return Err(EngineError::Serialization(
                format!("Page overflow: {} + {} > {}", offset, bytes.len(), PAGE_SIZE)
            ));
        }
        self.data[offset..end].copy_from_slice(bytes);
        self.set_free_offset(end as u32);
        self.set_checksum();
        Ok(())
    }

    /// Read data at a specific offset and length.
    pub fn read_data(&self, offset: u32, len: u32) -> &[u8] {
        let start = offset as usize;
        let end = (start + len as usize).min(PAGE_SIZE);
        &self.data[start..end]
    }

    /// Get the raw data region.
    pub fn data_region(&self) -> &[u8] {
        &self.data[HEADER_SIZE..]
    }

    /// Get mutable data region.
    pub fn data_region_mut(&mut self) -> &mut [u8] {
        &mut self.data[HEADER_SIZE..]
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_page_header() {
        let mut page = Page::new(42, PageType::Leaf);
        assert_eq!(page.page_id(), 42);
        assert_eq!(page.free_offset(), HEADER_SIZE as u32);
        assert_eq!(page.item_count(), 0);
        assert_eq!(page.page_type(), Some(PageType::Leaf));
        assert!(page.verify_checksum());
    }

    #[test]
    fn test_write_read_data() {
        let mut page = Page::new(1, PageType::Leaf);
        let data = b"hello world";
        let offset = page.free_offset();
        page.write_data(data).unwrap();
        assert_eq!(page.read_data(offset, data.len() as u32), data);
        assert!(page.verify_checksum());
    }

    #[test]
    fn test_checksum_detection() {
        let mut page = Page::new(1, PageType::Leaf);
        page.data[100] ^= 0xFF; // corrupt
        assert!(!page.verify_checksum());
    }

    #[test]
    fn test_free_space() {
        let mut page = Page::new(1, PageType::Leaf);
        let initial = page.free_space();
        page.write_data(&vec![0u8; 100]).unwrap();
        assert_eq!(page.free_space(), initial - 100);
    }
}
