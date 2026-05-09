# Nexus Testing

Status: MVP shell implemented
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus-Testing is the ecosystem test orchestration service. It accepts test job submissions,
tracks run state, and surfaces results.

## Dual Mode Target

- Standalone: docker compose based local runtime
- Orchestrated: integrated with Nexus-Cloud via Nexus Systems API

## MVP Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness probe |
| POST | /api/v1/testing/runs | Submit a test job |
| GET | /api/v1/testing/status | Runner counters |

## Runtime

```sh
cd Nexus-Testing
npm install
node src/index.js
# or: PORT=3037 node src/index.js
```

## Tests

```sh
npm test
```

## Layout

- src/: implementation code
- docs/: design and architecture notes
- tests/: automated tests

## Gate

- docs/MVP_CONTRACT_V1_DRAFT.md
- docs/NEXT_BATCH_ITEM8_NEXUS_TESTING_GATING.md (in ecosystem docs/)
