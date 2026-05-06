# Software Design Document (SDD)

## Scope

This document describes the technical implementation model for the Nexus Modeling kernel and contribution constraints.

## Architecture Overview

- Public API surface: src/kernel/include/nexus
- Internal implementation: src/kernel/src
- Test suites: tests/kernel
- Backends:
  - Vulkan backend for production rendering.
  - Null backend for deterministic CI/headless parity.

## Core Subsystems

1. Graphics Abstraction
- IDevice and ICommandBuffer contracts define backend-agnostic GPU operations.
- Opaque handle types provide stable API and serialization-safe identity.

2. Render Context
- Owns backend device, allocators, and frame lifecycle coordination.
- Supports backend selection and headless-safe operation.

3. Renderer
- Frame scheduler is the authoritative production path.
- Deferred flow includes geometry and lighting/composite passes.
- Explicit texture layout transitions and pass ordering are required.
- Deferred composite inputs can be queried as a compact descriptor via
  `Renderer::buildCompositePassBindingDesc()` for backend binding code.
- Shadow target state can be queried explicitly via
  `Renderer::buildShadowPassBindingDesc()` to validate owned shadow atlas
  lifetime, per-cascade pass descriptors, and sampling readiness across
  scheduler and resize/reset paths.
- Composite readiness diagnostics are exposed via
  `CompositePassBindingDesc::diagnose()` and `CompositeInputMissing`.
- Shadow lighting sampling inputs are exposed separately via
  `Renderer::buildShadowLightingBindingDesc()` to keep contracts explicit.

4. Scene and Camera
- Scene graph manages hierarchy, visibility, and traversal.
- Camera tracks per-frame projection/view state and temporal continuity data.

## Current Design Priorities

1. Shadow pass chain (targets, depth pass, lighting sampling).
2. Scheduler path remains authoritative for production rendering; non-scheduler path is de-scoped from feature parity.
3. Replace mesh shader/RT placeholders with full backend implementations.
4. Continue expanding descriptor/material validation and deterministic renderer regression coverage.

## Synchronization and Resource-Lifetime Rules

- Resource transitions/layouts must be explicit for pass boundaries.
- Destroy paths must clear internal ownership slots to prevent double free.
- Scheduler-recorded descriptor sets must stay alive until the owning frame slot fence retires.
- Scene reset and resize paths must clear shadow-pass targets before the next frame rebuilds them.
- Render pass sequencing guarantees must be verified by behavior tests.

## Testing and Verification Strategy

- Behavior tests for renderer pass sequence and transition invariants.
- Regression tests for Vulkan lifecycle and backend-specific defect classes.
- Required quality gates:
  1. cmake --build build -j$(nproc)
  2. ctest --test-dir build --output-on-failure

## Security and Operational Constraints

- Never commit credentials, tokens, or secrets.
- Avoid destructive repository operations unless explicitly requested.
