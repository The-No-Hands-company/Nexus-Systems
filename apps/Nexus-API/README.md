# Nexus API

Status: MVP shell implemented
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus-API is the ecosystem API gateway and aggregator layer. It receives inbound requests from
external clients and internal services, applies auth passthrough, and routes to downstream Nexus
services.

## Dual Mode Target

- Standalone: docker compose based local runtime
- Orchestrated: integrated with Nexus-Cloud via Nexus Systems API

## MVP Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness probe |
| GET | /api/v1/contract | Versioned gateway contract for clients and operators |
| POST | /api/v1/gateway/route | Route request to downstream service |
| GET | /api/v1/gateway/status | Gateway counters |

`/api/v1/contract` is the stable discovery entry point for this gateway. It
declares which routes are public and which require an operator credential. It
does not grant access; callers still authenticate each request according to the
route policy.

## Runtime

```sh
cd Nexus-API
npm install
node src/index.js
# or: PORT=3036 node src/index.js
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
- docs/NEXT_BATCH_ITEM7_NEXUS_API_GATING.md (in ecosystem docs/)
