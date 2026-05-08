# Month 4 Parametric Foundation Checklist

This checklist executes the Month 4 roadmap commitment in
[vision-and-roadmap.md](vision-and-roadmap.md).

Primary slice: Parametric Foundation.

## Scope Lock

In scope:

1. Constraint graph core and parametric entity representation.
2. Deterministic solver v0 for basic geometric constraints.
3. Deterministic graph serialization and replay tests.
4. Additional geometric constraints (coincident and axis-aligned distance).
5. Tiny sample generator for sketch-like constrained models.

Out of scope:

1. Advanced nonlinear constraints and optimization tuning.
2. Full editor/UI authoring tools for parametric workflows.
3. CAD-grade robustness and tolerance management.

## Work Packages

## R4.1 Constraint Graph Core

Status: Complete

Delivered:

1. Added public API in `nexus/parametric/ConstraintGraph.h`.
2. Implemented deterministic entity and multi-constraint IDs.
3. Implemented graph mutation primitives:
   - add/remove entity
   - add/remove distance constraint
   - add/remove coincident constraint
   - add/remove axis-aligned distance constraint
   - point update/query
4. Enforced graph validity by removing constraints that reference removed entities.

Done when:

1. Graph can represent and mutate basic parametric point networks deterministically.

## R4.2 Solver v0

Status: Complete

Delivered:

1. Added public solver API in `nexus/parametric/ParametricSolver.h`.
2. Implemented deterministic multi-constraint solve pass in
   `src/kernel/src/parametric/ParametricSolver.cpp`.
3. Added convergence and validation reporting:
   - converged flag
   - iterationsRan
   - maxConstraintError
   - errors

Done when:

1. A simple constrained model evaluates deterministically in headless test runs.

## R4.3 Serialization and Replay

Status: Complete

Delivered:

1. Added serializer API in `nexus/parametric/ParametricSerialization.h`.
2. Implemented text format `NEXUS_PARAM_GRAPH_V1` in
   `src/kernel/src/parametric/ParametricSerialization.cpp`.
3. Added deterministic replay tests validating:
   - round-trip serialization identity
   - solve replay determinism across repeated loads

Done when:

1. Parametric graph state can be saved and reloaded deterministically.

## R4.4 Constraint Expansion and Sketch Sample

Status: Complete

Delivered:

1. Added coincident and axis-aligned distance constraints to the public graph API.
2. Extended solver and serializer to process the new constraint types.
3. Added `ParametricSampleGenerator` in
   `src/kernel/src/parametric/ParametricSamples.cpp` that builds and solves a
   tiny rectangle-like constrained sketch model.
4. Added integration coverage validating sample generation + solve behavior.

Done when:

1. Additional constraint types and sample generator are deterministic and tested.

## Test Coverage

Implemented in `tests/kernel/test_ParametricFoundation.cpp`:

1. Graph add/remove and constraint lifecycle behavior.
2. Distance solver convergence for a basic constrained model.
3. Serialization round-trip identity.
4. Replay deterministic solve equivalence.
5. Invalid serialization header rejection.
6. Coincident constraint solve behavior.
7. Axis-aligned distance constraint solve behavior.
8. Serialization round-trip with expanded constraint set.
9. Sketch sample generator integration behavior.

## Month 4 Exit Gate

Month 4 is complete only when all are true:

1. Constraint graph core is public, deterministic, and tested.
2. Solver v0 evaluates a basic constrained model deterministically.
3. Graph serialization/reload path is deterministic and replay-tested.
4. Additional geometric constraints and sample generator are implemented and
   covered by deterministic tests.

Status: COMPLETE

Test baseline: 281 discovered / 281 passed / 0 failed.
