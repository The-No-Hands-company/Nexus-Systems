# Nexus Modeling — Autonomous Loop Roadmap

Priority order for the autonomous loop agent. Each item includes:
- A unique target ID
- Priority (P0=critical, P1=high, P2=medium, P3=low)
- Acceptance criteria (how to verify completion)
- Estimated difficulty

## P0 — Topology Completeness

### [P0-001] insertEdgeLoop face-splitting cross-edges ✅
- Status: **DONE** — `splitFacesAlongLoop` adds cross-edges between consecutive new vertices
- File: `src/kernel/src/geometry/HalfEdgeMesh.cpp`
- Test: `HEMOperations.InsertEdgeLoopOnCubeCreatesNewFaces`, `InvalidEdgeFails`

### [P0-002] splitEdge NGon generalization ✅
- Status: **DONE** — non-triangle faces delegate to `insertEdgeVertex`
- File: `src/kernel/src/geometry/HalfEdgeMesh.cpp`
- Test: `SplitEdgeOnQuadSucceeds`, `SplitEdgeOnTrianglePairStillWorks`

## P1 — Boolean & Exact Arithmetic

### [P1-001] Wire RobustPredicates into Boolean point-in-mesh
- Status: `RobustPredicates.h/.cpp` complete (235 lines); `BooleanOperation::pointInMesh` uses float epsilons
- File: `src/kernel/src/geometry/BooleanOperation.cpp`
- Verify: Boolean on near-degenerate meshes still produces valid output
- Difficulty: low (infrastructure exists, just wiring)

### [P1-002] BVH-accelerated point-in-mesh for Boolean
- Status: `MeshBVH` exists; `pointInMesh` uses O(n) ray-casting per point
- File: `src/kernel/src/geometry/BooleanOperation.cpp`
- Verify: Boolean on 100k+ triangle meshes completes in reasonable time
- Difficulty: medium

### [P1-003] BooleanOperation edge-splitting at intersection curves
- Status: triangles kept/discarded whole; no splitting at true intersection curve
- File: `src/kernel/src/geometry/BooleanOperation.cpp`
- Verify: Boolean of intersecting cubes has clean edges at intersection line
- Difficulty: very high

## P2 — Half-Edge Mesh Operators

### [P2-001] BevelEdgesHEM full topology remeshing
- Status: creates miter vertices + bevel strips but doesn't rewire original faces
- File: `src/kernel/src/geometry/HalfEdgeMesh.cpp` bevelEdgesHEM
- Verify: Bevel on cube edge: original faces shrink, bevel strip fills gap
- Difficulty: high

### [P2-002] connectVertices proper face splitting
- Status: creates edge pair but doesn't split the sharing face
- File: `src/kernel/src/geometry/HalfEdgeMesh.cpp` connectVertices
- Verify: Connecting opposite vertices of a quad splits it into 2 triangles
- Difficulty: medium

### [P2-003] gridFill quad topology for even loops
- Status: works but produces unconnected faces; needs proper adjacency
- File: `src/kernel/src/geometry/HalfEdgeMesh.cpp` gridFill
- Verify: Grid-filled hole has valid manifold topology
- Difficulty: medium

## P3 — Module Deepening & Tests

### [P3-001] Write test for bevelEdgesHEM
- File: `tests/kernel/test_HEMOperations.cpp`
- Verify: bevel on cube edge: vertex count increases, result is valid
- Difficulty: low

### [P3-002] Write test for gridFill
- File: `tests/kernel/test_HEMOperations.cpp`
- Verify: Fill 4-vertex boundary loop produces a valid quad face
- Difficulty: low

### [P3-003] Write test for connectVertices
- File: `tests/kernel/test_HEMOperations.cpp`
- Verify: Connect opposite vertices of quad: 2 triangular faces result
- Difficulty: low

### [P3-004] Deepen TemplateManager
- Status: 9-line compressed stub with typo in namespace (`templat`)
- File: `src/kernel/src/template/TemplateManager.cpp`
- Verify: TemplateManager can serialize/deserialize template definitions
- Difficulty: medium

## P4 — Performance & Hardening

### [P4-001] Add BVH acceleration to SnappingEngine snap()
- Status: uses `MeshBVH::closestPoint()` which is single-point query
- File: `src/kernel/src/geometry/SnappingEngine.cpp`
- Verify: Snap on 1M-triangle mesh completes in <1ms
- Difficulty: low

### [P4-002] HalfEdgeMesh edge caching for edgeLoop/edgeRing
- Status: `HardSurfaceWorkflow` rebuilds HEM from mesh on every call
- File: `src/kernel/src/geometry/HardSurfaceWorkflow.cpp`
- Verify: Edge loop selection on 100k-tri mesh completes in <10ms
- Difficulty: low

## Completion Criteria
- All P0 items: ✓ before first release
- All P1 items: ✓ before beta
- P2 items: 3 of 3 ✓ before beta  
- P3 items: all 4 ✓ before beta
- P4 items: at least 1 ✓
