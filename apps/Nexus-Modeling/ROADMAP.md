# Nexus Modeling — Autonomous Loop Roadmap

Priority order for the autonomous loop agent. Each item includes:
- A unique target ID
- Priority (P0=critical, P1=high, P2=medium, P3=low)
- Acceptance criteria (how to verify completion)
- Estimated difficulty

## P0 — Topology Completeness

### [P0-001] insertEdgeLoop face-splitting cross-edges ✅
- **Status**: **DONE** — `splitFacesAlongLoop` adds cross-edges between consecutive new vertices
- **File**: `src/kernel/src/geometry/HalfEdgeMesh.cpp`
- **Test**: `HEMOperations.InsertEdgeLoopOnCubeCreatesNewFaces`, `InvalidEdgeFails`
- **Verification**: `ctest -R InsertEdgeLoop --output-on-failure`

### [P0-002] splitEdge NGon generalization ✅
- **Status**: **DONE** — non-triangle faces delegate to `insertEdgeVertex`
- **File**: `src/kernel/src/geometry/HalfEdgeMesh.cpp`
- **Test**: `SplitEdgeOnQuadSucceeds`, `SplitEdgeOnTrianglePairStillWorks`
- **Verification**: `ctest -R SplitEdge --output-on-failure`

### [P0-003] collapseEdge boundary handling
- **Status**: Boundary edges can be collapsed but may create non-manifold topology
- **File**: `src/kernel/src/geometry/HalfEdgeMesh.cpp`
- **Test**: Add `CollapseEdge_OnBoundary_PreservesManifold`
- **Difficulty**: Medium
- **Verification**: New test passes + existing integrity tests pass

## P1 — Boolean & Exact Arithmetic

### [P1-001] Wire RobustPredicates into Boolean point-in-mesh
- **Status**: `RobustPredicates.h/.cpp` complete (235 lines); `BooleanOperation::pointInMesh` uses float epsilons
- **File**: `src/kernel/src/geometry/BooleanOperation.cpp`
- **Verify**: Boolean on near-degenerate meshes still produces valid output
- **Difficulty**: Low (infrastructure exists, just wiring)

### [P1-002] BVH-accelerated point-in-mesh for Boolean
- **Status**: `MeshBVH` exists; `pointInMesh` uses O(n) ray-casting per point
- **File**: `src/kernel/src/geometry/BooleanOperation.cpp`
- **Verify**: Boolean on 100k+ triangle meshes completes in reasonable time
- **Difficulty**: Medium

### [P1-003] BooleanOperation edge-splitting at intersection curves
- **Status**: Triangles kept/discarded whole; no splitting at true intersection curve
- **File**: `src/kernel/src/geometry/BooleanOperation.cpp`
- **Verify**: Boolean of intersecting cubes has clean edges at intersection line
- **Difficulty**: Very High

### [P1-004] Exact curve/surface intersection for Analytic B-rep Boolean
- **Status**: Analytic B-rep Boolean stubs exist; need exact intersection curves
- **File**: `src/kernel/src/geometry/AnalyticBRep.cpp`
- **Verify**: `booleanDifference(box, cylinder)` produces exact cylindrical hole
- **Difficulty**: High

## P2 — Half-Edge Mesh Operators

### [P2-001] BevelEdgesHEM full topology remeshing
- **Status**: Creates miter vertices + bevel strips but doesn't rewire original faces
- **File**: `src/kernel/src/geometry/HalfEdgeMesh.cpp` bevelEdgesHEM
- **Verify**: Bevel on cube edge: original faces shrink, bevel strip fills gap
- **Difficulty**: High

### [P2-002] connectVertices proper face splitting
- **Status**: Creates edge pair but doesn't split the sharing face
- **File**: `src/kernel/src/geometry/HalfEdgeMesh.cpp` connectVertices
- **Verify**: Connecting opposite vertices of a quad splits it into 2 triangles
- **Difficulty**: Medium

