# Release Notes Draft — Nexus Modeling Kernel 1.0-alpha

Status: Tag-ready draft
Target tag: v1.0.0-alpha
Date: 2026-05-09

## Summary

Nexus Modeling kernel reaches 1.0-alpha baseline with stabilized public API auditing, deterministic headless validation gates, and full vertical slices across geometry, rendering, asset pipelines, animation, procedural evaluation, simulation, and scripting automation.

## Highlights

1. Public API freeze enforcement moved from policy-only to manifest-backed automated audit.
2. Month 5 through Month 11 feature slices are landed and validated in one coherent test baseline.
3. Month 12 alpha hardening introduces deterministic perf smoke gate and compatibility reporting.
4. Script-only automation pipeline is available for cross-domain command execution in headless environments.

## Major Additions Since Baseline

### Geometry and Modeling

1. Weighted tangent accumulation and explicit bitangent reconstruction helper.
2. Boolean v0 diagnostics hardening with non-manifold and degenerate detection.
3. Modeling operations: bevel/chamfer, extrude, inset faces, remesh.
4. Mesh diagnostics overlay and mesh export (OBJ/PLY).

### Asset and Data

1. SceneAsset package format with versioning and migration hooks.
2. SceneAsset importer with deterministic dependency-driven load ordering.
3. Asset dependency graph validation and ordering contracts.

### Animation and Rigging

1. Animation serialization contracts and deterministic replay coverage.
2. Skeleton retargeting hooks and mapping behavior tests.

### Procedural, Simulation, and Rendering Extensions

1. EvalGraph deterministic execution and cache behavior coverage.
2. SimulationCore interface baseline with deterministic step/replay tests.
3. Gaussian splatting and temporal accumulation added to advanced rendering track.

### Scripting and Automation

1. Stable command registry and script batch harness across geometry/render/animation/asset domains.
2. Script-driven end-to-end pipeline test coverage in kernel test suite.

## Reliability and Hardening

1. API freeze alpha audit:
   - Manifest: tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt
   - Gate test: tests/kernel/test_ApiFreezeAudit.cpp
2. Determinism perf gate:
   - CTest: KernelPerfSmoke.Determinism
   - Harness: tests/kernel/perf_smoke.cpp --determinism-runs
3. Alpha compatibility report published:
   - docs/month-12-alpha-compatibility-report.md

## Validation Snapshot

1. Build gate: cmake --build build -j$(nproc)
2. Full suite: 529 passed, 5 skipped, 0 failed
3. Expected skips: Vulkan capability-gated tests on unsupported environments
4. Perf determinism gate: determinism_consistent=true (3 runs)

## Breaking Changes

No intentional breaking public API removals in this alpha window.

## Known Constraints

1. Vulkan mesh shader and ray tracing tests are capability-gated and may skip on hardware/driver combinations without support.
2. OpenImageDenoise is optional; when unavailable, OIDN-dependent paths remain disabled.

## Upgrade and Integration Notes

1. Treat all headers under src/kernel/include/nexus as audited alpha contract surface.
2. Additive API changes require manifest update and behavior tests in the same change.
3. Use tools/release_gate_alpha.sh to generate a reproducible signoff report before tagging.

## Suggested Tag Checklist

1. Ensure working tree is clean except intentionally excluded local workspace files.
2. Run tools/release_gate_alpha.sh and archive generated signoff artifact.
3. Review docs/month-12-alpha-compatibility-report.md for latest baseline metrics.
4. Create annotated tag v1.0.0-alpha referencing this release note draft.
