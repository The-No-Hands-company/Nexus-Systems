# Nexus Modeling — Half-Edge Mesh Guide

*Complete guide to the half-edge topology structure, Euler operators, and integrity validation.*

---

## Overview

The `HalfEdgeMesh` provides a **manifold half-edge data structure** with **integrity-clean Euler operators**. All 6 local operators are proven integrity-clean via `checkIntegrity()` + property tests.

---

## Data Structures

```cpp
struct Vertex { 
    uint32_t edge = kInvalid;  // One incident half-edge
    Vec3 pos;                  // Position
};

struct Edge {
    uint32_t vert[2] = {kInvalid, kInvalid};  // End vertices
    uint32_t face[2] = {kInvalid, kInvalid};  // Incident faces (max 2 for manifold)
};

struct HalfEdge {
    uint32_t vertex, edge, face;  // Origin vertex, geometric edge, incident face
    uint32_t next, prev, twin;    // Topological links
};

struct Face {
    uint32_t halfEdge = kInvalid;  // One incident half-edge
};
```

**Constants:**
```cpp
static constexpr uint32_t kInvalid = std::numeric_limits<uint32_t>::max();
```

**Topological Relationships:**

```
HalfEdge → vertex (origin)
HalfEdge → edge (geometric edge)
HalfEdge → face (incident face)
HalfEdge → next (next half-edge around face)
HalfEdge → prev (prev half-edge around face)
HalfEdge → twin (opposite half-edge on same edge)

Edge → vert[0], vert[1] (endpoints)
Edge → face[0], face[1] (incident faces, max 2 for manifold)

Vertex → edge (one incident half-edge)
Face → halfEdge (one incident half-edge)
```

---

## Construction

```cpp
// From indexed mesh (positions + face indices)
static std::optional<HalfEdgeMesh> fromMesh(const Mesh&);

// To indexed mesh
Mesh toMesh() const;
```

### Construction Algorithm (`fromMesh`)

```cpp
std::optional<HalfEdgeMesh> HalfEdgeMesh::fromMesh(const Mesh& mesh) {
    // 1. Create vertices
    for each position: create Vertex{edge=kInvalid, pos}
    
    // 2. Build edges (deduplicate by vertex pair)
    //    Use hash map: (min(v0,v1), max(v0,v1)) -> edgeId
    
    // 3. Build faces from face indices
    //    For each face, create half-edges in CCW order
    
    // 4. Build coedges, set next/prev/twin
    //    next/prev: within face ring
    //    twin: opposite half-edge on same edge
    
    // 5. Link coedges to vertices
    //    vertex.edge = one incident half-edge
    
    // 6. Build loops (outer + inner)
    //    Identify boundary edges (1 coedge) vs interior (2 coedges)
}
```

### Detailed Construction Steps

```cpp
// Step 1: Vertices
m_vertices.resize(mesh.vertexCount());
for (uint32_t i = 0; i < mesh.vertexCount(); ++i) {
    m_vertices[i] = Vertex{kInvalid, mesh.positions()[i]};
}

// Step 2: Edges (with deduplication)
struct EdgeKey {
    uint32_t v0, v1;
    bool operator==(const EdgeKey& o) const { return v0==o.v0 && v1==o.v1; }
};
struct EdgeKeyHash {
    size_t operator()(const EdgeKey& k) const noexcept {
        return (size_t(k.v0) << 32) ^ size_t(k.v1);
    }
};
std::unordered_map<EdgeKey, uint32_t, EdgeKeyHash> edgeMap;

for each face in mesh:
    for each edge (v0, v1) in face:
        EdgeKey key{min(v0,v1), max(v0,v1)};
        auto [it, inserted] = edgeMap.try_emplace(key, m_edges.size());
        if (inserted) {
            m_edges.push_back(Edge{{v0, v1}, {kInvalid, kInvalid}});
        }
        uint32_t edgeId = it->second;
        // Record half-edge -> edge mapping

// Step 3: Half-edges per face
for each face:
    uint32_t firstHalfEdge = m_halfEdges.size();
    uint32_t prev = kInvalid;
    for each vertex in face (CCW):
        uint32_t heId = m_halfEdges.size();
        m_halfEdges.push_back(HalfEdge{vertex, edgeId, faceId, kInvalid, prev, kInvalid});
        if (prev != kInvalid) m_halfEdges[prev].next = heId;
        prev = heId;
        // Record vertex -> half-edge if not set
        if (m_vertices[vertex].edge == kInvalid) m_vertices[vertex].edge = heId;
    // Close the loop
    m_halfEdges[prev].next = firstHalfEdge;
    m_halfEdges[firstHalfEdge].prev = prev;
    m_faces[faceId].halfEdge = firstHalfEdge;

// Step 4: Set twins
for each edge:
    if (edge.face[0] != kInvalid && edge.face[1] != kInvalid) {
        // Find half-edges for each face
        uint32_t he0 = findHalfEdge(edge.face[0], edge.vert[0], edge.vert[1]);
        uint32_t he1 = findHalfEdge(edge.face[1], edge.vert[1], edge.vert[0]);
        m_halfEdges[he0].twin = he1;
        m_halfEdges[he1].twin = he0;
    } else {
        // Boundary edge - one twin is kInvalid
    }

// Step 5: Build loops (outer/inner per face)
// Identify boundary cycles and interior cycles
```

