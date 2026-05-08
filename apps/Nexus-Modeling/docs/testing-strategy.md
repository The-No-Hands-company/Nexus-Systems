# Testing Strategy

This document defines how Nexus Modeling validates kernel quality as the codebase scales.

## Goals

- Catch regressions in correctness-critical systems early.
- Keep test execution fast enough for local development loops.
- Ensure GPU paths remain verifiable in CI/headless environments.
- Grow coverage with feature complexity, not only line count.

## Current baseline

- Framework: GoogleTest
- Discovered tests: 281
- Scope currently covered:
  - Type system and flag semantics
   - Geometry mesh and upload contracts
  - Camera math and inverse path
  - Scene graph create/find/remove/traversal behavior
  - Null backend lifecycle and command API no-crash coverage
  - RenderContext creation and queue submission wrappers
   - Renderer behavior and deterministic regression coverage
  - Vulkan shader/pipeline/frame-scheduler integration tests (headless-aware)

## Test layers

1. Fast unit tests
   - Pure logic, math, and data structure semantics.
   - No external runtime dependency.

2. Backend contract tests
   - Validate that IDevice and ICommandBuffer contracts are honored across backends.
   - Null backend acts as always-on CI baseline.

3. Vulkan integration tests
   - Exercise initialization, shader/pipeline creation, and frame scheduling.
   - Headless-capable where possible.

4. Scenario/regression tests
   - Reproduce previously fixed bugs.
   - Include synchronization and transition ordering where practical.

## Required local quality gates

Run before merging behavior changes:

1. Build:
   - cmake --build build -j$(nproc)
2. Tests:
   - ctest --test-dir build --output-on-failure

## Expansion roadmap

1. Renderer behavior tests
   - Validate geometry pass draw path and composite path contracts.
   - Add assertions for pass ordering and stats progression.

2. Synchronization tests
   - Add targeted transition and barrier regression cases.

3. Resource lifecycle tests
   - Stress create/destroy cycles and resize churn.

4. Descriptor/material tests
   - Once descriptor binding API lands, add deterministic material sampling tests.

5. Golden output tests
   - Introduce deterministic image/hash checks for selected render scenarios.

## Authoring guidance

- Name tests by behavior, not implementation detail.
- Keep assertions specific and deterministic.
- Avoid over-mocking; prefer real interface interactions where possible.
- Add tests in the same change that introduces behavior changes.
