# AGENTS

Scope: entire Nexus Modeling workspace — the DCC application and its kernel.

## Mission

Build a production-grade C++23 Vulkan-first geometry and rendering kernel for a full DCC (Nexus Modeling) spanning geometry, CAD, animation, simulation, sculpting, and rendering.

## Quality Gates (Mandatory)

After every code change, run in order:

```bash
cmake --build build -j$(nproc)          # must pass -Werror; no warnings
ctest --test-dir build --output-on-failure  # 2026+ tests, 0 regressions
```

One pre-existing allowed failure: `ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace`.

## Architecture Layers

```
nexus_modeling (executable — GLFW + Vulkan viewport)
    └── nexus::app (application framework)
        ├── ModeOrchestrator, ModeRegistry, AppMode (State pattern)
        ├── ModelingApplication (top-level app)
        └── ViewportController
    └── nexus::cad (CAD — parametric modeling)
        ├── CadDocument, CadSelection, CadFeatureTree
        ├── FeatureHistory, CadCommands (undo/redo)
        └── CadAutoConstraintSketch, CadViewer
    └── nexus::geometry (geometry kernel — ~60 source files)
        ├── B-Rep, NURBS, Mesh, HalfEdgeMesh, Boolean
        ├── Constraint solver (7 base types + 11 combinators)
        ├── SolidOperations, BlendNetwork, Analysis
        └── 12 curve types, 13 surface types, 9 interchange formats
    └── nexus::render, nexus::animation, nexus::sim, nexus::sculpt
    └── nexus::parametric (constraint graph, solver, serialization)
    └── nexus::gfx (Vulkan/Null device abstraction)
```

**Layer dependency rule:** `nexus::app` depends on `nexus::cad`. `nexus::cad` depends on `nexus::geometry` + `nexus::parametric`. No upward dependencies.

## Public API Boundary

- Public headers: `src/kernel/include/nexus/<module>/`
- Internal impl: `src/kernel/src/<module>/`
- New source files MUST be registered in `src/kernel/CMakeLists.txt`
- New test files MUST be registered in `tests/CMakeLists.txt`
- `nexus_modeling` binary source: `app/main.cpp` (separate from kernel library)

## Running Specific Tests

```bash
# Run all tests matching a pattern
ctest --test-dir build -R "Pattern" --output-on-failure

# Run a single test directly
./build/tests/nexus_kernel_tests --gtest_filter="SuiteName.TestName"

# Run the windowed application (requires display)
./build/src/kernel/nexus_modeling
```

## Naming Rules (Hard-Earned)

- **No vendor names** in code, comments, or filenames (no Parasolid, ACIS, SolidWorks, etc.)
- **No marketing buzzwords** (no "Smart", "Advanced", "Production" in filenames)
- **No abbreviations** in public API headers (spell out — e.g. `CadProductManufacturingInfo` not `CadMBD`)
- **Files named by domain**, not by perceived quality (e.g. `CadAutoConstraintSketch`, not `SmartSketch`)
- **One header → one concern.** Avoid multi-group junk-drawer headers.

## Clear Separation of Concerns (Non-Negotiable)

Every visual, interactive, or behavioral unit gets its own header+source pair. When extending the system, create new files — never grow existing ones beyond their concern.

Examples of what MUST be separate files:

| Concern | File pair | Must NOT live in |
|---------|-----------|-----------------|
| Viewport grid | `ViewportGrid.h/.cpp` | main.cpp, CadViewer |
| Transform gizmo | `TransformGizmo.h/.cpp` | main.cpp, viewport |
| Axis widget | `ViewportAxes.h/.cpp` | main.cpp |
| Selection highlight | `SelectionHighlight.h/.cpp` | CadViewer |
| Sketch preview overlay | `SketchPreview.h/.cpp` | CadAutoConstraintSketch |
| Dimension display | `DimensionDisplay.h/.cpp` | CadDimension |

Rule: if you can describe what a piece of code does in one sentence and that sentence doesn't overlap with the file it lives in, it belongs in its own file.

This applies to the application binary (`app/`) as well. The binary's `main.cpp` should ONLY contain: window creation, event loop wiring, and top-level dispatch. Rendering code, UI code, and tool code all belong in their own files under the appropriate namespace.

Result: when a bug or feature involves "the grid," you open `ViewportGrid.cpp`. When it involves "the gizmo," you open `TransformGizmo.cpp`. Nobody roams through millions of lines — every file is a single-page concern.

## Vec3 Type Gotcha

`Vec3` is `nexus::render::Vec3`. In `nexus::geometry` and `nexus::cad` sources, either:

```cpp
using Vec3 = nexus::render::Vec3;  // preferred
// or use the full type: nexus::render::Vec3
```

Headers in `nexus/geometry/` that use `Vec3` must declare the using or fully qualify.

## Class Design Gotchas

- `SceneGraph` is non-copyable (contains `unique_ptr`). Use pointers or references.
- `CadAutoConstraintSketch` has a reference member (`CadDocument&`) — cannot be default-constructed or reassigned. Use `unique_ptr`.
- `MeshAttributes::positions()` returns `const std::vector<Vec3>&`. To modify positions, copy, modify, then call `setPositions()`.
- `const std::vector<Vec3>&` returns a const ref — `auto pos = ...` creates a copy (strips const&).
- `[[nodiscard]]` is used everywhere. Cast return values with `(void)` when intentionally ignoring.

## Constraint Solver Quirks

- The parallel solver (`parallelSolve`) is in `nexus::parametric`, not `nexus::geometry::parametric`.
- Constraint combinators (parallel, perpendicular, midpoint, etc.) compose from existing primitives. They must be declared in BOTH the header AND implemented in the .cpp.
- The solver uses adaptive correction — when error increases, step size is halved (`adaptiveMaxCorrection *= 0.5`).

## CMakeLists Registration

Every new `.cpp` file must be listed in `src/kernel/CMakeLists.txt` alphabetically within its module. New test files go in `tests/CMakeLists.txt`. Forgetting this causes linker errors with no obvious cause.

## Build Configuration

```bash
cmake -S . -B build -DNEXUS_BACKEND_VULKAN=ON -DNEXUS_BACKEND_NULL=ON
```

The Null backend is required for CI/headless testing. The Vulkan backend requires Vulkan SDK + GLSL compiler + VMA.

The `nexus_modeling` executable additionally requires GLFW3 and GLU (Linux): `pkg-config glfw3 glu`.

## Existing Scaffolding (Do Not Break)

- Test count: 2026 (one pre-existing API freeze audit failure allowed)
- All tests run headless via Null backend where Vulkan is unavailable
- `nexus_kernel_perf_smoke` — separate performance benchmark binary
- Sources in `src/automation/` — scripting infrastructure; maintain API compatibility
- Sources in `src/animation/` — keyframe/blend/IK; avoid breaking animation contracts

---

*See `docs/developer/architecture.md` for full system architecture, `docs/developer/geometry-kernel.md` for kernel internals, `docs/GEOMETRY_KERNEL_COMPENDIUM.md` for the technology bible.*