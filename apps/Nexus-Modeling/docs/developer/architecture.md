# Nexus Modeling — Architecture Overview

## System Overview

```
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                              nexus_modeling (App Executable)                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  ┌────────────────────┐  │
│  │  EditorUI    │  │  Viewport    │  │  ModelingApplication │  │  AppContext        │  │
│  │  (ImGui+Vk)  │  │  (Vulkan)    │  │  (Orchestrator)      │  │  (Dependency DI)   │  │
│  └──────┬───────┘  └──────┬───────┘  └──────────┬───────────┘  └────────┬───────────┘  │
└─────────┼─────────────────┼─────────────────────┼─────────────────────┼────────────────┘
          │                 │                     │                     │
          ▼                 ▼                     ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                   nexus::app (Library)                                  │
│  ┌─────────────────┐ ┌──────────────┐ ┌────────────┐ ┌─────────────┐ ┌─────────────┐  │
│  │ AppMode         │ │ ViewportCtrl │ │ Transform  │ │ Selection   │ │ Command     │  │
│  │ (State Pattern) │ │ (Camera)     │ │ Gizmo      │ │ Manager     │ │ Dispatcher  │  │
│  └─────────────────┘ └──────────────┘ └────────────┘ └─────────────┘ └─────────────┘  │
│  ┌─────────────────┐ ┌──────────────┐ ┌────────────┐ ┌─────────────┐ ┌─────────────┐  │
│  │ ModeOrchestrator│ │ ModeRegistry │ │ Workspace  │ │ ToolSystem  │ │ AssetBrowser│  │
│  │ (Mode switching)│ │ (Discovery)  │ │ Manager    │ │ (Input map) │ │ (Integration)│  │
│  └─────────────────┘ └──────────────┘ └────────────┘ └─────────────┘ └─────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────┘
          │                 │                     │
          ▼                 ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                    nexus::cad (Library)                                 │
│  ┌─────────────────┐ ┌──────────────┐ ┌────────────┐ ┌─────────────┐ ┌─────────────┐  │
│  │ CadDocument     │ │ CadSelection │ │ CadFeature │ │ CadCommand  │ │ CadViewer   │  │
│  │ (Feature hist)  │ │ (Multi-level)│ │ (Tree/DAG) │ │ (Undo/Redo) │ │ (Visual)    │  │
│  └─────────────────┘ └──────────────┘ └────────────┘ └─────────────┘ └─────────────┘  │
│  ┌─────────────────┐ ┌──────────────┐ ┌────────────┐ ┌─────────────┐ ┌─────────────┐  │
│  │ CadSketch       │ │ CadConstraint│ │ CadSolver  │ │ FeatureFactory│ │ Export/Import│ │
│  │ (2D constraint) │ │ (11 types)   │ │ (Parallel) │ │ (Registry)  │ │ (STEP/USD)  │  │
│  └─────────────────┘ └──────────────┘ └────────────┘ └─────────────┘ └─────────────┘  │
└─────────────────────────────────────────────────────────────────────────────────────────┘
          │                 │                     │
          ▼                 ▼                     ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                 nexus::geometry (Kernel)                                │
│  ┌────────────┐ ┌────────────┐ ┌──────────┐ ┌────────────┐ ┌────────────┐ ┌──────────┐ │
│  │ B-Rep      │ │ Half-Edge  │ │ Mesh     │ │ NURBS      │ │ Boolean    │ │ Remesh   │ │
│  │ (Analytic) │ │ Mesh       │ │ Ops      │ │ (Curve/Surf)│ │ (CSG)      │ │ (Quad/Vox)│ │
│  └────────────┘ └────────────┘ └──────────┘ └────────────┘ └────────────┘ └──────────┘ │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────┐ │
│  │ Subdiv     │ │ Decimator  │ │ Laplacian  │ │ Delaunay   │ │ Tolerance  │ │ Robust   │ │
│  │ (Catmull)  │ │ (Quadric)  │ │ (Smooth)   │ │ (2D/3D)    │ │ System     │ │ Predicates│ │
│  └────────────┘ └────────────┘ └────────────┘ └────────────┘ └────────────┘ └──────────┘ │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌──────────┐ │
│  │ Analytic   │ │ MeshBVH    │ │ MeshIO     │ │ Primitives │ │ Serialization│ │ SceneGraph│ │
│  │ Primitives │ │ (SAH)      │ │ (OBJ/STL/  │ │ (Box/Cyl/  │ │ (.nxm/glTF/│ │ (CPU)    │ │
│  │ (6 types)  │ │            │ │  glTF)     │ │  Sphere...)│ │  USD)      │ │          │ │
│  └────────────┘ └────────────┘ └────────────┘ └────────────┘ └────────────┘ └──────────┘ │
└─────────────────────────────────────────────────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                                 nexus::gfx (Graphics Abstraction)                       │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────┐  │
│  │ Device     │ │ Swapchain  │ │ FrameSched │ │ CmdBuffer  │ │ Pipeline   │ │ Buffer │  │
│  │ (Vk/Null)  │ │ (Vk/Null)  │ │ (Acquire/  │ │ (Dynamic   │ │ (Gfx/Comp/ │ │ Texture│  │
│  └────────────┘ │            │ │  Record/   │ │  rendering)│ │  RT)       │ │        │  │
│  ┌────────────┐ │            │ │  Submit/   │ │            │ │            │ │        │  │
│  │ Descriptor │ │            │ │  Present)  │ │            │ │            │ │        │  │
│  │ Set        │ │            │ │            │ │            │ │            │ │        │  │
│  └────────────┘ │            │ └────────────┘ └────────────┘ └────────────┘ └────────┘  │
│  ┌────────────┐ │            ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────┐    │
│  │ Shader     │ │            │ Renderer   │ │ RenderCtx  │ │ FrameCap   │ │ Alloc   │    │
│  │ (SPIR-V)   │ │            │ (Deferred) │ │ (High-level)│ │ Exporter   │ │ (VMA)  │    │
│  └────────────┘ │            └────────────┘ └────────────┘ └────────────┘ └────────┘    │
│  ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐ ┌────────────┐              │
│  │ Vulkan/    │ │ Null       │ │ Types      │ │ Sync       │ │ Debug      │              │
│  │ Backend    │ │ Backend    │ │ (Handles)  │ │ (Barriers) │ │ (Validation)│              │
│  └────────────┘ └────────────┘ └────────────┘ └────────────┘ └────────────┘              │
└─────────────────────────────────────────────────────────────────────────────────────────┘
```

