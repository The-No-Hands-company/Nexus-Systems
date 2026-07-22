# Nexus Modeling

Nexus Modeling is the geometry and rendering application in Nexus Systems, targeting production-scale DCC workflows (Blender/Rhino/Maya/Modo/Houdini/Plasticity/Rhino class) with a **Vulkan-first C++26 kernel**.

---

## Current State

| Component | Status | Tests |
|-----------|--------|-------|
| Geometry Kernel | ✅ Core complete | 1921/1921 |
| Vulkan Renderer | ✅ Headless + Windowed | 145/145 |
| Analytic B-rep | ✅ Box/Cylinder/Sphere/Cone/Torus | 2025/2026 |
| Boolean/CSG | ✅ Real CSG pipeline | 2025/2026 |
| Half-edge + Euler ops | ✅ 6/6 ops integrity-clean | 2025/2026 |
| Analytic B-rep primitives | ✅ 5 types with eval/normalAt | ✅ |
| Editor UI (Vulkan) | ✅ ImGui + Vulkan backend | Working |
| Headless render-to-PNG | ✅ `--shot` flag | Working |
| **Total Test Suite** | **2026 tests passing** | **1 pre-existing ApiFreezeAudit failure** |

---

## Repository Layout

```
Nexus-Modeling/
├── AGENTS.md                    # Agent/contributor contract
├── CLAUDE.md                    # Claude Code guidance
├── CONTRIBUTING.md              # Contribution guidelines
├── README.md                    # This file
├── ROADMAP.md                   # Autonomous loop roadmap
├── docs/                        # Full documentation
│   ├── README.md                # Documentation index
│   ├── GEOMETRY_KERNEL_COMPENDIUM.md  # Technology bible (2000+ lines)
│   ├── kernel-capability-map.md       # Capability ownership
│   ├── developer/               # Developer deep-dives
│   │   ├── architecture.md
│   │   ├── geometry-kernel.md
│   │   ├── boolean-csg.md
│   │   ├── halfedge-mesh.md
│   │   ├── analytic-brep.md
│   │   ├── renderer.md
│   │   ├── editor-architecture.md
│   │   ├── build-system.md
│   │   └── testing.md
│   ├── expert/                  # Expert user guides
│   ├── beginner/                # Beginner tutorials
│   └── html/                    # Generated HTML site
├── app/
│   ├── CMakeLists.txt
│   └── main.cpp                 # Binary entry (window, event loop)
├── src/
│   └── kernel/                  # Core kernel library
│       ├── CMakeLists.txt
│       ├── include/nexus/       # Public API
│       │   ├── geometry/
│       │   ├── cad/
│       │   ├── render/
│       │   ├── gfx/
│       │   ├── app/
│       │   ├── animation/
│       │   ├── sim/
│       │   ├── sculpt/
│       │   ├── parametric/
│       │   └── Kernel.h
│       └── src/                 # Implementation
│           ├── geometry/        # ~60 files: Mesh, B-rep, Half-edge, CSG, Remesh, Subdiv, BVH
│           ├── cad/             # Feature history, sketches, constraints, commands
│           ├── render/          # Scene graph, camera, materials, lights
│           ├── gfx/             # Vulkan/Null backend abstraction
│           │   └── vulkan/      # Vulkan 1.3 implementation
│           ├── app/             # App framework: modes, gizmos, viewport, UI
│           ├── animation/       # Keyframes, blending, IK, skinning
│           ├── sim/             # Rigid body, fluid, cloth, driver
│           ├── sculpt/          # Brushes, dynotopo, multires, layers
│           └── parametric/      # Constraint graph, parallel solver
└── tests/
    └── kernel/                  # 1921 unit + integration + perf tests
```

---

## Build and Test

### Prerequisites

- **CMake 3.28+** (4.3.4 recommended)
- **C++26 compiler**: GCC 13+, Clang 17+, MSVC 19.40+
- **Vulkan SDK 1.3+** (for Vulkan backend)
- **GLFW 3.4+**, **VMA 3.x**, **glslang**, **SPIRV-Tools**, **SPIRV-Cross**
- **Ninja** (recommended)

### Linux (Fedora/RHEL)

```bash
sudo dnf install cmake gcc-c++ ninja-build vulkan-devel glfw-devel \
    vulkan-memory-allocator-devel glslang-devel spirv-tools-devel
```

### Linux (Ubuntu/Debian)

```bash
sudo apt install cmake g++ ninja-build libvulkan-dev libglfw3-dev \
    libvma-dev glslang-dev spirv-tools-dev
```

### Configure & Build

```bash
# Standard build (Vulkan + Null backends)
cmake -S . -B build -G Ninja \
    -DNEXUS_BACKEND_VULKAN=ON \
    -DNEXUS_BACKEND_NULL=ON \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build -j$(nproc)
```

### Run Tests

```bash
# All tests (headless via Null backend)
ctest --test-dir build --output-on-failure

# Filter by pattern
ctest --test-dir build -R "MeshBoolean" --output-on-failure

# Single test suite
./build/tests/nexus_kernel_tests --gtest_filter="MeshBoolean.*"

# Performance tests
./build/tests/nexus_kernel_perf_smoke
```

### Run Application

