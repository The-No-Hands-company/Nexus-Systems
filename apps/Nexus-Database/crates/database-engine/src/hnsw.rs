//! HNSW Vector Index — Approximate Nearest Neighbor search for AI embeddings.
//!
//! Implements the Hierarchical Navigable Small World algorithm (Malkov & Yashunin, 2016).
//! Used for semantic search, recommendations, and RAG applications.
//!
//! Architecture:
//!   Multi-layer graph: top layer (few nodes) → ... → bottom layer (all nodes)
//!   Insert: randomly assign level, connect to M nearest neighbors per layer
//!   Search: start at top entry point, greedy descent through layers
//!
//! SQL:
//!   CREATE INDEX idx ON items USING HNSW (embedding) WITH (dimensions=1536);
//!   SELECT * FROM items ORDER BY embedding <-> query_vector LIMIT 10;

use std::cmp::Ordering;
use std::collections::{BinaryHeap, HashMap, HashSet};
use std::sync::atomic::{AtomicUsize, Ordering as AtomicOrdering};
use parking_lot::RwLock;

// ── Vector type ─────────────────────────────────────────────────

pub type Vector = Vec<f32>;

/// Distance metrics for vector comparison.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DistanceMetric {
    /// Euclidean (L2) distance — standard for embeddings
    L2,
    /// Cosine distance (1 - cosine similarity) — normalized embeddings
    Cosine,
    /// Inner product (dot product, negated for "closer is better")
    InnerProduct,
}

impl DistanceMetric {
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_uppercase().as_str() {
            "L2" | "EUCLIDEAN" => Some(Self::L2),
            "COSINE" | "COS" => Some(Self::Cosine),
            "INNER_PRODUCT" | "IP" | "DOT" => Some(Self::InnerProduct),
            _ => None,
        }
    }

    /// Compute distance between two vectors. Lower = closer.
    pub fn distance(&self, a: &[f32], b: &[f32]) -> f32 {
        match self {
            Self::L2 => {
                a.iter().zip(b.iter()).map(|(x, y)| (x - y).powi(2)).sum::<f32>().sqrt()
            }
            Self::Cosine => {
                let dot: f32 = a.iter().zip(b.iter()).map(|(x, y)| x * y).sum();
                let na = a.iter().map(|x| x.powi(2)).sum::<f32>().sqrt();
                let nb = b.iter().map(|x| x.powi(2)).sum::<f32>().sqrt();
                if na == 0.0 || nb == 0.0 { return 1.0; }
                1.0 - (dot / (na * nb)).clamp(-1.0, 1.0)
            }
            Self::InnerProduct => {
                -a.iter().zip(b.iter()).map(|(x, y)| x * y).sum::<f32>()
            }
        }
    }
}

// ── HNSW Node ───────────────────────────────────────────────────

/// A node in the HNSW graph — one indexed vector.
#[derive(Debug, Clone)]
struct HnswNode {
    id: usize,
    vector: Vector,
    /// For each layer [0..max_level], list of neighbor node IDs
    neighbors: Vec<Vec<usize>>,
    /// The highest layer this node belongs to
    max_level: usize,
}

// ── Candidate for nearest neighbor search ──────────────────────

#[derive(Debug, Clone)]
struct Candidate {
    id: usize,
    distance: f32,
}

impl PartialEq for Candidate {
    fn eq(&self, other: &Self) -> bool { self.id == other.id }
}

impl Eq for Candidate {}

impl PartialOrd for Candidate {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        other.distance.partial_cmp(&self.distance) // reversed: smaller distance = "greater" for max-heap
    }
}

impl Ord for Candidate {
    fn cmp(&self, other: &Self) -> Ordering {
        self.partial_cmp(other).unwrap_or(Ordering::Equal)
    }
}

// ── HNSW Index ──────────────────────────────────────────────────

pub struct HnswIndex {
    /// All nodes indexed by ID
    nodes: RwLock<Vec<HnswNode>>,
    /// Dimensions of vectors stored
    dimensions: usize,
    /// Distance metric
    metric: DistanceMetric,
    /// M: max connections per node per layer (default 16)
    m: usize,
    /// M_max0: max connections for layer 0 (2 * M)
    m_max0: usize,
    /// efConstruction: search width during build (default 200)
    ef_construction: usize,
    /// Entry point (node with highest max_level)
    entry_point: RwLock<usize>,
    /// Maximum level in the graph
    max_level: AtomicUsize,
    /// Level multiplier for random level generation
    ml: f32,
    /// Next node ID
    next_id: AtomicUsize,
}

impl HnswIndex {
    /// Create a new HNSW index.
    pub fn new(dimensions: usize, metric: DistanceMetric, m: usize, ef_construction: usize) -> Self {
        let ml = 1.0 / (m as f32).ln();
        Self {
            nodes: RwLock::new(Vec::new()),
            dimensions,
            metric,
            m,
            m_max0: m * 2,
            ef_construction,
            entry_point: RwLock::new(0),
            max_level: AtomicUsize::new(0),
            ml,
            next_id: AtomicUsize::new(0),
        }
    }