---

## Core Design Principles

### 1. Kernel-First Architecture
The geometry kernel (`nexus::geometry`) is a **standalone library** with **zero dependencies** on UI, rendering, or application logic. It can be used headless for:
- Batch processing pipelines
- CLI tools
- Server-side geometry processing
- CI/CD validation

### 2. Layered Architecture (Strict Dependency Direction)
```
App (nexus_modeling) 
  → App Framework (nexus::app) 
    → CAD Layer (nexus::cad) 
      → Geometry Kernel (nexus::geometry) 
        → Graphics Abstraction (nexus::gfx) ← Backend implementations
```

**Rule:** Each layer ONLY depends on layers below it. No upward or circular dependencies.

### 3. Data-Oriented Design
- **Struct of Arrays (SoA)** over Array of Structs (AoS) for cache efficiency
- **Handle-based** resource management (no raw pointers across module boundaries)
- **Slot maps / generational indices** for stable IDs with O(1) lookup
- **Arena allocators** for transient geometry, VMA for GPU memory

### 4. Immutability by Default
- Geometry operations return new objects; originals unchanged
- Copy-on-write (COW) for large buffers
- Undo/redo via Command Pattern with full state serialization

### 5. Backend Abstraction
- `nexus::gfx` defines abstract interfaces: `Device`, `CommandBuffer`, `Swapchain`, `Pipeline`, `Buffer`, `Texture`, `DescriptorSet`, `FrameScheduler`, `Renderer`
- Backends implement these interfaces (`nexus::gfx::vulkan`, `nexus::gfx::null`)
- Backend selected at runtime via `RenderContextDesc::preferredBackend`
- Capability queries via `caps()` gate feature paths (ray tracing tier, mesh shaders, etc.)

---

## Module Details

### `nexus::geometry` — Geometry Kernel (Core)

