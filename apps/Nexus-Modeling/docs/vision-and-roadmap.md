# Vision and Roadmap

This page is the single north-star reference for Nexus Modeling direction, execution cadence, and scope control.

It consolidates and complements:

- [PRD](PRD.md)
- [SDD](SDD.md)
- [Kernel Capability Map](kernel-capability-map.md)
- [API Freeze v0](api-freeze-v0.md)
- [UX Baseline Charter](ux-baseline-charter.md)
- [Kernel Baseline Report](kernel-baseline-report.md)

## Vision

Nexus Modeling is building a production-grade, open-source 3D creation kernel that can evolve into a full all-in-one DCC pipeline over time.

Near-term, the kernel is geometry and rendering focused. Long-term, it must support modeling, rendering, animation, rigging, simulation, scripting, and data pipeline automation under one stable architecture.

The user experience target is explicit:

1. New users should not face a wall of tools on first launch.
2. Experts should be able to surface advanced capability quickly through deep customization.
3. The product should feel simple by default and powerful by intent.

The strategy is ambitious vision with disciplined execution:

1. Build complete vertical slices.
2. Keep contracts explicit and deterministic.
3. Expand into new domains only when current milestone exits are met.

## Kernel Direction: Hybrid and Multi-Engine

Nexus Modeling is not a mesh-only kernel.

Mesh is the universal interchange and real-time rendering language, but it is not the only authoring or simulation representation the kernel should understand. The long-term architecture target is a hybrid, multi-engine kernel that can host multiple geometry representations behind stable contracts.

Planned representation families:

1. Polygon and subdivision surfaces for broad compatibility, games, and animation.
2. Parametric curve and surface representations for CAD-grade precision.
3. Implicit and signed-distance-field representations for booleans, sculpting, lattices, and field-driven design.
4. Voxels and volume grids for simulation, analysis, and AI-friendly spatial data.
5. Point-cloud and radiance-field domains such as Gaussian splats for scanned or neural scene capture.

Architecture rules for this direction:

1. No single representation should leak assumptions into the whole kernel.
2. Shared systems such as scene graph, evaluation, rendering, scripting, and asset versioning must remain representation-agnostic where practical.
3. Conversion boundaries must be explicit, testable, and loss-aware.
4. Mesh remains the baseline render and interchange path during early milestones, but it is a foundation slice, not the end-state product boundary.

## UX Doctrine: Progressive Disclosure

This project adopts progressive disclosure as a hard product principle.

Default experience (new user):

1. Guided introduction on first launch.
2. Lean creation start with 6 to 10 core primitives.
3. Immediate access to core modeling actions (inset, extrude, transform) without UI noise.

Advanced experience (expert user):

1. Large settings and preferences surface for deep control.
2. User-defined workspace layouts, profile presets, and quick-access tool surfacing.
3. Optional exposure of advanced systems by domain and context, not by default.

Design rules:

1. Context over clutter: show tools relevant to current selection/mode.
2. Process-first language: prefer user-intent phrasing where possible.
3. Customization without lock-in: all defaults can be overridden by profiles.
4. No regressions in onboarding clarity when adding advanced features.

## Execution Ratio (Monthly)

Every month follows the same effort split:

1. 65% feature implementation
2. 25% tests and regression hardening
3. 10% docs and design notes

## 12-Month Roadmap (Scope-Safe)

## Month 1: Kernel Baseline Lock

Goals:

1. Freeze v0 public API boundaries for render, scene, camera, and resource handles.
2. Define performance and determinism baselines on Null and Vulkan.
3. Ship one kernel health dashboard in CI: build, tests, perf smoke, memory leaks.
4. Publish UX baseline charter for progressive disclosure and customization boundaries.

Exit criteria:

1. Zero ambiguous ownership for current modules.
2. Reproducible baseline metrics.
3. UX baseline charter merged with acceptance checks for first-launch simplicity.

## Month 2: Geometry Core v0

Goals:

1. Add robust mesh data model primitives: topology validation, attribute channels, stable IDs.
2. Implement first geometry ops set: transform, merge, split, weld, normals/tangents rebuild.
3. Add deterministic geometry regression suite with golden topology checks.
4. Keep the mesh slice aligned with the broader multi-engine plan so later NURBS, implicit, voxel, and neural representations can plug in without core rewrites.

Exit criteria:

1. Geometry ops are script-callable.
2. Geometry ops are deterministic across repeated runs.

## Month 3: Render Pipeline v1

Goals:

1. Finalize deferred baseline: GBuffer, shadow chain, composite contracts, descriptor lifecycle.
2. Add render graph validation hooks for pass ordering and resource state transitions.
3. Add frame capture metadata export for debugging and CI artifacts.

Exit criteria:

1. Stable multi-frame rendering with deterministic descriptor/state behavior.

## Month 4: Parametric Foundation

Goals:

1. Introduce constraint graph core and parametric entity representation.
2. Implement solver v0 for basic geometric constraints.
3. Add serialization for parametric graph and replay tests.

Exit criteria:

1. A simple parametric model can be evaluated, saved, and reloaded deterministically.

## Month 5: Modeling Workflow Slice 1

Goals:

1. Build one end-to-end workflow: hard-surface blockout to clean mesh output.
2. Add boolean v0, bevel/chamfer v0, remesh v0 with robust failure diagnostics.
3. Integrate viewport overlays for topology and diagnostics data (kernel-generated, UI-consumed).
4. Ship first "simple by default" modeling shell with guided intro and lean starter toolset.

Exit criteria:

1. One daily-usable modeling slice from import to export.
2. New-user flow can complete first model without enabling advanced panels.

## Month 6: Asset and Pipeline Core

Goals:

