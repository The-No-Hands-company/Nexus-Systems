# AGENTS

Scope: `apps/Nexus-Engine`

This file defines execution rules for coding agents and contributors working in Nexus Engine.

## Core Principles

- Prioritize correctness, reproducibility, and performance-focused architecture.
- Treat the current TypeScript runtime as a production prototype, not the final performance ceiling.
- Keep architecture decisions documented in `docs/` as part of the same change set.
- Preserve compatibility expectations with surrounding Nexus ecosystem services where applicable.

## Required Workflow

1. Make focused, reversible changes.
2. Add or update tests for behavior changes.
3. Run `npm run type-check` and relevant tests.
4. Update documentation when introducing new systems or changing behavior.

## Autonomous Agent Roles and Ownership

Every autonomous cycle must assign all five roles explicitly.

### 1. Planner

Ownership:

- Defines cycle scope and acceptance criteria.
- Maps work items to roadmap milestones.
- Splits work into bounded tasks with explicit done conditions.

Required outputs:

- Cycle brief
- Task list
- Roadmap mapping

### 2. Implementer

Ownership:

- Executes code/doc changes for planned tasks.
- Keeps changes small and reversible.
- Ensures interfaces and contracts remain explicit.

Required outputs:

- Implementation diffs
- Notes on design tradeoffs

### 3. Reviewer

Ownership:

- Validates correctness and behavioral intent.
- Checks regressions and boundary safety.
- Enforces no hidden architecture drift.

Required outputs:

- Review findings
- Pass/fail decision with rationale

### 4. Docs Keeper

Ownership:

- Updates architecture/system docs for behavior or boundary changes.
- Keeps roadmap and systems docs aligned with implementation.

Required outputs:

- Doc change summary
- Updated links/indexes where needed

### 5. Performance Analyst

Ownership:

- Defines and validates performance assumptions.
- Identifies hotspots and migration candidates (TS vs Rust/native).
- Tracks perf risk notes per cycle.

Required outputs:

- Performance notes
- Profiling/benchmark follow-up actions

## Role Assignment Template

Use this template per cycle:

- Planner: `<name or agent-id>`
- Implementer: `<name or agent-id>`
- Reviewer: `<name or agent-id>`
- Docs Keeper: `<name or agent-id>`
- Performance Analyst: `<name or agent-id>`

## Architecture Direction

- The project is moving toward a hybrid runtime model.
- Performance-critical subsystems are expected to migrate to a native core (Rust-first candidate, alternatives allowed with documented rationale).
- High-level orchestration and tooling may remain in TypeScript where it improves iteration speed.

## System Extraction Policy

- Prefer small interaction systems over large manager conditionals.
- Keep `arpg-manager` orchestration-oriented.
- New command handling should be extracted into dedicated systems when complexity grows.

## Documentation Policy

When adding a major feature area, update at least one of:

- `docs/DESIGN.md`
- `docs/ARCHITECTURE-RUNTIME.md`
- `docs/SCRIPTING.md`
- `docs/ANIMATION.md`
- `docs/PHYSICS.md`
- `docs/ROADMAP.md`

## Testing Policy

- Unit tests for pure logic and helpers.
- Integration tests for command routing and interaction systems.
- Orchestration tests for manager update sequencing.

## Quality Gates (Mandatory Per Autonomous Task)

Autonomous tasks are not complete until all gates pass:

1. `npm run type-check`
2. `npm test -- --run`
3. Docs updated when architecture/system boundaries or behavior changed

If any gate fails, task status remains `incomplete`.

## Autonomous Workflow Entry Point

- Canonical loop and cycle templates live in:
	- `docs/AUTONOMOUS-WORKFLOW.md`
	- `docs/AUTONOMOUS-CYCLES.md`
	- `docs/AUTONOMOUS-ROLE-TEMPLATES.md`

## Vertical Slice Start Policy

- Each cycle starts with one bounded vertical slice.
- First mandated slice: Audio Engine MVP planning-to-stub path.
- Slice must include:
	- doc update
	- interface definition (or explicit placeholder contract)
	- initial runtime stub
	- test coverage entry

## Roadmap Tracking Requirement

- Every autonomous cycle must reference at least one milestone from `docs/ROADMAP.md`.
- Cycle output must include:
	- milestone reference
	- status delta (`not started` -> `in progress` -> `completed`)
	- next dependency

## Safety and Repo Hygiene

- Do not commit secrets, keys, or environment credentials.
- Keep generated artifacts and local environment files out of version control.
- Avoid destructive git operations unless explicitly requested.