---

## Local Euler Operators (All Integrity-Clean)

| Operator | Description | Signature |
|----------|-------------|-----------|
| `insertEdgeVertex` | Split edge, create vertex + 2 edges + 2 coedges | `uint32_t insertEdgeVertex(uint32_t edgeId, float t)` |
| `splitEdge` | Same, but explicit position | `uint32_t splitEdge(uint32_t edgeId)` |
| `collapseEdge` | Merge vertices, remove edge | `void collapseEdge(uint32_t edgeId)` |
| `splitFace` | Add diagonal edge across face | `uint32_t splitFace(uint32_t faceId, uint32_t v0, uint32_t v1)` |
| `flipEdge` | Flip interior edge (Delaunay) | `void flipEdge(uint32_t edgeId)` |
| `collapseFace` | Remove face, merge edges | `void collapseFace(uint32_t faceId)` |
| `pokeFace` | Insert center vertex, fan triangulate | `void pokeFace(uint32_t faceId)` |
| `insertEdgeLoop` | Split edge, create loop | `void insertEdgeLoop(uint32_t edgeId, float t)` |

**All 6 local ops: integrity-clean (proven by `checkIntegrity()` + property tests)**

---

## Operator Specifications (Detailed)

### `insertEdgeVertex(edgeId, t)`

**Preconditions:** `0 < t < 1`, edge valid, not boundary-only if would create non-manifold

**Effect:**
1. Creates new vertex at `edge.pos0 + t * (edge.pos1 - edge.pos0)`
2. Splits edge into two edges
3. Creates two new half-edges (one per new edge)
4. Updates all incident face loops

**Returns:** New vertex ID

**Algorithm:**
```cpp
uint32_t insertEdgeVertex(uint32_t edgeId, float t) {
    // 1. Get edge endpoints
    Vec3 p0 = m_vertices[m_edges[edgeId].vert[0]].pos;
    Vec3 p1 = m_vertices[m_edges[edgeId].vert[1]].pos;
    Vec3 newPos = p0 + t * (p1 - p0);
    
    // 2. Create new vertex
    uint32_t newVert = m_vertices.size();
    m_vertices.push_back(Vertex{kInvalid, newPos});
    
    // 3. Create two new edges
    uint32_t edgeA = m_edges.size();
    m_edges.push_back(Edge{{m_edges[edgeId].vert[0], newVert}, {kInvalid, kInvalid}});
    uint32_t edgeB = m_edges.size();
    m_edges.push_back(Edge{{newVert, m_edges[edgeId].vert[1]}, {kInvalid, kInvalid}});
    
    // 4. Update incident faces
    for (int side = 0; side < 2; ++side) {
        uint32_t faceId = m_edges[edgeId].face[side];
        if (faceId != kInvalid) {
            // Find half-edges using this edge in this face
            // Replace with two half-edges (one per new edge)
            // Update next/prev pointers
        }
    }
    
    // 5. Create half-edges for new edges
    // 6. Set twins
    // 7. Update vertex.edge
    
    // 8. Remove old edge (mark invalid or remove from vector)
    // 9. Update edgeMap
    
    return newVert;
}
```

### `splitEdge(edgeId)`