1. Introduce scene/package format v0 with versioning and migration hooks.
2. Add import/export adapters v0 (one mesh format and one scene format first).
3. Add asset dependency graph and deterministic load order.

Exit criteria:

1. Round-trip scene fidelity for target baseline formats.

## Month 7: Animation and Rigging Core v0

Goals:

1. Implement skeleton, skinning, and pose evaluation core contracts.
2. Add animation clip sampling, blending primitives, and retargeting hooks.
3. Add deterministic animation playback tests across frame rates.

Exit criteria:

1. Rigged mesh playback is stable in viewport and headless runs.

## Month 8: Procedural and Evaluation Graph

Goals:

1. Add node-based evaluation runtime (no full UI in this phase), with typed compute nodes.
2. Implement geometry and animation nodes for procedural workflows.
3. Add cache invalidation and dependency-cycle detection.

Exit criteria:

1. Procedural graph executes deterministically with cache correctness.

## Month 9: Simulation Interfaces v0

Goals:

1. Define solver API contracts for rigid, cloth, and fluid integration points.
2. Implement rigid-body solver baseline with cacheable sim state format.
3. Add deterministic step/replay testing and rollback checks.

Exit criteria:

1. One simulation domain usable end-to-end with reproducible results.

## Month 10: Advanced Rendering Track

Goals:

1. Add high-value hybrid path features: temporal accumulation, denoising hooks, material extensions.
2. Introduce representation abstraction for point/raster/volume pipelines.
3. Start Gaussian Splatting as an optional rendering domain behind stable interfaces.
4. Validate coexistence of mesh-authored and scan-authored scene content under the same scene/runtime contracts.

Exit criteria:

1. Gaussian module can load, render, and coexist with mesh pipeline without core rewrites.

## Month 11: Scripting and Automation Layer

Goals:

1. Expose stable command API for geometry, render, animation, and asset operations.
2. Add scripting bindings v0 and deterministic script execution harness.
3. Add pipeline automation examples for batch workflows and cloud execution.

Exit criteria:

1. Full sample pipeline runs via script only, with no manual editor dependency.

## Month 12: Alpha Kernel Release

Goals:

1. Stabilize API version 1.0-alpha and publish compatibility guarantees.
2. Run reliability sprint: crash triage, memory/perf tuning, determinism gaps.
3. Publish capability report and next-year roadmap based on measured gaps.

Exit criteria:

1. Alpha-grade kernel with one production-relevant end-to-end DCC slice.

## Start Month 1 Now (Measurable Baselines)

This section is active immediately.

## Deliverables

1. API boundary freeze record:
   - Explicitly list v0 public surface roots under src/kernel/include/nexus.
   - Record change policy: additive-first, no silent breakage.
2. Baseline benchmark matrix:
   - Null backend: frame time baseline, deterministic output checks, test pass rate.
   - Vulkan backend: frame time baseline, startup/shader path smoke checks.
3. CI kernel health dashboard:
   - Build status
   - Test status
   - Perf smoke status
   - Memory/lifetime checks status
4. Ownership lock:
   - Every active module mapped to a primary owner domain per [Kernel Capability Map](kernel-capability-map.md).
5. UX baseline charter:
   - Define first-launch screen behavior and starter tool policy.
   - Define expert-surface customization policy and profile model.

## Baseline Metrics (Initial Targets)

1. Build reliability:
   - 100% successful CI builds on required targets.
2. Test reliability:
   - 100% pass on required test gates.
   - No flaky tests in required suites.
3. Determinism:
   - Null backend runs produce stable behavior signatures across repeated runs.
4. Perf smoke:
   - Track median frame time for selected smoke scenes and report trend deltas.
5. Memory/lifetime:
   - No unbounded resource growth across fixed-length frame loops.
6. Onboarding clarity:
   - First-launch path exposes only starter primitives and core modeling operations.
   - Guided introduction path is present and runnable.

## Month 1 Done Criteria

Month 1 is complete only when all are true:

1. Public API v0 freeze document is merged.
2. Baseline benchmark data is captured and reproducible.
3. CI health dashboard is visible to contributors.
4. Module ownership has zero ambiguous areas.
5. UX baseline charter and onboarding checks are merged.

## Month 2 Single Geometry Slice Decision

Before any new major rendering branch, the project commits to one geometry slice:

Geometry Slice: Deterministic Mesh Foundation v0

Execution checklist: [month-2-geometry-checklist.md](month-2-geometry-checklist.md)

In scope:

1. Mesh topology validation primitives (manifold checks, index validity, section bounds).
2. Attribute channel framework (normals, tangents, UV sets, color channels).
3. Stable mesh element IDs for deterministic references across edits.
4. Core operations: transform, merge, split, weld, normals/tangents rebuild.
5. Script-callable API hooks for each operation.
6. Golden topology regression tests for repeatable outputs.

Out of scope for Month 2:

1. New major rendering branch features.
2. Full CAD solver work.
3. High-level UI authoring workflows.

Month 2 completion gate:

1. All listed operations pass deterministic golden tests.
2. Operations are callable from scripting layer contracts.
3. No new major rendering branch starts until this gate passes.

## Anti-Scope-Collapse Rules

These rules are mandatory for roadmap execution:

1. Only one major domain expansion every two months.
2. No new subsystem starts unless one current subsystem exits with defined done criteria.
3. Every feature PR declares owner domain per [kernel-capability-map.md](kernel-capability-map.md).
4. Test additions must be tied to shipped behavior; avoid speculative test growth.
5. Maintain a quarterly kill-list of deferred ideas to preserve focus.

## Governance Notes

1. This document sets execution direction. Detailed requirements remain in [PRD](PRD.md), [SDD](SDD.md), and [FRD](FRD.md).
2. If any milestone changes, update this page and reference the design decision in [design-decisions.md](design-decisions.md).