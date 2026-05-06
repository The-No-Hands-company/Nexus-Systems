# Physics System Plan

## Goals

- Robust, predictable, and scalable physics simulation.
- Deterministic stepping model suitable for gameplay consistency.
- Support both arcade and simulation-oriented use cases.

## Required Features

- Rigid body dynamics
- Collision detection and response
- Character controller support
- Trigger volumes and raycasts
- Continuous collision detection for fast objects
- Constraint/joint support

## Architecture

- Physics interface abstraction in engine systems.
- Pluggable backend strategy for implementation flexibility.
- Fixed timestep simulation with interpolation/extrapolation strategy.

## Determinism Considerations

- Fixed simulation step duration.
- Stable update ordering.
- Controlled floating-point behavior where possible.

## Engine Integration

- Transform sync policy between render and physics worlds.
- Event bridge for collision and trigger callbacks.
- Query API for gameplay and scripting systems.

## Performance Strategy

- Broadphase partitioning strategy for large scenes.
- Sleeping and activation policies.
- Batched queries where applicable.
- Native acceleration for heavy collision workloads.

## Backend Evaluation

- Prototype backend in current TS stack for iteration.
- Evaluate long-term backend for native core migration.
- Keep interface stable to minimize gameplay-side churn.

## Milestones

1. Stable rigid body and collision baseline.
2. Character controller and interaction queries.
3. Deterministic fixed-step integration with gameplay loop.
4. Optimization and stress test pass with large object counts.
