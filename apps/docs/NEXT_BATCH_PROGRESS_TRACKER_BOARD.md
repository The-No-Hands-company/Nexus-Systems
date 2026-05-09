# Next Batch Progress Tracker Board

Generated: 2026-05-08
Scope: Recommended next batch (10 items)
Tracking mode: MVP gate completion for each item

## Status Key

- not-started: no validated MVP gate outputs
- gating-started: baseline and contract gate opened
- in-progress: implementation underway with partial outputs validated
- mvp-complete: MVP gate outputs fully validated
- beyond-mvp: existing project exceeds baseline, alignment-only work

## Board

| # | App | Current status | Maturity signal from repo | Next validation gate |
|---|---|---|---|---|
| 1 | Nexus-Monitor | mvp-complete | runtime shell exists with health, ingest, status, and passing smoke test | monitor follow-ups only: invalid-ingest test + cloud heartbeat hook |
| 2 | Nexus-Compliance | mvp-complete | runtime shell exists with health, events, status, and passing smoke test | keep compliance follow-ups scoped (invalid payload + cloud heartbeat hook) |
| 3 | Nexus-AI-Hub | mvp-complete | runtime shell exists with health, route, providers-status, and passing smoke test | keep AI-Hub follow-ups scoped (invalid route payload + cloud heartbeat hook) |
| 4 | Nexus-Files | mvp-complete | runtime shell exists with health, files write/list, and passing smoke test | keep Files follow-ups scoped (invalid write payload + cloud heartbeat hook) |
| 5 | Nexus-Search | mvp-complete | runtime shell exists with health, query, status, and passing smoke test | keep Search follow-ups scoped (invalid query payload + cloud heartbeat hook) |
| 6 | Nexus-IDE | mvp-complete | runtime shell exists with health, ide-session, status, and passing smoke test | begin Nexus-API gating |
| 7 | Nexus-API | mvp-complete | runtime shell exists with health, route, gateway-status, and passing smoke test | begin Nexus-Testing gating |
| 8 | Nexus-Testing | mvp-complete | runtime shell exists with health, test-run submit, status, and passing smoke test | begin Nexus-Forge alignment |
| 9 | Nexus-Forge | beyond-mvp | large runtime/codebase and test suite already present | aligned: health delta fixed, cloud reg and heartbeat deltas documented, interop smoke pinned |
| 10 | Nexus-Systems-API SDK/spec hardening | mvp-complete | cloud-internal stream with contracts/examples/tests present | batch service registry pinned, compatibility matrix documented, 4/4 contract tests pass |

## Weekly Update Template

```md
## Batch update YYYY-MM-DD

### Completed this cycle
- item

### Newly opened blockers
- app: blocker id + one-line impact

### Contracts pinned this cycle
- app: contract + location

### Next cycle focus
1. app gate
2. app gate
```
