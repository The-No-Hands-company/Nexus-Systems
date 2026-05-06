# Nexus Engine Design Document

## Purpose

This document defines the design goals, architecture constraints, and system boundaries for Nexus Engine.

## Product Goals

- Support high-density scenes with very large model counts.
- Provide reusable engine systems independent of a single game genre.
- Maintain fast iteration workflows for gameplay and tools.
- Enable a migration path from browser-first prototype to high-performance hybrid runtime.

## Non-Goals (Current Phase)

- Building a full AAA editor in this phase.
- Solving distributed simulation in the first core milestone.
- Locking to one rendering backend forever.

## Design Pillars

- Performance by architecture, not only micro-optimization.
- Deterministic system boundaries and explicit data flow.
- Testability through extraction of interaction and orchestration logic.
- Extensibility for rendering, physics, animation, and scripting backends.

## Current Architecture Snapshot

- Engine loop and scene management in `src/engine`.
- Gameplay orchestration in `src/game`.
- Extracted feature systems in `src/systems`.
- Authoring helpers in `src/tools`.
- Type contracts in `src/types`.

## Target Architecture

- Thin orchestration layer calling dedicated systems.
- Command interaction systems for input-driven features.
- Native acceleration layer for hot paths.
- Unified asset and resource lifecycle management.

## Core Subsystems

- Rendering and scene graph
- ECS-style simulation update model
- Animation graph and runtime blending
- Physics integration and deterministic stepping
- Audio and event pipelines
- Scripting runtime bridge
- Asset import and streaming

## Performance Strategy

- Profile first, optimize second.
- Move CPU-bound hotspots into native modules.
- Use data-oriented layouts for simulation-heavy paths.
- Minimize per-frame allocations and dynamic dispatch overhead.

## Risks

- Complexity growth from dual-runtime architecture.
- Runtime mismatch between script and native world state.
- Tooling drift if docs are not updated during implementation.

## Success Criteria

- Stable frame times with large scene stress tests.
- Documented and testable system interfaces.
- Reproducible gameplay behavior across test runs.
- Measured gains from native acceleration milestones.
