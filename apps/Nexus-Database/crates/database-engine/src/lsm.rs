//! LSM-Tree Storage Engine — write-optimized, append-only.
//! The Delta store in the Delta-Main architecture.

use std::collections::BTreeMap;
use std::fs::File;
use std::io::{BufReader, BufWriter, Read, Seek, SeekFrom, Write};
use std::path::PathBuf;
use crate::{EngineError, Result};

pub struct LsmTree {
    memtable: BTreeMap<Vec<u8>, Option<Vec<u8>>>,
    memtable_max_bytes: usize,
    memtable_bytes: usize,
    sstables: Vec<SsTable>,
    data_dir: PathBuf,
    generation: u64,
}

struct SsTable {
    path: PathBuf,
    keys: Vec<Vec<u8>>,
    offsets: Vec<u64>,
    generation: u64,
    key_count: usize,
}

impl LsmTree {
    pub fn open(data_dir: PathBuf) -> Result<Self> {
        std::fs::create_dir_all(&data_dir)?;
        Ok(Self { memtable: BTreeMap::new(), memtable_max_bytes: 64*1024, memtable_bytes: 0, sstables: vec![], data_dir, generation: 0 })
    }

    pub fn put(&mut self, key: &[u8], value: &[u8]) -> Result<()> {
        self.memtable.insert(key.to_vec(), Some(value.to_vec()));
        self.memtable_bytes += key.len() + value.len();
        if self.memtable_bytes >= self.memtable_max_bytes { self.flush()?; }
        Ok(())
    }

    pub fn delete(&mut self, key: &[u8]) -> Result<()> {
        self.memtable.insert(key.to_vec(), None);
        if self.memtable_bytes >= self.memtable_max_bytes { self.flush()?; }
        Ok(())
    }

    pub fn get(&self, key: &[u8]) -> Result<Option<Vec<u8>>> {
        if let Some(v) = self.memtable.get(key) { return Ok(v.clone()); }
        for sst in &self.sstables {
            if let Some(v) = sst.get(key) { return Ok(Some(v)); }
        }
        Ok(None)
    }

    pub fn flush(&mut self) -> Result<()> {
        if self.memtable.is_empty() { return Ok(()); }
        self.generation += 1;
        let path = self.data_dir.join(format!("{:08}.sst", self.generation));
        let mut file = BufWriter::new(File::create(&path)?);
        let mut keys = Vec::new();
        let mut offsets = Vec::new();
        let mut offset = 0u64;

        for (key, value) in &self.memtable {
            let val = value.as_deref().unwrap_or(&[]);
            file.write_all(&[value.is_none() as u8])?;
            file.write_all(&(key.len() as u32).to_le_bytes())?;
            file.write_all(key)?;
            file.write_all(&(val.len() as u32).to_le_bytes())?;
            file.write_all(val)?;
            keys.push(key.clone());
            offsets.push(offset);
            offset += 1 + 4 + key.len() as u64 + 4 + val.len() as u64;
        }
        file.flush()?;

        let kc = keys.len();
        self.sstables.insert(0, SsTable { path, keys, offsets, generation: self.generation, key_count: kc });
        self.memtable.clear();
        self.memtable_bytes = 0;
        Ok(())
    }

    pub fn compact(&mut self) -> Result<()> {
        if self.sstables.len() <= 2 { return Ok(()); }
        let b = self.sstables.pop().unwrap();
        let a = self.sstables.pop().unwrap();
        self.generation += 1;
        let path = self.data_dir.join(format!("{:08}.sst", self.generation));
        let merged = SsTable::merge(&a, &b, path, self.generation)?;
        let _ = std::fs::remove_file(&a.path);
        let _ = std::fs::remove_file(&b.path);
        self.sstables.insert(0, merged);
        Ok(())
    }

    pub fn sstable_count(&self) -> usize { self.sstables.len() }
    pub fn memtable_size(&self) -> usize { self.memtable.len() }
}

impl SsTable {
    fn get(&self, key: &[u8]) -> Option<Vec<u8>> {
        let idx = self.keys.binary_search(&key.to_vec()).ok()?;
        let offset = self.offsets[idx];
        let mut file = BufReader::new(File::open(&self.path).ok()?);
        file.seek(SeekFrom::Start(offset)).ok()?;
        let mut tb = [0u8;1]; file.read_exact(&mut tb).ok()?;
        let mut lb = [0u8;4]; file.read_exact(&mut lb).ok()?;
        let kl = u32::from_le_bytes(lb) as usize;
        let mut kb = vec![0u8;kl]; file.read_exact(&mut kb).ok()?;
        file.read_exact(&mut lb).ok()?;
        let vl = u32::from_le_bytes(lb) as usize;
        let mut vb = vec![0u8;vl]; file.read_exact(&mut vb).ok()?;
        if tb[0] == 0 { Some(vb) } else { None }
    }

    fn merge(a: &SsTable, b: &SsTable, path: PathBuf, gen: u64) -> Result<Self> {
        let mut file = BufWriter::new(File::create(&path)?);
        let mut keys = Vec::new();
        let mut offsets = Vec::new();
        let mut offset = 0u64;
        let (mut ai, mut bi) = (0usize, 0usize);

        while ai < a.keys.len() || bi < b.keys.len() {
            let pick_a = bi >= b.keys.len() || (ai < a.keys.len() && a.keys[ai] <= b.keys[bi]);
            let (key, val) = if pick_a {
                let v = a.get(&a.keys[ai]).unwrap_or_default();
                let k = a.keys[ai].clone(); ai += 1; (k, v)
            } else {
                let v = b.get(&b.keys[bi]).unwrap_or_default();
                let k = b.keys[bi].clone(); bi += 1; (k, v)
            };
            file.write_all(&[0u8])?;
            file.write_all(&(key.len() as u32).to_le_bytes())?; file.write_all(&key)?;
            file.write_all(&(val.len() as u32).to_le_bytes())?; file.write_all(&val)?;
            keys.push(key);
            offsets.push(offset);
            offset += 1 + 4 + keys.last().unwrap().len() as u64 + 4 + val.len() as u64;
        }
        file.flush()?;
        let kc = keys.len();
        Ok(SsTable { path, keys, offsets, generation: gen, key_count: kc })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tempfile::tempdir;

    #[test]
    fn test_put_get() {
        let dir = tempdir().unwrap();
        let mut tree = LsmTree::open(dir.path().to_path_buf()).unwrap();
        tree.put(b"alice", b"engineer").unwrap();
        tree.put(b"bob", b"designer").unwrap();
        assert_eq!(tree.get(b"alice").unwrap(), Some(b"engineer".to_vec()));
        assert_eq!(tree.get(b"charlie").unwrap(), None);
    }

    #[test]
    fn test_flush_and_read() {
        let dir = tempdir().unwrap();
        let mut tree = LsmTree::open(dir.path().to_path_buf()).unwrap();
        let max = tree.memtable_max_bytes;
        tree.memtable_max_bytes = 100;
        for i in 0..100u32 {
            tree.put(&format!("k{:04}", i).into_bytes(), &format!("v{:04}", i).into_bytes()).unwrap();
        }
        tree.memtable_max_bytes = max;
        assert!(tree.sstable_count() > 0);
        for i in 0..100u32 {
            assert_eq!(tree.get(&format!("k{:04}", i).into_bytes()).unwrap(), Some(format!("v{:04}", i).into_bytes()));
        }
    }
}
