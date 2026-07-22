# Nexus Modeling Documentation

**Professional 3D Modeling & CAD Application**  
*Building the alternative to Blender, Maya, 3ds Max, Modo, Houdini, Plasticity, and Rhino*

---

## Documentation Index

### Developer Documentation
| Document | Description |
|----------|-------------|
| [Architecture Overview](developer/architecture.md) | System architecture, module dependencies, data flow |
| [Kernel API Reference](developer/kernel-api.md) | Complete kernel API reference |
| [Geometry Kernel](developer/geometry-kernel.md) | Geometry kernel internals, algorithms, data structures |
| [Render System](developer/renderer.md) | Vulkan renderer, frame scheduler, pipeline |
| [Editor Architecture](developer/editor-architecture.md) | Editor UI, ImGui integration, viewport |
| [Build System](developer/build-system.md) | CMake, dependencies, cross-compilation |
| [Testing](developer/testing.md) | Unit tests, integration tests, CI/CD |
| [Contributing](developer/contributing.md) | Code style, PR process, architecture decisions |
| [Adding Geometry Op](developer/adding-geometry-op.md) | How to add a new mesh operation |
| [Adding CAD Feature](developer/adding-cad-feature.md) | How to add a parametric CAD feature |
| [Mesh Operations](developer/mesh-operations.md) | Boolean, remesh, subdivision, decimation |
| [Boolean/CSG Deep Dive](developer/boolean-csg.md) | Exact CSG pipeline details |
| [Half-edge Mesh](developer/halfedge-mesh.md) | Half-edge topology, Euler ops, integrity |
| [Analytic B-rep](developer/analytic-brep.md) | Exact B-rep with analytic geometry |
| [Mesh I/O](developer/mesh-io.md) | OBJ, STL, PLY, glTF import/export |
| [Tolerance System](developer/tolerance.md) | Scale-aware tolerance management |
| [Porting Guide](developer/porting.md) | Porting to new platforms |

### Expert User Documentation
| Document | Description |
|----------|-------------|
| [Advanced Modeling](expert/advanced-modeling.md) | Boolean, bevel, fillet, subdivision, remesh |
| [CAD Workflows](expert/cad-workflows.md) | Sketch constraints, parametric features, fillet/chamfer |
| [Sculpting](expert/sculpting.md) | Brushes, dyntopo, multires, layers |
| [Animation & Rigging](expert/animation.md) | Keyframes, curves, IK, skinning |
| [Rendering & Shaders](expert/rendering.md) | Materials, PBR, custom shaders |
| [Scripting & Automation](expert/scripting.md) | Python API, headless batch, CLI |
| [Pipeline Integration](expert/pipeline.md) | USD, Alembic, FBX, pipeline integration |

### Beginner / User Documentation
| Document | Description |
|----------|-------------|
| [Getting Started](beginner/getting-started.md) | Installation, first launch, UI tour |
| [Interface Tour](beginner/interface-tour.md) | Viewport, panels, toolbars, shortcuts |
| [First Model](beginner/first-model.md) | Box → cylinder → boolean → export |
| [Navigation & Selection](beginner/navigation.md) | Orbit, pan, zoom, select modes |
| [Modeling Basics](beginner/modeling-basics.md) | Primitives, transform, edit modes |
| [Boolean Operations](beginner/boolean.md) | Union, difference, intersection |
| [Export & Import](beginner/export-import.md) | OBJ, STL, glTF, USD workflows |
| [Keyboard Shortcuts](beginner/shortcuts.md) | Complete reference |
| [Troubleshooting](beginner/troubleshooting.md) | Common issues & solutions |

### API Reference
| Document | Description |
|----------|-------------|
| [Kernel API](api/kernel.md) | Geometry, topology, math |
| [Render API](api/render.md) | Vulkan renderer, shaders, pipelines |
| [Editor API](api/editor.md) | UI, viewport, gizmos, selection |
| [Scripting API](api/scripting.md) | Python bindings, headless CLI |

### HTML Versions
All documents available in both **Markdown (.md)** and **HTML** formats:
- [Developer Docs (HTML)](html/developer/index.html)
- [Expert Docs (HTML)](html/expert/index.html)
- [Beginner Docs (HTML)](html/beginner/index.html)
- [API Reference (HTML)](html/api/index.html)

---

## Project Status

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

## Quick Links

- [Getting Started →](beginner/getting-started.md)
- [Architecture Overview](developer/architecture.md)
- [Keyboard Shortcuts](beginner/shortcuts.md)
- [API Reference](api/kernel.md)
- [Contributing](developer/contributing.md)
- [Roadmap](ROADMAP.md)
- [Technology Compendium](GEOMETRY_KERNEL_COMPENDIUM.md)
- [Capability Map](kernel-capability-map.md)

---

## Project Process & Collaboration

- [PRD](PRD.md) — Product Requirements Document
- [SDD](SDD.md) — Software Design Document
- [FRD](FRD.md) — Functional Requirements Document
- [Kernel Capability Map](kernel-capability-map.md)
- [Vision & Roadmap](vision-and-roadmap.md)
- [UX Baseline Charter](ux-baseline-charter.md)

---

## Technology Deep Dives

- [Design Decisions](design-decisions.md) — Architecture decision records
- [Graphics Kernel](graphics-kernel.md) — Renderer internals
- [Shader Pipeline](shader-pipeline.md) — GLSL→SPIR-V, reflection
- [Testing Strategy](testing-strategy.md) — Property-based, deterministic CI
- [Memory Lifetime Report](memory-lifetime-report.md) — Ownership, sync, lifetimes
- [Hardening Debt Ledger](hardening-debt-ledger.md) — Technical debt tracking
- [Performance Benchmark Matrix](perf-benchmark-matrix.md) — Baselines & targets

---

## Monthly Checklists

- [Month 1: Geometry](month-1-geometry-checklist.md)
- [Month 2: Render](month-2-render-checklist.md)
- [Month 3: Parametric](month-3-parametric-checklist.md)
- [Month 12: Alpha Compat](month-12-alpha-compatibility-report.md)
- [Month 13: RT & Sim Coupling](month-13-rt-and-simulation-coupling.md)

---

## API Freeze & Release

- [API Freeze Alpha](api-freeze-alpha.md)
- [API Freeze v0](api-freeze-v0.md)
- [API Contract Alpha Summary](api-contract-alpha-summary.md)
- [Release Notes 1.0 Alpha Draft](release-notes-1.0-alpha-draft.md)

---

*Last updated: 2026-07-14 | Nexus Modeling v0.1.0-dev*