```bash
# Windowed (requires display)
./build/src/kernel/nexus_modeling

# Headless screenshot
./build/src/kernel/nexus_modeling --shot /tmp/screenshot.png 1280 720

# With custom size
./build/src/kernel/nexus_modeling --width 1920 --height 1080
```

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                    nexus_modeling (App)                         │
├─────────────────────────────────────────────────────────────────┤
│  nexus::app (Framework)                                         │
│  ├── ModeOrchestrator, ModeRegistry, AppMode (State pattern)   │
│  ├── ModelingApplication, ViewportController                    │
│  ├── TransformGizmo, SelectionManager, ToolSystem               │
│  └── EditorUI (ImGui + Vulkan)                                  │
├─────────────────────────────────────────────────────────────────┤
│  nexus::cad (Parametric CAD)                                    │
│  ├── CadDocument (feature history, undo/redo)                   │
│  ├── CadSketch + 11 constraint types                            │
│  ├── CadSolver (parallel, adaptive)                             │
│  ├── CadFeature (Extrude, Revolve, Fillet, Chamfer, Boolean)   │
│  └── CadCommand (command pattern)                               │
├─────────────────────────────────────────────────────────────────┤
│  nexus::geometry (Kernel — zero deps, headless)                 │
│  ├── Analytic B-rep: Box/Cylinder/Sphere/Cone/Torus + Boolean  │
│  ├── Half-edge Mesh: 6 integrity-clean Euler ops                │
│  ├── Mesh: n-gon + attributes + BVH (SAH, parallel)            │
│  ├── CSG: tri-tri → retriangulate → cut → classify → stitch    │
│  ├── Subdivision: Catmull-Clark/Loop + crease weights          │
│   + Remesh: Voxel/Quad + Decimation (quadric)                  │
│  ├── Tolerance: scale-aware, IEEE-754 compliant                 │
│  └── Robust Predicates: Shewchuk exact orient/incircle         │
├─────────────────────────────────────────────────────────────────┤
│  nexus::gfx (Graphics Abstraction)                              │
│  ├── Device, Swapchain, FrameScheduler, CommandBuffer          │
│  ├── Pipeline (gfx/comp/RT), Buffer, Texture, DescriptorSet    │
│  ├── Renderer (deferred, G-buffer, shadow, composite)          │
│  └── Backends: Vulkan 1.3 (dynamic rendering, sync2) + Null    │
└─────────────────────────────────────────────────────────────────┘
```

**Layer Rule:** `app → cad → geometry → gfx` (no upward deps)

---

## Key Technologies

| Area | Choice | Rationale |
|------|--------|-----------|
| Language | **C++26** | Modules, concepts, contracts, std::execution |
| Build | **CMake + Ninja** | Standard, fast, cross-platform |
| GPU | **Vulkan 1.3** | Explicit control, compute, ray tracing, portability |
| Memory | **VMA 3.x** | Sub-allocation, defragmentation |
| Math | **Eigen + GLM** | Linear algebra + SIMD geometry |
| Exact Math | **Shewchuk predicates** | Zero rounding error in topology |
| UI | **ImGui 1.91+ (docking)** | Immediate mode, Vulkan backend |
| Test | **GoogleTest + Benchmark** | Unit, property, perf regression |
| Sanitizers | ASan/UBSan/TSan/MSan | CI-caught UB |

---

## Documentation

| Audience | Entry Point |
|----------|-------------|
| **Developers** | [docs/developer/architecture.md](docs/developer/architecture.md) |
| **Kernel Internals** | [docs/developer/geometry-kernel.md](docs/developer/geometry-kernel.md) |
| **CSG Pipeline** | [docs/developer/boolean-csg.md](docs/developer/boolean-csg.md) |
| **Half-edge Mesh** | [docs/developer/halfedge-mesh.md](docs/developer/halfedge-mesh.md) |
| **Analytic B-rep** | [docs/developer/analytic-brep.md](docs/developer/analytic-brep.md) |
| **Renderer** | [docs/developer/renderer.md](docs/developer/renderer.md) |
| **Build System** | [docs/developer/build-system.md](docs/developer/build-system.md) |
| **Testing** | [docs/developer/testing.md](docs/developer/testing.md) |
| **Technology Bible** | [docs/GEOMETRY_KERNEL_COMPENDIUM.md](docs/GEOMETRY_KERNEL_COMPENDIUM.md) |
| **Capability Map** | [docs/kernel-capability-map.md](docs/kernel-capability-map.md) |

### HTML Site

All docs available as HTML in `docs/html/`:
- `docs/html/developer/index.html`
- `docs/html/expert/index.html`
- `docs/html/beginner/index.html`
- `docs/html/api/index.html`

---

## Quality Gates (Every Commit)

```bash
# 1. Build (must pass -Werror)
cmake --build build -j$(nproc)

# 2. Test (2026 tests, 0 regressions)
ctest --test-dir build --output-on-failure

# 3. Format
cmake --build build --target format-check

# 4. Static analysis
cmake --build build --target tidy-check
```

One pre-existing allowed failure: `ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace`

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the autonomous loop priority list:

- **P0**: Topology completeness (insertEdgeLoop, splitEdge NGon) ✅
- **P1**: Boolean exact arithmetic + BVH acceleration
- **P2**: Half-edge bevel/connect/gridFill completion
- **P3**: TemplateManager, test coverage
- **P4**: BVH snapping, edge caching

---

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and [AGENTS.md](AGENTS.md).

**Branch:** `main`  
**Commit style:** `type: description` (feat, fix, refactor, docs, test, perf)  
**Push:** Direct to `origin/main`  

---

## License

Apache 2.0 — see [LICENSE](../LICENSE)

---

*Nexus Modeling v0.1.0-dev | 2026 tests passing | C++26 | Vulkan 1.3*