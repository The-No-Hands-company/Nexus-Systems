# Nexus Testing Docs

## Architecture Note

Nexus-Testing is a thin Node.js ESM test orchestration service. It receives job submissions,
assigns run IDs, tracks counters, and surfaces status. In MVP scope, dispatch is stubbed.

## Contract

- MVP contract draft: MVP_CONTRACT_V1_DRAFT.md
- Gating tracker: ecosystem docs/NEXT_BATCH_ITEM8_NEXUS_TESTING_GATING.md

## Endpoint Surface (MVP v1)

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness probe |
| POST | /api/v1/testing/runs | Submit a test job |
| GET | /api/v1/testing/status | Runner counters |

## Smoke Test Evidence

- 2026-05-08: 3/3 pass — tests/testing.smoke.test.js