### [P2-003] gridFill quad topology for even loops
- **Status**: Works but produces unconnected faces; needs proper adjacency
- **File**: `src/kernel/src/geometry/HalfEdgeMesh.cpp` gridFill
- **Verify**: Grid-filled hole has valid manifold topology
- **Difficulty**: Medium

### [P2-004] CollapseFace with hole handling
- **Status**: Basic collapse works; holes (inner loops) not handled
- **File**: `src/kernel/src/geometry/HalfEdgeMesh.cpp` collapseFace
- **Verify**: Collapsing face with inner loops preserves topology
- **Difficulty**: Medium

## P3 — Module Deepening & Tests

### [P3-001] Write test for bevelEdgesHEM
- **File**: `tests/kernel/test_HEMOperations.cpp`
- **Verify**: Bevel on cube edge: vertex count increases, result is valid
- **Difficulty**: Low

### [P3-002] Write test for gridFill
- **File**: `tests/kernel/test_HEMOperations.cpp`
- **Verify**: Fill 4-vertex boundary loop produces a valid quad face
- **Difficulty**: Low

### [P3-003] Write test for connectVertices
- **File**: `tests/kernel/test_HEMOperations.cpp`
- **Verify**: Connect opposite vertices of quad: 2 triangular faces result
- **Difficulty**: Low

### [P3-004] Deepen TemplateManager
- **Status**: 9-line compressed stub with typo in namespace (`templat`)
- **File**: `src/kernel/src/template/TemplateManager.cpp`
- **Verify**: TemplateManager can serialize/deserialize template definitions
- **Difficulty**: Medium

### [P3-005] SubdivisionSurface crease weight propagation tests
- **File**: `tests/kernel/test_SubdivisionSurface.cpp`
- **Verify**: Crease weights (0=smooth, 1=sharp) preserved through subdivision levels
- **Difficulty**: Low

### [P3-006] MeshRemesh voxel/quad parameter sweep tests
- **File**: `tests/kernel/test_MeshRemesh.cpp`
- **Verify**: Remesh produces target edge length ±10%, preserves sharp features
- **Difficulty**: Medium

## P4 — Performance & Hardening

### [P4-001] Add BVH acceleration to SnappingEngine snap()
- **Status**: Uses `MeshBVH::closestPoint()` which is single-point query
- **File**: `src/kernel/src/geometry/SnappingEngine.cpp`
- **Verify**: Snap on 1M-triangle mesh completes in <1ms
- **Difficulty**: Low

### [P4-002] HalfEdgeMesh edge caching for edgeLoop/edgeRing
- **Status**: `HardSurfaceWorkflow` rebuilds HEM from mesh on every call
- **File**: `src/kernel/src/geometry/HardSurfaceWorkflow.cpp`
- **Verify**: Edge loop selection on 100k-tri mesh completes in <10ms
- **Difficulty**: Low

### [P4-003] Parallel BVH build for MeshBVH
- **Status**: SAH build is single-threaded
- **File**: `src/kernel/src/geometry/MeshBVH.cpp`
- **Verify**: BVH build on 1M triangles <50ms (8 cores)
- **Difficulty**: Medium

### [P4-004] Deterministic float regression tests
- **Status**: Perf tests exist but no bit-identical float regression
- **File**: `tests/perf/test_Determinism.cpp`
- **Verify**: Same input → bit-identical output across Debug/Release, GCC/Clang
- **Difficulty**: Medium

### [P4-005] API freeze audit: auto-generate manifest
- **Status**: Manual manifest maintenance
- **File**: `tools/api_freeze_check.py`
- **Verify**: `cmake --build build --target ApiFreezeAudit` passes without manual updates
- **Difficulty**: Medium

## P5 — Rendering & Editor Features

### [P5-001] Complete EditorUI Vulkan backend integration
- **Status**: EditorUI migrated to Vulkan but `initializeVulkan` needs `RenderContext` method exposure
- **File**: `src/kernel/src/app/EditorUI.cpp`, `src/kernel/include/nexus/gfx/RenderContext.h`
- **Verify**: ImGui renders correctly via Vulkan backend (not OpenGL)
- **Difficulty**: Medium