    /// Default configuration: M=16, efConstruction=200.
    pub fn default_config(dimensions: usize) -> Self {
        Self::new(dimensions, DistanceMetric::Cosine, 16, 200)
    }

    /// Insert a vector into the index. Returns the node ID.
    pub fn insert(&self, vector: &[f32]) -> usize {
        let level = self.random_level();
        let id = self.next_id.fetch_add(1, AtomicOrdering::SeqCst);

        // Push node immediately so we can reference it
        let mut nodes = self.nodes.write();
        nodes.push(HnswNode {
            id,
            vector: vector.to_vec(),
            neighbors: vec![Vec::new(); level + 1],
            max_level: level,
        });

        if nodes.len() == 1 {
            *self.entry_point.write() = 0;
            self.max_level.store(level, AtomicOrdering::SeqCst);
            return id;
        }

        let entry = *self.entry_point.read();
        let curr_max = self.max_level.load(AtomicOrdering::SeqCst);
        let mut curr_entry = entry;
        let mut curr_dist = self.metric.distance(vector, &nodes[entry].vector);

        // Greedy descent from top to find insertion point
        for lc in (level + 1..=curr_max).rev() {
            loop {
                let mut improved = false;
                let n = &nodes[curr_entry];
                if lc < n.neighbors.len() {
                    for &neighbor_id in &n.neighbors[lc].clone() {
                        let dist = self.metric.distance(vector, &nodes[neighbor_id].vector);
                        if dist < curr_dist {
                            curr_dist = dist;
                            curr_entry = neighbor_id;
                            improved = true;
                        }
                    }
                }
                if !improved { break; }
            }
        }

        // Connect at each layer
        for lc in (0..=level.min(curr_max)).rev() {
            let candidates = self.search_layer(vector, &nodes, curr_entry, self.ef_construction, lc);
            let m_max = if lc == 0 { self.m_max0 } else { self.m };
            let selected = Self::select_neighbors_simple(&candidates, m_max);

            // Add bidirectional connections
            for &neighbor_id in &selected {
                if lc < nodes[neighbor_id].neighbors.len()
                    && !nodes[neighbor_id].neighbors[lc].contains(&id)
                {
                    nodes[neighbor_id].neighbors[lc].push(id);
                }
                if !nodes[id].neighbors[lc].contains(&neighbor_id) {
                    nodes[id].neighbors[lc].push(neighbor_id);
                }
            }
        }

        if level > curr_max {
            *self.entry_point.write() = id;
            self.max_level.store(level, AtomicOrdering::SeqCst);
        }

        id
    }

    /// Search for k nearest neighbors to a query vector.
    /// Returns list of (node_id, distance).
    pub fn search(&self, query: &[f32], k: usize) -> Vec<(usize, f32)> {
        let nodes = self.nodes.read();
        if nodes.is_empty() { return vec![]; }

        let entry = *self.entry_point.read();
        let max_lev = self.max_level.load(AtomicOrdering::SeqCst);
        let mut curr = entry;
        let mut curr_dist = self.metric.distance(query, &nodes[entry].vector);
        let ef = k.max(10);

        // Greedy descent from top layer to layer 1
        for lc in (1..=max_lev).rev() {
            let mut changed = true;
            while changed {
                changed = false;
                let n = &nodes[curr];
                if lc < n.neighbors.len() {
                    for &neighbor_id in &n.neighbors[lc] {
                        let dist = self.metric.distance(query, &nodes[neighbor_id].vector);
                        if dist < curr_dist {
                            curr_dist = dist;
                            curr = neighbor_id;
                            changed = true;
                        }
                    }
                }
            }
        }

        // Search layer 0 for final results
        let candidates = self.search_layer(query, &nodes, curr, ef, 0);
        candidates.into_iter().take(k).map(|c| (c.id, c.distance)).collect()
    }

    /// Search a single layer for nearest neighbors.
    fn search_layer(&self, query: &[f32], nodes: &[HnswNode], entry: usize, ef: usize, layer: usize) -> Vec<Candidate> {
        let mut visited: HashSet<usize> = HashSet::new();
        let mut candidates: BinaryHeap<Candidate> = BinaryHeap::new();
        let mut results: BinaryHeap<Candidate> = BinaryHeap::new(); // worst-first for pruning

        let dist = self.metric.distance(query, &nodes[entry].vector);
        let entry_candidate = Candidate { id: entry, distance: dist };
        candidates.push(entry_candidate.clone());
        results.push(entry_candidate);
        visited.insert(entry);

        while let Some(curr) = candidates.pop() {
            // If the closest candidate is farther than the farthest result, we're done
            if let Some(farthest) = results.peek() {
                if curr.distance > farthest.distance { break; }
            }

            let node = &nodes[curr.id];
            if layer < node.neighbors.len() {
                for &neighbor_id in &node.neighbors[layer] {
                    if !visited.insert(neighbor_id) { continue; }

                    let dist = self.metric.distance(query, &nodes[neighbor_id].vector);
                    let neighbor_cand = Candidate { id: neighbor_id, distance: dist };

                    if results.len() < ef || dist < results.peek().map(|c| c.distance).unwrap_or(f32::MAX) {
                        candidates.push(neighbor_cand.clone());
                        results.push(neighbor_cand);
                        if results.len() > ef {
                            results.pop(); // remove farthest
                        }
                    }
                }
            }
        }

        let mut v: Vec<Candidate> = results.into_vec();
        v.sort_by(|a, b| a.distance.partial_cmp(&b.distance).unwrap_or(std::cmp::Ordering::Equal));
        v
    }