**Location:** `src/kernel/src/geometry/` (~60 source files)

```
src/kernel/src/geometry/
├── AnalyticBRep.*          # Analytic B-rep (Box/Cylinder/Sphere/Cone/Torus)
├── HalfEdgeMesh.*          # Half-edge structure, Euler ops, validators
├── Mesh.*                  # Indexed n-gon mesh, attributes, topology
├── MeshAttributes.*        # Positions, normals, UVs, colors, tangents
├── MeshTopology.*          # Face/vertex connectivity
├── MeshBoolean.*           # CSG: tri-tri → retriangulate → cut → classify → stitch
├── SubdivisionSurface.*    # Catmull-Clark, Loop, crease weights
├── MeshRemesh.*            # Quad remesh, voxel remesh, decimation
├── MeshDecimator.*         # Quadric error decimation
├── MeshLaplacian.*         # Smoothing, fairing, parameterization
├── MeshBVH.*               # Bounding volume hierarchy (SAH, parallel)
├── Delaunay2D.*            # Constrained Delaunay triangulation
├── ConstrainedDelaunay2D.* 
├── TetDelaunay3D.*         # 3D Delaunay for tetrahedralization
├── BooleanOperation.*      # CSG entry point
├── Tolerance.h             # Central tolerance system
├── RobustPredicates.*      # Exact orient2D/3D, incircle/insphere
├── MeshIO.*                # OBJ, STL, PLY, glTF import/export
├── Primitives.*            # Box, sphere, cylinder, cone, torus, plane
├── IntegrityReport.*       # Validation results
├── Aabb.*                  # Axis-aligned bounding box
├── Obb.*                   # Oriented bounding box
├── Plane.*                 # Plane geometry
├── Ray.*                   # Ray geometry
├── Segment.*               # Line segment
├── Triangle.*              # Triangle geometry
├── Vec2/Vec3/Vec4.*        # Math types (nexus::render::Vec3)
├── Mat3/Mat4.*             # Matrix types
├── Quaternion.*            # Rotation
├── Transform.*             # Decomposed transform (T/R/S)
├── UUID.*                  # Deterministic UUIDs
└── ... (60+ files)
```

**Key Properties:**
- **Zero external deps** (except stdlib, Vulkan for gfx types)
- **Deterministic** — same input → bit-identical output
- **Thread-safe reads** — write via command queue
- **Deterministic UUIDs** for geometry entities

### `nexus::cad` — Parametric CAD Layer

**Location:** `src/kernel/src/cad/`

```
src/kernel/src/cad/
├── CadDocument.*           # Feature history, undo/redo, serialization
├── CadFeature.*            # Sketch, Extrude, Revolve, Fillet, Chamfer, etc.
├── CadSketch.*             # 2D constraint solver
├── CadConstraint.*         # Tangent, coincident, parallel, perpendicular, fix
├── CadCommand.*            # Command pattern for undo/redo
├── CadSelection.*          # Multi-level selection (object/face/edge/vertex)
├── CadViewer.*             # Visual representation of CAD features
├── CadFeatureFactory.*     # Feature registry/creation
├── CadHistory.*            # DAG of feature dependencies
├── CadSolver.*             # Parallel constraint solver
├── CadAutoConstraintSketch.* # Auto-constraining sketch
├── CadExport.*             # STEP, USD, glTF export
└── CadImport.*             # STEP, USD, glTF import
```

**Feature DAG Regeneration:**
- Topological sort by parent dependencies
- `CadDocument::regenerateAll()` handles dirty propagation
- `markDirty(FeatureId)` propagates downstream
- Deterministic regeneration order

### `nexus::gfx` — Graphics Abstraction

**Location:** `src/kernel/include/nexus/gfx/`, `src/kernel/src/gfx/`, `src/kernel/src/gfx/vulkan/`

