# Kernel Capability Map

This document defines where capabilities belong in Nexus Modeling so contributors can place new features in the correct subsystem with clear ownership.

## Purpose

- Prevent architecture drift and cross-layer shortcuts.
- Keep public APIs stable and intentional.
- Ensure each feature has a clear technical owner and test strategy.

## Layer Model

Nexus Modeling is organized into layered responsibilities:

1. Kernel Core
- Deterministic, reusable runtime capabilities.
- Public contracts under src/kernel/include/nexus.
- Internal implementation under src/kernel/src.

2. Product and Tooling Layer
- Authoring workflows, editor UX, and user-facing tool behavior.
- Built on top of kernel contracts.

3. Integration Layer
- Import/export bridges, scripting adapters, cloud orchestration, plugin hosts.
- Depends on kernel APIs and product workflows.

## Capability Domains and Ownership

| Domain | In Kernel Scope | Out of Kernel Scope | Primary Owner | Secondary Owner | Primary API Roots |
|---|---|---|---|---|---|
| Geometry | Topology, mesh ops, subdivision, parametric/B-Rep data model, implicit/field and voxel representation contracts, point/splat representation bridges, booleans, remesh, acceleration structures | Editor tools, manipulator UX, one-off interactive operators | Geometry Core | Runtime and Evaluation | src/kernel/include/nexus/render, src/kernel/include/nexus/gfx |
| Rendering | Device abstraction, frame scheduler, pass sequencing, transitions, material binding contracts, shadow/gbuffer/composite, mesh and RT pipeline creation | Render settings panels, viewport UI overlays, workflow presets | Render Core | Platform Backend | src/kernel/include/nexus/gfx, src/kernel/include/nexus/render |
| Animation | Time sampling primitives, transform evaluation contracts, skinning/pose data structures, dependency graph hooks | Timeline UX, graph editor UX, rig authoring widgets | Evaluation and Animation Core | Geometry Core | src/kernel/include/nexus/render |
| Simulation | Deterministic solver interfaces, step orchestration contracts, cacheable sim state formats | Simulation authoring UI, bake management UX, per-project presets | Simulation Core | Evaluation and Animation | src/kernel/include/nexus/render, src/kernel/include/nexus/gfx |
| Asset and Data | Scene serialization contracts, versioning, stable IDs/handles, import/export interface boundaries | App-level content browser UX, project templates, ad-hoc migration scripts | Asset and Data Core | Integration and Cloud | src/kernel/include/nexus, docs format specs |
| Scripting | Stable command API surfaces, deterministic execution hooks, typed bindings to kernel contracts | IDE-like script editor UX, package manager UI, tutorial workflows | Scripting and Integration Core | Domain owners per API | src/kernel/include/nexus public API surface |

## Ownership Rules

- Every feature PR must declare one primary domain owner.
- Cross-domain features must list a reviewer from each affected domain.
- If ownership is unclear, the change is blocked until the capability is mapped.

## Boundary Rules

1. Public API boundary
- Anything exposed under src/kernel/include/nexus is a contract and must be version-safe.
- Internal helper code must not leak into public headers.

2. Runtime determinism boundary
- Kernel behavior must remain deterministic on Null backend for CI and headless workflows.
- Non-deterministic behavior must be isolated behind optional feature flags.

3. Rendering authority boundary
- Scheduler path is authoritative for production rendering flow.
- Non-scheduler path is compatibility fallback and is not feature parity target.

4. Synchronization and lifetime boundary
- Render features must define explicit transitions and ownership.
- Resource lifecycle behavior requires regression tests.

## Placement Decision Checklist

When adding a feature, answer in order:

1. Is this reusable runtime logic across tools and workflows?
- Yes: kernel domain implementation.
- No: product/tooling layer.

2. Does it require stable API contracts for integrations or plugins?
- Yes: kernel public API.
- No: internal implementation or app layer.

3. Does it impact synchronization, resource transitions, or deterministic execution?
- Yes: kernel render/runtime ownership and mandatory behavior tests.

4. Is this primarily UX/editor workflow?
- Yes: outside kernel; consume kernel APIs.

## Feature Routing Examples

- Cascaded shadow maps with light matrix contracts: Rendering domain in kernel.
- Bone transform evaluation and skinning buffer contract: Animation domain in kernel.
- Representation bridge from implicit field asset to mesh render proxy: Geometry domain in kernel.
- Gaussian splat scene payload routed through shared scene/runtime contracts: Geometry and Rendering domain review.
- Timeline scrubber and keyframe panel: Product layer, not kernel.
- New importer for external format: Integration layer with Asset domain review.
- Python script command binding for mesh boolean operation: Scripting layer with Geometry and Scripting domain owners.

## Test Ownership Matrix

| Domain | Required Test Type | Minimum Location |
|---|---|---|
| Geometry | Behavior and regression tests | tests/kernel |
| Rendering | Pass ordering, transitions, lifecycle regressions | tests/kernel |
| Animation | Deterministic evaluation and transform invariants | tests/kernel |
| Simulation | Deterministic step and cache consistency | tests/kernel |
| Asset and Data | Round-trip serialization and schema version migration | tests/kernel or integration suites |
| Scripting | Binding contract and deterministic command execution | tests/kernel or integration suites |

## Documentation Ownership

- Domain owners update docs for architecture-impacting changes in the same PR.
- Update this capability map when a new subsystem or boundary is introduced.
- If a feature crosses domains, record the chosen ownership split in docs design notes.

## Non-Goals

- This map does not define UI roadmap or release scheduling.
- This map does not replace PRD, SDD, or FRD. It complements them by defining technical placement and ownership.