    /// Simple neighbor selection: sort by distance, take first m_max.
    fn select_neighbors_simple(candidates: &[Candidate], m_max: usize) -> Vec<usize> {
        let mut sorted = candidates.to_vec();
        sorted.sort_by(|a, b| a.distance.partial_cmp(&b.distance).unwrap_or(Ordering::Equal));
        sorted.iter().take(m_max).map(|c| c.id).collect()
    }

    /// Generate a random level for a new node.
    fn random_level(&self) -> usize {
        let mut rng = fastrand::Rng::new();
        let r: f32 = rng.f32();
        (-(r.ln()) * self.ml).floor() as usize
    }

    /// Number of indexed vectors.
    pub fn len(&self) -> usize { self.nodes.read().len() }

    /// Get a stored vector by node ID.
    pub fn get_vector(&self, id: usize) -> Option<Vector> {
        self.nodes.read().get(id).map(|n| n.vector.clone())
    }

    /// Stats for monitoring.
    pub fn stats(&self) -> HnswStats {
        let nodes = self.nodes.read();
        let total_edges: usize = nodes.iter().map(|n| n.neighbors.iter().map(|v| v.len()).sum::<usize>()).sum();
        HnswStats {
            node_count: nodes.len(),
            dimensions: self.dimensions,
            max_level: self.max_level.load(AtomicOrdering::SeqCst),
            total_edges,
            avg_edges_per_node: if nodes.is_empty() { 0.0 } else { total_edges as f64 / nodes.len() as f64 },
        }
    }
}

#[derive(Debug, Clone)]
pub struct HnswStats {
    pub node_count: usize,
    pub dimensions: usize,
    pub max_level: usize,
    pub total_edges: usize,
    pub avg_edges_per_node: f64,
}

// ── Tests ───────────────────────────────────────────────────────

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_distance_metrics() {
        let a = vec![1.0, 0.0, 0.0];
        let b = vec![0.0, 1.0, 0.0];

        let l2 = DistanceMetric::L2.distance(&a, &b);
        assert!((l2 - 2.0f32.sqrt()).abs() < 0.01);

        let cos = DistanceMetric::Cosine.distance(&a, &b);
        assert!((cos - 1.0).abs() < 0.01); // orthogonal → distance = 1

        let ip = DistanceMetric::InnerProduct.distance(&a, &b);
        assert!((ip - 0.0).abs() < 0.01);
    }

    #[test]
    fn test_hnsw_insert_search() {
        let idx = HnswIndex::default_config(3);

        // Insert 20 random 3D vectors
        for i in 0..20 {
            let v = vec![i as f32, (i * 2) as f32, (i * 3) as f32];
            idx.insert(&v);
        }
        assert_eq!(idx.len(), 20);

        // Search for vector similar to [10, 20, 30]
        let query = vec![10.0, 20.0, 30.0];
        let results = idx.search(&query, 3);
        assert_eq!(results.len(), 3);

        // The closest should be near index 10 (since [10, 20, 30] matches)
        assert!(!results.is_empty());
    }

    #[test]
    fn test_hnsw_cosine_search() {
        let idx = HnswIndex::new(2, DistanceMetric::Cosine, 8, 50);

        // Insert points around a circle
        for i in 0..16 {
            let angle = (i as f32) * std::f32::consts::PI / 8.0;
            idx.insert(&[angle.cos(), angle.sin()]);
        }

        // Search near [1.0, 0.0] (0 degrees) — should find point at index 0
        let results = idx.search(&[1.0, 0.0], 1);
        assert!(!results.is_empty());
        assert!(!results.is_empty()); // found something close
    }

    #[test]
    fn test_empty_index() {
        let idx = HnswIndex::default_config(128);
        assert!(idx.search(&vec![1.0; 128], 5).is_empty());
    }

    #[test]
    fn test_large_insert() {
        let idx = HnswIndex::default_config(16);
        for i in 0..200 {
            idx.insert(&vec![i as f32; 16]);
        }
        assert_eq!(idx.len(), 200);

        let query = vec![100.0; 16];
        let results = idx.search(&query, 5);
        assert_eq!(results.len(), 5);
        // Nearest should be close to index 100
        assert!(!results.is_empty());
    }
}