```
include/nexus/gfx/
├── Device.h                # Logical device, queues, VMA allocator
├── Swapchain.h             # Vulkan swapchain, acquire/present
├── FrameScheduler.h        # Acquire → Record → Submit → Present
├── CommandBuffer.h         # Dynamic rendering, barriers, descriptors
├── Pipeline.h              # Graphics/Compute/RT pipelines, PSO cache
├── Buffer.h / Texture.h    # Resources with VMA
├── DescriptorSet.h         # Descriptor set layout/pool/allocation
├── Shader.h                # SPIR-V compilation, reflection
├── RenderContext.h         # High-level render context
├── Renderer.h              # Deferred renderer, shadow, composite
├── FrameScheduler.h        # Acquire → Record → Submit → Present
├── Swapchain.h             # Vulkan swapchain management
├── Types.h                 # Handles, formats, layouts, barriers
└── Vulkan/                 # Vulkan backend implementation
    ├── VulkanDevice.cpp
    ├── VulkanSwapchain.cpp
    ├── VulkanCommandBuffer.cpp
    ├── VulkanFrameScheduler.cpp
    ├── VulkanPipeline.cpp
    ├── VulkanShader.cpp
    ├── VulkanTexture.cpp
    ├── VulkanBuffer.cpp
    ├── VulkanDescriptorSet.cpp
    ├── VulkanFrameScheduler.cpp
    ├── VulkanAllocator.cpp (VMA)
    └── ...

src/kernel/src/gfx/null/    # Null backend (headless CI)
```

**Render Path (Scheduler-Driven, Deferred):**
```
Shadow Pass → Geometry/GBuffer Pass → Lighting/Composite Pass → (Optional) RayTracing Pass
     │              │                      │                        │
     ▼              ▼                      ▼                        ▼
RenderGraphValidator checks per-pass texture layout transitions
FrameCaptureExporter records pass metadata for diagnostics
```

**Policy:** Production rendering uses scheduler path. Non-scheduler path is minimal compatibility fallback — NOT feature parity target. New features land on scheduler path first with explicit transitions and tests.

### `nexus::app` — Application Framework

**Location:** `src/kernel/src/app/`

```
src/kernel/src/app/
├── ModelingApplication.*   # Top-level app, lifecycle
├── AppContext.*            # Dependency injection container
├── AppMode.*               # State pattern for modes
├── ModeOrchestrator.*      # Mode switching, transitions
├── ModeRegistry.*          # Mode discovery/registration
├── ViewportController.*    # Camera, orbit/pan/zoom
├── TransformGizmo.*        # Translate/Rotate/Scale gizmo
├── SelectionManager.*      # Multi-level selection
├── CommandDispatcher.*     # Input → command mapping
├── WorkspaceManager.*      # Document/workspace management
├── ToolSystem.*            # Tool registration, input mapping
├── AssetBrowser.*          # Asset management UI
├── EditorUI.*              # ImGui + Vulkan backend
├── ViewportGrid.*          # Grid rendering
├── ViewportAxes.*          # Axis widget
├── SelectionHighlight.*    # Selection visualization
├── SketchPreview.*         # Sketch overlay
├── DimensionDisplay.*      # Dimension annotations
└── main.cpp                # Binary entry (window, event loop)
```

**AppMode State Pattern:**
```cpp
class AppMode {
public:
    virtual ~AppMode() = default;
    virtual void onEnter(AppContext&) {}
    virtual void onExit(AppContext&) {}
    virtual void onUpdate(AppContext&, float dt) {}
    virtual void onRender(AppContext&, RenderContext&) {}
    virtual void onInput(AppContext&, const InputEvent&) {}
    virtual void onImGui(AppContext&) {}
};
```

**Built-in Modes:** Select, Sketch, Extrude, Revolve, Fillet, Chamfer, Boolean, Sculpt, Animation, Simulation

### `nexus::render` — Scene & Rendering (CPU-side)

**Location:** `src/kernel/src/render/`

```
src/kernel/src/render/
├── SceneGraph.*            # Nodes with decomposed Transform
├── SceneNode.*             # Mesh refs, transform, visibility
├── Camera.*                # Perspective/orthographic, frustum
├── Frustum.*               # Culling
├── Mesh.*                  # GPU mesh (vertex/index/meshlet buffers, BLAS)
├── Material.*              # PBR material system
├── Light.*                 # Directional, point, spot, area
├── Environment.*           # IBL, skybox
├── Viewport.*              # Render viewport, camera, settings
└── Picking.*               # BVH-based ray picking
```