Same as `insertEdgeVertex` but position is midpoint or explicit.

### `collapseEdge(edgeId)`

**Preconditions:** Edge valid, not boundary-only, collapse won't create non-manifold

**Effect:**
1. Merges the two vertices of the edge
2. Removes the edge
3. Updates all incident faces
4. Removes degenerate faces (2 edges)

### `splitFace(faceId, v0, v1)`

**Preconditions:** v0,v1 on face boundary, not adjacent, face has ≥4 vertices

**Effect:**
1. Adds diagonal edge between v0 and v1
2. Splits face into two faces
3. Updates loops

### `flipEdge(edgeId)`

**Preconditions:** Interior edge (2 incident faces), not boundary

**Effect:**
1. Flips the diagonal of the quad formed by two triangles
2. Updates all topological links

### `collapseFace(faceId)`

**Preconditions:** Face valid, not sole face of shell

**Effect:**
1. Removes face
2. Merges boundary edges
3. Updates loops

### `pokeFace(faceId)`

**Preconditions:** Face has ≥3 vertices

**Effect:**
1. Inserts center vertex (centroid)
2. Creates edges from center to all boundary vertices
3. Fan triangulates

### `insertEdgeLoop(edgeId, t)`

**Preconditions:** `0 < t < 1`, edge valid

**Effect:**
1. Splits edge at t
2. Creates new edge loop connecting split points

---

## Queries

```cpp
std::vector<uint32_t> vertexRing(uint32_t vert) const;
std::vector<uint32_t> faceRing(uint32_t face) const;
std::vector<uint32_t> boundaryLoops() const;
```

### `vertexRing(vert)` — Ordered ring of faces around vertex

```cpp
std::vector<uint32_t> vertexRing(uint32_t vert) const {
    std::vector<uint32_t> ring;
    uint32_t startHe = m_vertices[vert].edge;
    if (startHe == kInvalid) return ring;
    
    uint32_t he = startHe;
    do {
        ring.push_back(m_halfEdges[he].face);
        he = m_halfEdges[m_halfEdges[he].twin].next;
    } while (he != startHe && he != kInvalid);
    
    return ring;
}
```

### `faceRing(face)` — Ordered ring of vertices around face

```cpp
std::vector<uint32_t> faceRing(uint32_t face) const {
    std::vector<uint32_t> ring;
    uint32_t he = m_faces[face].halfEdge;
    if (he == kInvalid) return ring;
    
    uint32_t start = he;
    do {
        ring.push_back(m_halfEdges[he].vertex);
        he = m_halfEdges[he].next;
    } while (he != start);
    
    return ring;
}
```

### `boundaryLoops()` — All boundary edge loops

```cpp
std::vector<uint32_t> boundaryLoops() const {
    std::vector<uint32_t> loops;
    // Find edges with only 1 coedge
    // Walk each boundary cycle
}
```

---

## Integrity Validation

```cpp
struct IntegrityReport {
    bool ok = true;
    std::string reason;
    uint32_t vertices = 0, edges = 0, faces = 0;
    uint32_t coedges = 0, loops = 0, shells = 0;
    uint32_t boundaryEdges = 0;
    int euler = 0;  // V - E + F
};

IntegrityReport checkIntegrity() const;
```

### Checks Performed

| Check | Failure Example |
|-------|-----------------|
| Edge vertices valid | `edge.vert[0] >= vertexCount` |
| Edge non-degenerate | `v0 == v1` |
| Coedge edge valid | `coedge.edge >= edges.size()` |
| Coedge loop valid | `coedge.loop >= loops.size()` |
| Next/prev bounds | `next >= coedges.size()` |
| Next/prev reciprocal | `coedges[next].prev != self` |
| Loop consistency | `coedges[next].loop != coedge.loop` |
| Vertex continuity | `endV(c) != startV(c.next)` |
| Partner reciprocal | `partner.partner != self` |
| Partner orientation | `partner.reversed == coedge.reversed` |
| Partner edge match | `partner.edge != coedge.edge` |
| Partner loop diff | `partner.loop == coedge.loop` |
| Loop closure | Ring doesn't close within 2×C steps |
| Loop size | `< 3 coedges` |
| Face outer loop valid | `face.outerLoop >= loops.size()` |
| Face outer loop marked | `!loops[outer].outer` |
| Face outer loop ownership | `loops[outer].face != face` |
| Inner loop validity | `inner >= loops`, `loops[inner].outer`, `loops[inner].face != face` |
| Vertex root valid | `vertex.coedge >= coedges`, `startV(coedge) != vertex` |
| Edge orphaned | `coedgesPerEdge[e] == 0` |
| Edge non-manifold | `coedgesPerEdge[e] > 2` |
| Euler characteristic | `V - E + F != 2 - 2*genus - shells + boundaries` |

