# Month 12 Alpha Compatibility Report

Date: 2026-05-09

This report captures Month 12 alpha hardening outcomes for API stability, deterministic reliability, and compatibility status.

## Scope

1. API freeze audit
2. Determinism and perf reliability pass
3. Alpha compatibility status summary

## 1) API Freeze Audit

Public API roots audited:

1. src/kernel/include/nexus

Mechanism:

1. Manifest: tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt
2. Audit test: tests/kernel/test_ApiFreezeAudit.cpp

Baseline at report time:

1. Public header count: 39
2. Drift policy: any add/remove requires manifest update in same change

Result:

1. Pass

## 2) Determinism and Perf Reliability

Perf smoke harness:

1. Executable: build/tests/nexus_kernel_perf_smoke
2. Baseline test: KernelPerfSmoke.Null
3. New determinism gate: KernelPerfSmoke.Determinism

Determinism hardening added:

1. --determinism-runs N support in perf_smoke
2. Report fields: determinism_runs, determinism_consistent
3. Non-deterministic draw-call behavior returns non-zero exit code

Current test suite status:

1. 529 passed
2. 5 skipped (expected Vulkan capability-gated tests)
3. 0 failed

Determinism gate result:

1. determinism_runs=3
2. determinism_consistent=true

## 3) Compatibility Status (Alpha)

Domain compatibility snapshot:

1. Geometry: stable command/mesh workflows, deterministic regression coverage
2. Rendering: Null and Vulkan paths active, capability-gated Vulkan tests explicitly skipped when unsupported
3. Asset/Data: deterministic scene package load order and round-trip coverage
4. Animation: deterministic pose/evaluator core and retargeting tests
5. Procedural/Sim: deterministic graph and simulation interface tests
6. Scripting/Automation: script-only pipeline harness with cross-domain commands

## Compatibility Risks and Controls

Residual risks:

1. Vulkan capability variance across machines
2. Performance variance by host scheduling/load

Controls in place:

1. Null backend as deterministic CI baseline
2. Explicit capability-gated Vulkan skips
3. Determinism perf smoke gate
4. Public API manifest audit

## Exit Decision

Month 12 alpha hardening targets in this repository state are satisfied for:

1. API freeze audit enforcement
2. Determinism/perf reliability gate extension
3. Compatibility report publication