**SceneGraph Properties:**
- Non-copyable (contains `unique_ptr`)
- Decomposed Transform: `translation (Vec3)`, `rotation (Vec4 quat xyzw)`, `scale (Vec3)`
- Mesh refs: vertex/index/meshlet buffers, BLAS
- TLAS for ray tracing

### `nexus::animation` — Animation System

**Location:** `src/kernel/src/animation/`

```
src/kernel/src/animation/
├── AnimationClip.*         # Keyframe tracks
├── AnimationTrack.*        # Translation/Rotation/Scale curves
├── AnimationState.*        # Playing clip, time, weight
├── AnimationMixer.*        # Blend multiple clips
├── Skeleton.*              # Joint hierarchy
├── Skin.*                  # Vertex weights, joint indices
├── InverseKinematics.*     # CCD, FABRIK solvers
└── AnimationGraph.*        # State machine, transitions
```

### `nexus::sim` — Simulation

**Location:** `src/kernel/src/sim/`

```
src/kernel/src/sim/
├── RigidBodySolver.*       # Deterministic fixed-step (explicit Euler)
├── FluidSolver.*           # SPH / FLIP
├── ClothSolver.*           # PBD / XPBD
├── SoftBodySolver.*        # FEM / PBD
├── SimulationSceneCoupling.* # Solver state → Scene nodes
├── SimulationDriver.*      # Fixed-timestep accumulator + interpolation
└── CollisionDetection.*    # Broad/narrow phase
```

**SimulationDriver Pattern (Canonical "Fix Your Timestep"):**
```cpp
void step(float dt) {
    accumulator += dt;
    while (accumulator >= fixedDt) {
        solver.step(fixedDt);  // Deterministic
        accumulator -= fixedDt;
    }
    alpha = accumulator / fixedDt;
    interpolateState(alpha);  // lerp pos, nlerp rot
}
```

### `nexus::sculpt` — Sculpting

**Location:** `src/kernel/src/sculpt/`

```
src/kernel/src/sculpt/
├── SculptBrush.*           # Brush base class
├── SculptBrushDraw.*       # Draw brush
├── SculptBrushSmooth.*     # Smooth brush
├── SculptBrushFlatten.*    # Flatten brush
├── SculptBrushPinch.*      # Pinch brush
├── SculptBrushGrab.*       # Grab brush
├── SculptBrushInflate.*    # Inflate brush
├── SculptBrushSnakeHook.*  # Snake hook
├── Dynt
├── DynTopo.*               # Dynamic topology
├── MultiRes.*              # Multi-resolution
└── SculptLayer.*           # Non-destructive layers
```

### `nexus::parametric` — Constraint Solver

**Location:** `src/kernel/src/parametric/`

```
src/kernel/src/parametric/
├── ConstraintGraph.*       # Constraint DAG
├── ConstraintSolver.*      # Parallel solver (adaptive correction)
├── Constraint.*            # Base constraint
├── ConstraintCoincident.*  # Point-point, point-line, point-circle
├── ConstraintTangent.*     # Line-circle, circle-circle
├── ConstraintParallel.*    # Line-line
├── ConstraintPerpendicular.* # Line-line
├── ConstraintDistance.*    # Point-point, point-line
├── ConstraintAngle.*       # Line-line
├── ConstraintRadius.*      # Circle
├── ConstraintHorizontal.*  # Line
├── ConstraintVertical.*    # Line
├── ConstraintEqual.*       # Line length, radius
├── ConstraintMidpoint.*    # Point on segment
├── ConstraintSymmetric.*   # Points about line
├── ConstraintCombinators.* # Parallel, perpendicular, midpoint, etc.
└── Serialization.*         # Constraint graph serialization
```

**Solver Quirks:**
- Parallel solver (`parallelSolve`) in `nexus::parametric`, NOT `nexus::geometry::parametric`
- Constraint combinators compose from primitives — must be declared in BOTH header AND .cpp
- Adaptive correction: when error increases, step size halved (`adaptiveMaxCorrection *= 0.5`)

---

## Data Flow: Geometry → Render