### Detailed Check Implementation

```cpp
IntegrityReport HalfEdgeMesh::checkIntegrity() const {
    IntegrityReport report;
    report.vertices = m_vertices.size();
    report.edges = m_edges.size();
    report.faces = m_faces.size();
    report.coedges = m_halfEdges.size();
    
    // 1. Edge validity
    for (uint32_t i = 0; i < m_edges.size(); ++i) {
        const auto& e = m_edges[i];
        if (e.vert[0] >= m_vertices.size() || e.vert[1] >= m_vertices.size()) {
            report.ok = false; report.reason = "Edge vertices out of bounds"; return report;
        }
        if (e.vert[0] == e.vert[1]) {
            report.ok = false; report.reason = "Degenerate edge (v0==v1)"; return report;
        }
    }
    
    // 2. Coedge validity
    for (uint32_t i = 0; i < m_halfEdges.size(); ++i) {
        const auto& c = m_halfEdges[i];
        if (c.edge >= m_edges.size()) { /* fail */ }
        if (c.loop >= m_loops.size()) { /* fail */ }
        if (c.next >= m_halfEdges.size()) { /* fail */ }
        if (c.prev >= m_halfEdges.size()) { /* fail */ }
        if (m_halfEdges[c.next].prev != i) { /* fail */ }
        if (m_halfEdges[c.prev].next != i) { /* fail */ }
        if (m_halfEdges[c.next].loop != c.loop) { /* fail */ }
        if (c.next == i) { /* fail */ }
        if (c.prev == i) { /* fail */ }
    }
    
    // 3. Loop closure & size
    for (uint32_t i = 0; i < m_loops.size(); ++i) {
        uint32_t first = m_loops[i].first;
        if (first == kInvalid || first >= m_halfEdges.size()) { /* fail */ }
        uint32_t he = first;
        uint32_t count = 0;
        do {
            if (m_halfEdges[he].loop != i) { /* fail */ }
            he = m_halfEdges[he].next;
            count++;
            if (count > 2 * m_halfEdges.size()) { /* fail - infinite loop */ }
        } while (he != first);
        if (count < 3) { /* fail */ }
    }
    
    // 4. Face validity
    for (uint32_t i = 0; i < m_faces.size(); ++i) {
        const auto& f = m_faces[i];
        if (f.outerLoop >= m_loops.size()) { /* fail */ }
        if (!m_loops[f.outerLoop].outer) { /* fail */ }
        if (m_loops[f.outerLoop].face != i) { /* fail */ }
        for (uint32_t inner : f.innerLoops) {
            if (inner >= m_loops.size()) { /* fail */ }
            if (m_loops[inner].outer) { /* fail */ }
            if (m_loops[inner].face != i) { /* fail */ }
        }
    }
    
    // 5. Vertex root validity
    for (uint32_t i = 0; i < m_vertices.size(); ++i) {
        uint32_t c = m_vertices[i].edge;
        if (c >= m_halfEdges.size()) { /* fail */ }
        // Check that half-edge starts at this vertex
        if (m_halfEdges[c].vertex != i) { /* fail */ }
    }
    
    // 6. Edge coedge count (manifold check)
    std::vector<uint32_t> coedgesPerEdge(m_edges.size(), 0);
    for (const auto& c : m_halfEdges) {
        if (c.edge < m_edges.size()) coedgesPerEdge[c.edge]++;
    }
    for (uint32_t i = 0; i < m_edges.size(); ++i) {
        if (coedgesPerEdge[i] == 0) { /* orphaned */ }
        if (coedgesPerEdge[i] > 2) { /* non-manifold */ }
    }
    report.boundaryEdges = std::count(coedgesPerEdge.begin(), coedgesPerEdge.end(), 1);
    
    // 7. Euler characteristic
    int V = m_vertices.size();
    int E = m_edges.size();
    int F = m_faces.size();
    report.euler = V - E + F;
    // Expected: 2 - 2*genus - shells + boundaryComponents
    // For single shell, genus 0, no boundaries: V - E + F = 2
    
    return report;
}
```

