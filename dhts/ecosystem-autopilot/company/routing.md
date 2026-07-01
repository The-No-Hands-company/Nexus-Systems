# Nexus Autonomous Routing

## Routing Rules

1. Architecture boundary or cross-app dependency question -> Chief Architect.
2. Backlog order, sequencing, and milestone planning -> Program Manager + Planner.
3. Runtime/API implementation -> Backend.
4. Infrastructure/toolchain/runtime drift -> DevOps.
5. Verification, regressions, acceptance blocking -> QA.
6. Architecture docs and change records -> Docs.

## Dispatch Strategy

- Use `/dispatch <role>` to fetch the top ready task for a specialist role.
- Use `/dispatch <role> --start` to atomically assign and activate.
- Use `/work-order <task-id>` before implementation starts.

## Escalation Rules

- If a dependency blocks two or more critical tasks, escalate to Program Manager.
- If an interface contract changes across apps, escalate to Chief Architect.
- If gates fail in `pre-merge` or `release`, escalate to Engineering Lead.

## Release Routing

- Before release candidate creation: run `/gate pre-merge`.
- Before production release: run `/gate release`.
- After release gate pass: run `/pipeline release-readiness`.
