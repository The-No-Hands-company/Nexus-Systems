# Autonomous Role Templates

Use these templates at the start and end of each autonomous cycle.

## Role Assignment Sheet

- Cycle ID:
- Planner:
- Implementer:
- Reviewer:
- Docs Keeper:
- Performance Analyst:

## Planner Template

- Objective:
- Scope (bounded):
- Non-goals:
- Acceptance criteria:
  1.
  2.
  3.
- Roadmap milestone mapping:
- Task breakdown:
  1.
  2.
  3.

## Implementer Handoff Template

- Tasks completed:
  1.
  2.
  3.
- Files changed:
- Interface or boundary changes:
- Known risks:
- Quality gate pre-check status:
  - `npm run type-check`:
  - `npm test -- --run`:

## Reviewer Template

- Correctness findings:
- Regression risks:
- Architecture drift check:
- Decision:
  - approved
  - approved with follow-ups
  - rejected
- Required follow-ups:

## Docs Keeper Template

- Docs updated:
- Architecture/system changes documented:
- Systems index updates required:
- Roadmap/cycle tracker updates completed:

## Performance Analyst Template

- Performance assumptions touched:
- Hotspot notes:
- TS vs Rust/native ownership implications:
- Benchmark/profiling follow-up actions:

## Final Cycle Completion Checklist

1. Role assignment sheet completed.
2. Quality gates passed:
   - `npm run type-check`
   - `npm test -- --run`
3. Docs updated for architecture/system changes.
4. Cycle entry updated in `docs/AUTONOMOUS-CYCLES.md`.
5. Milestone status delta recorded against `docs/ROADMAP.md`.
