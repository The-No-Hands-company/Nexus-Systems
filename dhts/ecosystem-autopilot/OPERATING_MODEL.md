# Nexus Ecosystem Autonomous Operating Model

This model turns `dhts/ecosystem-autopilot` into a company-grade delivery operating system for The No Hands Company.

## Purpose

- Drive deterministic execution order across the Nexus ecosystem.
- Preserve complete context from strategy to implementation.
- Keep specialist AI agents aligned to architecture, quality, and release gates.

## Enterprise Roles

- `executive` (`cto`): Defines objectives, budget, and risk profile.
- `architecture` (`chief-architect`): Owns ecosystem boundaries and integration contracts.
- `program` (`program-manager`): Owns dependency graph, sequencing, and schedule control.
- `delivery` (`engineering-lead`): Owns technical decomposition and review gates.
- `execution` (specialist roles in config): Implements and validates scoped work.

## Specialist Execution Roles

- `planner`: Shapes prerequisite-aware backlog and change plans.
- `backend`: Implements runtime behavior and API contracts.
- `devops`: Controls infrastructure consistency and operational guardrails.
- `qa`: Validates acceptance criteria and regression protection.
- `docs`: Maintains architecture and decision clarity.

## Task Contract

Each task should include:

- `id`, `title`, `lane`, `priority`, `owner`
- `dependsOn` (prerequisites)
- `what`, `why`, `how`, `acceptance`, `rollback`
- status lifecycle: `not-started` -> `in-progress` -> `done`

## Command Model

- `/status`: ecosystem control-plane status.
- `/board`: full task board with blocked/ready indicators.
- `/next [lane]`: next ready tasks by priority.
- `/dispatch <role> [lane] [--start]`: role-scoped assignment queue and optional auto-start.
- `/work-order <task-id>`: structured execution packet for the assigned role.
- `/handoff <task-id> [--out <path>]`: generate specialist handoff packet mapped to role-card prompts.
- `/decision <task-id> <summary> [--out <path>]`: append durable decision logs mapped to role responsibilities.
- `/brief`: executive delivery briefing with bottlenecks and readiness.
- `/org`: print organization model and role hierarchy.
- `/plan`: show ready-now and blocked maps by lane.
- `/scorecard [--json-out <path>] [--md-out <path>]`: generate ecosystem scorecard artifacts.
- `/run <workflow> [--dry-run]`: execute a single subsystem workflow.
- `/pipeline <name> [--dry-run]`: execute deterministic workflow chain.
- `/gate <name> [--dry-run]`: run company quality gates.

## Quality Controls

- `pre-merge` gate: `graph`, `nit`.
- `release` gate: `stack`, `graph`, `nit`, `docs`, `porter`.

## Cadence

- Daily: `stack`, `graph`, `nit`.
- Weekly: `porter`, `docs`.
- Release: `stack`, `graph`, `nit`, `docs`.

## Rollout Path

1. Keep seed backlog small but strictly prerequisite-driven.
2. Add tasks per bounded context using the task contract fields.
3. Enforce `/gate pre-merge` before merge operations.
4. Enforce `/gate release` for release preparation.
5. Track history and decisions as immutable logs in state/history events.

## CI Activation

- Active workflows are installed at `.github/workflows/nexus-autopilot-heartbeat.yml` and `.github/workflows/nexus-release-gate.yml`.
- Heartbeat now emits `reports/scorecards/latest.json` and `reports/scorecards/latest.md` artifacts each run.