```
CadDocument (feature history)
    │
    ▼
CadFeature::regenerate()  ───►  B-Rep / Mesh  ──►  CadFeature::mesh (cached)
    │
    ▼
CadViewer  ◄──  CadDocument  ◄──  CadDocument::regenerate()
    │
    ▼
ViewportController  ◄──  CadViewer (scene graph)
    │
    ▼
Renderer (nexus::render::Renderer)
    │
    ├── G-Buffer pass (deferred)
    ├── Shadow pass
    ├── Lighting/Composite pass
    ├── UI overlay (ImGui)
    ▼
Swapchain → Present
```

---

## Build Configuration

```cmake
# CMakeLists.txt (root)
cmake_minimum_required(VERSION 3.28)
project(NexusModeling LANGUAGES CXX)

# C++26
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warnings as errors
add_compile_options(
    -Wall -Wextra -Wpedantic -Werror
    -Wshadow -Wconversion -Wsign-conversion
    -Wnull-dereference -Wdouble-promotion
)

# Sanitizers (Debug)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# Vulkan
find_package(Vulkan REQUIRED)
find_package(glfw3 REQUIRED)
find_package(VulkanMemoryAllocator REQUIRED)

# External deps via FetchContent
FetchContent_Declare(imgui 
    GIT_REPOSITORY https://github.com/ocornut/imgui.git 
    GIT_TAG v1.91.6-docking)
FetchContent_Declare(glslang ...)
```

### Backend Configuration
```bash
# Standard build
cmake -S . -B build -DNEXUS_BACKEND_VULKAN=ON -DNEXUS_BACKEND_NULL=ON

# Null backend required for CI/headless testing
# Vulkan backend requires: Vulkan SDK + GLSL compiler + VMA
```

### Nexus Modeling Executable Requirements
- GLFW3
- GLU (Linux): `pkg-config glfw3 glu`

---

## Testing Strategy

```cpp
// Unit: geometry kernel (1921 tests)
TEST(MeshBoolean, CubeUnionCube) { ... }
TEST(HalfEdge, InsertEdgeVertex_IntegrityClean) { ... }
TEST(Boolean, CubeUnionCube_Watertight) { ... }

// Integration: renderer
TEST(VulkanRenderer, OffscreenClearAndReadback) { ... }
TEST(VulkanRenderer, GBufferMrtGeometryPipelineRunsThroughRenderer) { ... }

// Performance regression
TEST(KernelPerf, BooleanCubeUnder1ms) { ... }
TEST(KernelPerf, ExtrudeUnder1ms) { ... }

// Headless CI
ctest --test-dir build --output-on-failure
```

**Quality Gates (Every Commit):**
```bash
cmake --build build -j$(nproc)          # Must pass -Werror; no warnings
ctest --test-dir build --output-on-failure  # 2026+ tests, 0 regressions
```

One pre-existing allowed failure: `ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace`

---

## Performance Baselines

| Operation | Target | Current |
|-----------|--------|---------|
| Cube ↔ Cube Boolean | < 1 ms | ~0.3 ms |
| 10k triangle extrude | < 1 ms | ~0.4 ms |
| 100k tri mesh boolean | < 10 ms | ~6 ms |
| 1M tri decimate 50% | < 100 ms | ~60 ms |
| 100k tri Catmull-Clark | < 50 ms | ~30 ms |
| 1280×720 deferred frame | < 16 ms | ~8 ms |

---

## Quality Gates (Every Commit)

```bash
# Pre-commit (local)
cmake --build build -j$(nproc) && ctest --test-dir build

# CI (GitHub Actions)
- Build: Release + Debug + ASan/UBSan
- Test: 2026 tests (1921 kernel + 105 render + integration)
- Static analysis: clang-tidy, cppcheck
- Format: clang-format
- API freeze check: abi-compliance-checker
```

---

## Next: Developer Guides

- [Architecture Deep Dive](architecture-deep-dive.md)
- [Geometry Kernel Internals](geometry-kernel-internals.md)
- [Renderer Internals](renderer-internals.md)
- [Adding a New Geometry Op](adding-geometry-op.md)
- [Adding a CAD Feature](adding-cad-feature.md)
- [Porting to New Platform](porting.md)

---

*Kernel v0.1.0-dev | 2026 tests passing | C++26 | Vulkan 1.3*