### [P5-002] Interactive window display fix
- **Status**: Headless `--shot` works; interactive window appears in taskbar but closes immediately
- **File**: `app/main.cpp`, `src/kernel/src/app/Viewport.cpp`
- **Verify**: `./build/src/kernel/nexus_modeling` opens persistent window on Wayland/X11
- **Difficulty**: Medium

### [P5-003] Selection highlight (yellow outline) implementation
- **Status**: Hover pre-highlight (pale cyan) works; selection highlight missing
- **File**: `src/kernel/src/app/SelectionHighlight.cpp` (new)
- **Verify**: Selected objects show yellow outline in viewport
- **Difficulty**: Low

### [P5-004] Transform gizmo (translate/rotate/scale)
- **Status**: `TransformGizmo.h/.cpp` exist but not fully integrated
- **File**: `src/kernel/src/app/TransformGizmo.cpp`
- **Verify**: Gizmo appears on selection, handles mouse drag correctly
- **Difficulty**: Medium

### [P5-005] Viewport grid and axes widgets
- **Status**: `ViewportGrid.h/.cpp`, `ViewportAxes.h/.cpp` exist
- **File**: `src/kernel/src/app/ViewportGrid.cpp`, `src/kernel/src/app/ViewportAxes.cpp`
- **Verify**: Grid renders with proper scale, axes show orientation
- **Difficulty**: Low

## P6 — CAD & Parametric Features

### [P6-001] CadAutoConstraintSketch full constraint solver
- **Status**: Sketch exists, solver integration incomplete
- **File**: `src/kernel/src/cad/CadAutoConstraintSketch.cpp`
- **Verify**: Sketch with 10+ constraints solves in <10ms
- **Difficulty**: High

### [P6-002] Extrude feature with draft/taper
- **Status**: Basic extrude works; draft angle missing
- **File**: `src/kernel/src/cad/CadFeature.cpp` (ExtrudeFeature)
- **Verify**: Extrude with 5° draft produces tapered walls
- **Difficulty**: Medium

### [P6-003] Revolve feature with axis selection
- **Status**: Basic revolve works; axis from sketch plane missing
- **File**: `src/kernel/src/cad/CadFeature.cpp` (RevolveFeature)
- **Verify**: Revolve around selected edge produces correct solid
- **Difficulty**: Medium

### [P6-004] Fillet/Chamfer feature (Analytic B-rep)
- **Status**: Analytic B-rep has exact fillet stub; mesh fillet missing
- **File**: `src/kernel/src/geometry/AnalyticBRep.cpp`, `src/kernel/src/cad/FilletFeature.cpp`
- **Verify**: Fillet on box edge produces exact cylindrical face
- **Difficulty**: Very High

## P7 — Documentation & Infrastructure

### [P7-001] Regenerate HTML documentation site
- **Status**: 57 Markdown files exist; HTML generation script needed
- **File**: `docs/html/`
- **Verify**: `docs/html/developer/index.html` loads all developer docs
- **Difficulty**: Low

### [P7-002] Expert user guides: advanced-modeling, cad-workflows, scripting, pipeline
- **File**: `docs/expert/`
- **Verify**: All 4 guides exist with cross-references
- **Difficulty**: Medium

### [P7-003] Beginner tutorials: getting-started, interface-tour, first-model, navigation
- **File**: `docs/beginner/`
- **Verify**: All 4 tutorials complete with screenshots
- **Difficulty**: Medium

### [P7-004] API reference: kernel, render, editor, scripting
- **File**: `docs/api/`
- **Verify**: Doxygen-style API docs generated from headers
- **Difficulty**: Medium

## Completion Criteria

- All P0 items: ✅ before first release
- All P1 items: ✅ before beta
- P2 items: 3 of 3 ✅ before beta
- P3 items: all 6 ✅ before beta
- P4 items: at least 3 of 5 ✅
- P5 items: at least 3 of 5 ✅ before 1.0
- P6 items: at least 2 of 4 ✅ before 1.0
- P7 items: all 4 ✅ before 1.0

---

*Roadmap is living — update in same PR as implementation work.*