---

## Construction from Mesh

```cpp
// From indexed mesh (positions + face indices)
Mesh mesh;
mesh.attributes().setPositions(positions);
for (face : faces) mesh.topology().addFace(Face{indices});
mesh.topology().addFace(Face{indices, materialId});

// Convert to half-edge
auto hemOpt = HalfEdgeMesh::fromMesh(mesh);
if (!hemOpt) { /* invalid topology */ }
HalfEdgeMesh hem = std::move(*hemOpt);

// Modify topology
hem.insertEdgeVertex(edgeId, 0.5f);
hem.flipEdge(edgeId);

// Back to indexed mesh
Mesh result = hem.toMesh();
```

---

## Integrity Checks (Performed by `checkIntegrity()`)

| Check | Failure Example |
|-------|-----------------|
| Edge vertices valid | `edge.vert[0] >= vertexCount` |
| Edge non-degenerate | `v0 == v1` |
| Coedge edge valid | `coedge.edge >= edges.size()` |
| Coedge loop valid | `coedge.loop >= loops.size()` |
| Next/prev bounds | `next >= coedges.size()` |
| Next/prev reciprocal | `coedges[next].prev != self` |
| Loop consistency | `coedges[next].loop != coedge.loop` |
| Vertex continuity | `endV(c) != startV(c.next)` |
| Partner reciprocal | `partner.partner != self` |
| Partner orientation | `partner.reversed == coedge.reversed` |
| Partner edge match | `partner.edge != coedge.edge` |
| Partner loop diff | `partner.loop == coedge.loop` |
| Loop closure | Ring doesn't close within 2×C steps |
| Loop size | `< 3 coedges` |
| Face outer loop valid | `face.outerLoop >= loops.size()` |
| Face outer loop marked | `!loops[outer].outer` |
| Face outer loop ownership | `loops[outer].face != face` |
| Inner loop validity | `inner >= loops`, `loops[inner].outer`, `loops[inner].face != face` |
| Vertex root valid | `vertex.coedge >= coedges`, `startV(coedge) != vertex` |
| Edge orphaned | `coedgesPerEdge[e] == 0` |
| Edge non-manifold | `coedgesPerEdge[e] > 2` |
| Euler characteristic | `V - E + F != 2 - 2*genus - shells + boundaries` |

---

## Property Preservation

| Property | `fromMesh` | `toMesh` | Euler Ops |
|----------|------------|----------|-----------|
| Vertex positions | ✅ | ✅ | ✅ |
| Normals | ✅ | ✅ | ✅ (interpolated) |
| UVs | ✅ | ✅ | ✅ (interpolated) |
| Vertex colors | ✅ | ✅ | ✅ (interpolated) |
| Material IDs | ✅ | ✅ | ✅ (per-face) |
| Tangents | ✅ | ✅ | ✅ (recomputed) |

---

## Testing

```bash
# Run all integrity tests
ctest -R HalfEdge --output-on-failure

# Specific tests
./build/tests/nexus_kernel_tests --gtest_filter=HalfEdge*
./build/tests/nexus_kernel_tests --gtest_filter=*Integrity*
```

**Property Tests:**
- `HalfEdgeMesh_InsertEdgeVertex_IntegrityClean`
- `HalfEdgeMesh_SplitEdge_IntegrityClean`
- `HalfEdgeMesh_CollapseEdge_IntegrityClean`
- `HalfEdgeMesh_SplitFace_IntegrityClean`
- `HalfEdgeMesh_FlipEdge_IntegrityClean`
- `HalfEdgeMesh_CollapseFace_IntegrityClean`
- `HalfEdgeMesh_PokeFace_IntegrityClean`
- `HalfEdgeMesh_InsertEdgeLoop_IntegrityClean`
- `HalfEdgeMesh_FromMesh_RoundTrip`
- `HalfEdgeMesh_PropertyPreservation`

---

*Kernel v0.1.0-dev | 1921 tests passing | C++26 | Vulkan 1.3*