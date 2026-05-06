# Nexus Engine Roadmap

Execution governance:

- Autonomous loop definition: `docs/AUTONOMOUS-WORKFLOW.md`
- Cycle log and milestone deltas: `docs/AUTONOMOUS-CYCLES.md`

## Phase 0: Foundation (In Progress)

- Engine/editor baseline in TypeScript.
- System extraction to reduce monolithic managers.
- Test coverage for command routing and update sequencing.

## Phase 1: Engine Design Lock

- Finalize architecture documents and subsystem contracts.
- Confirm hybrid runtime strategy and migration gate criteria.
- Define stress-test scenarios and measurable KPIs.
- Lock explicit Rust shift trigger and Rust-vs-TS subsystem ownership matrix.

## Phase 2: Core Engine Features

- Animation runtime MVP
- Physics runtime MVP
- Audio engine runtime MVP
- Asset and scene streaming baseline
- First large-scene performance benchmark suite

## Phase 3: Scripting Stack

- Visual scripting graph MVP
- Text scripting host interface
- First language integration (Lua candidate)
- Additional language adapters (Python, C#)

## Phase 3.5: Systems Breadth Expansion

- Navigation/pathfinding framework baseline
- AI behavior runtime baseline
- UI/HUD and event pipeline hardening
- Save/load and serialization policy
- VFX and timeline/cinematic baseline

## Phase 4: Native Acceleration

- Extract first hotspot subsystem to Rust/native module.
- Validate correctness parity and benchmark gains.
- Expand to additional hot paths only after clear wins.

## Phase 5: Production Readiness

- Documentation hardening and onboarding improvements.
- Stability and regression testing pipelines.
- SDK and API policy stabilization.

## Cross-Phase Deliverables

- Keep docs current with implementation.
- Keep tests aligned with architecture boundaries.
- Maintain reproducible build and profiling workflows.
- Keep systems landscape coverage current as new engine families are introduced.
- Track each autonomous cycle against roadmap milestones and update status deltas in `docs/AUTONOMOUS-CYCLES.md`.

## Active Autonomous Slice

- Current seed slice: Audio Engine MVP (`Phase 2: Core Engine Features`)
- Tracking record: `docs/AUTONOMOUS-CYCLES.md` (`NX-20260429-1`)
- Milestone delta: Audio engine runtime MVP moved from `not started` to `in progress` via interface + runtime stub + tests baseline.
- Current increment: gameplay event wiring + adapter boundary + snapshot/bus controls (`NX-20260429-2`).
- Current increment: asset registration + Babylon backend + timed snapshot interpolation (`NX-20260429-3`).
- Current increment: packaged audio assets + prewarm/unload lifecycle + expanded emitters (`NX-20260429-4`).
- Current increment: music-loop orchestration + temporary ducking overlays (`NX-20260429-5`).
- Current increment: authored synthesized assets + editor mixer controls + menu/cinematic music states (`NX-20260429-6`, validation pending).
