# Nexus API Docs

## Architecture Note

Nexus-API is a thin Node.js ESM gateway. It receives inbound HTTP requests and routes them to
downstream Nexus services. In MVP scope it stubs downstream dispatch and tracks request counters.

## Contract

- MVP contract draft: MVP_CONTRACT_V1_DRAFT.md
- Gating tracker: ecosystem docs/NEXT_BATCH_ITEM7_NEXUS_API_GATING.md

## Endpoint Surface (MVP v1)

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness probe |
| POST | /api/v1/gateway/route | Route request to downstream service |
| GET | /api/v1/gateway/status | Gateway counters |

## Smoke Test Evidence

- 2026-05-08: 3/3 pass — tests/api.smoke.test.js
