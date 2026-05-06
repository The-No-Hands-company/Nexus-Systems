# Nexus Engine Autonomous Workflow

## Purpose

Defines the standard autonomous execution loop for Nexus Engine and how each cycle is tracked against roadmap milestones.

Role and handoff templates are defined in `docs/AUTONOMOUS-ROLE-TEMPLATES.md`.

## Canonical Execution Loop

1. Task intake
2. Planning and role assignment
3. Implementation
4. Test and quality gates
5. Documentation update
6. Review and merge decision
7. Roadmap milestone status update

## Step Details

### 1. Task Intake

- Capture objective, constraints, and expected deliverables.
- Link to target docs/systems and affected modules.

### 2. Planning and Role Assignment

Assign roles from `AGENTS.md`:

- Planner
- Implementer
- Reviewer
- Docs Keeper
- Performance Analyst

Planner defines:

- Bounded scope
- Acceptance criteria
- Milestone mapping (`docs/ROADMAP.md`)

### 3. Implementation

- Implementer executes smallest viable increments.
- Keep system boundaries explicit.
- Prefer extracted systems over manager growth.

### 4. Test and Quality Gates

Mandatory per task:

1. `npm run type-check`
2. `npm test -- --run`
3. Docs update if architecture/system behavior changed

No task is complete unless all gates pass.

### 5. Documentation Update

Docs Keeper updates relevant files in `docs/` and indexes where needed.

At minimum for architecture/system changes:

- update source design docs
- update affected system-family doc(s)
- ensure docs index links are valid

### 6. Review and Merge Decision

Reviewer checks:

- behavioral correctness
- regression risk
- architecture consistency
- test/doc completeness

Decision states:

- approved
- approved with follow-ups
- rejected (with blocking findings)

### 7. Roadmap Tracking

Each cycle must include:

- milestone targeted
- status before and after cycle
- completed deliverables
- next dependency

## Cycle Record Template

Use this template for each autonomous cycle:

- Cycle ID: `NX-YYYYMMDD-<n>`
- Objective:
- Roles:
  - Planner:
  - Implementer:
  - Reviewer:
  - Docs Keeper:
  - Performance Analyst:
- Scope (bounded):
- Roadmap milestone(s):
- Tasks:
  1.
  2.
  3.
- Quality gate results:
  - `npm run type-check`: pass/fail
  - `npm test -- --run`: pass/fail
  - docs updated: yes/no
- Review outcome:
- Milestone status delta:
- Follow-up actions:

## Initial Bounded Vertical Slice (Cycle 1)

### Slice

Audio Engine MVP (planning -> interface -> runtime stub -> tests)

### Minimum Deliverables

1. Update `docs/AUDIO.md` with MVP acceptance criteria.
2. Define initial audio engine interface contract in code.
3. Add first runtime stub implementation wired to current architecture.
4. Add test coverage entry for audio interface/stub behavior.
5. Map progress to `Phase 2: Core Engine Features` in `docs/ROADMAP.md`.

## Anti-Drift Rules

- Do not start a new cycle while quality gates are red.
- Do not merge architecture-affecting code without docs updates.
- Do not mark roadmap progress without explicit cycle evidence.
