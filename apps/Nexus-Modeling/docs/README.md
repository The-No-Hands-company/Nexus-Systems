# Nexus Modeling — Documentation

> **Status**: Scaffold phase — core graphics kernel is functional, render loop and higher-level systems are in progress.

## Contents

| Document | What it covers |
|---|---|
| [graphics-kernel.md](graphics-kernel.md) | Full kernel architecture, API reference, hardware tiers, Vulkan feature map |
| [getting-started.md](getting-started.md) | Build prerequisites, CMake configuration, running tests |
| [shader-pipeline.md](shader-pipeline.md) | GLSL → SPIR-V compilation, `ShaderDesc` dispatch rules |
| [design-decisions.md](design-decisions.md) | Why Vulkan-first, reversed-Z depth, handle system, VMA strategy, neural plugin pattern |
| [testing-strategy.md](testing-strategy.md) | Test pyramid, required gates, and coverage roadmap |
| [vision-and-roadmap.md](vision-and-roadmap.md) | Consolidated north-star vision, 12-month roadmap, Month 1 kickoff, scope controls |
| [api-freeze-v0.md](api-freeze-v0.md) | Month 1 public API boundary freeze record |
| [ux-baseline-charter.md](ux-baseline-charter.md) | Month 1 newcomer-first and expert-expandable UX baseline charter |
| [kernel-baseline-report.md](kernel-baseline-report.md) | Month 1 build, test, and baseline status report |
| [perf-benchmark-matrix.md](perf-benchmark-matrix.md) | Reproducible perf smoke matrix and harness contract |
| [memory-lifetime-report.md](memory-lifetime-report.md) | Explicit lifecycle and memory reporting artifact definition |
| [month-2-geometry-checklist.md](month-2-geometry-checklist.md) | Concrete implementation checklist for Month 2 Deterministic Mesh Foundation v0 |
| [month-3-render-checklist.md](month-3-render-checklist.md) | Concrete implementation checklist for Month 3 Render Pipeline v1 |
| [month-4-parametric-checklist.md](month-4-parametric-checklist.md) | Concrete implementation checklist for Month 4 Parametric Foundation |
| [api-freeze-alpha.md](api-freeze-alpha.md) | Month 12 API freeze audit process, manifest policy, and drift controls |
| [month-12-alpha-compatibility-report.md](month-12-alpha-compatibility-report.md) | Month 12 alpha hardening report: API audit, determinism/perf, compatibility status |
| [hardening-debt-ledger.md](hardening-debt-ledger.md) | Explicit Month 1-3 hardening debt items with measurable closure criteria |
| [PRD.md](PRD.md) | Product goals, user targets, and high-level functional requirements |
| [SDD.md](SDD.md) | Technical architecture, implementation constraints, and validation gates |
| [FRD.md](FRD.md) | Granular behavioral requirements and acceptance criteria |
| [kernel-capability-map.md](kernel-capability-map.md) | Domain boundaries, ownership, and feature routing rules |

---

## What is Nexus Modeling?

Nexus Modeling is a **production-grade 3D content creation suite** targeting Blender/Rhino/Maya class capability. It is one application inside the broader [Nexus-Systems](../../) ecosystem owned by The No-Hands Company.

### Modalities

- **Polygon/mesh** modeling and sculpting
- **Subdivision surface** workflows for smooth animation-grade surfaces
- **Parametric / CAD** (constraint-based solids)
- **Implicit / field-driven** design (SDFs, lattices, boolean-heavy workflows)
- **Voxel / volume** workflows for simulation and analysis
- **Point cloud / splat / radiance-field** scene capture workflows
- **Simulation** (cloth, fluid, rigid body)
- **Data visualization** (scientific, architectural)
- **Neural-assisted** workflows (AI denoising, super-resolution, style transfer)

### Kernel direction

Nexus Modeling is targeting a hybrid, multi-engine kernel. Mesh is the baseline render and interchange representation, but it is not the only geometry representation the product is expected to support over time.

The intent is to let different domains use the representation that fits the job:

- Mesh and Sub-D for broad DCC interoperability and animation
- Parametric surfaces for manufacturing and engineering precision
- Implicit fields for robust booleans, sculpting, and lattice generation
- Voxels for simulation-heavy and AI-friendly spatial workloads
- Point and splat representations for scanned or neural scene capture

Roadmap detail for this direction lives in [vision-and-roadmap.md](vision-and-roadmap.md).

### Technology pillars

| Pillar | Choice | Rationale |
|---|---|---|
| Language | C++23 | Modules-ready, `std::span`, structured bindings, ranges |
| Primary backend | Vulkan 1.4 | Widest reach on Linux/Windows/Android, exposes latest GPU features |
| Memory | VMA 3.1 | Industry standard for Vulkan memory management |
| Shaders | GLSL → SPIR-V via glslang | Runtime compile or pre-compiled, same pipeline |
| Build | CMake 3.28+ | Standard, composable, FetchContent for dependencies |
| Tests | GoogleTest | 534 unit/integration tests, CI-safe headless paths |
| Neural plugins | DLSS4 / XeSS / OIDN | Runtime `dlopen`/`LoadLibrary` — zero hard dependency |

---

## Nexus-Cloud API Contract

Nexus Modeling runs headlessly on Nexus-Cloud servers for server-side rendering jobs:

- Create `RenderContext` with `preferredBackend = Backend::Null` for CI/cloud.
- The Null backend satisfies the full `IDevice` interface — tests run without a GPU.
- Frame output textures are readable via `IDevice` readback for image export to cloud storage.
- All resource handles are opaque typed integers — safe to serialise over IPC.

---

## Repository layout

```
Nexus-Modeling/
├── docs/                    ← this documentation tree
├── src/
│   └── kernel/              ← graphics kernel library
│       ├── include/nexus/   ← public headers (installed)
│       │   ├── gfx/         ← IDevice, ICommandBuffer, Types, RenderContext …
│       │   ├── animation/   ← skeleton, transform tracks, pose evaluation core
│       │   ├── render/      ← Camera, SceneGraph, Renderer
│       │   └── neural/      ← NeuralRenderer factory
│       └── src/             ← private implementation
│           ├── animation/
│           ├── backend/null/
│           ├── backend/vulkan/
│           ├── gfx/
│           ├── render/
│           └── neural/
└── tests/
    └── kernel/              ← unit + integration tests
```
