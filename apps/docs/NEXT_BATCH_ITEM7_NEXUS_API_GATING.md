# Next Batch Item 7 — Nexus-API Gating

Generated: 2026-05-08
Status: mvp-complete

## Gate Scope

Nexus-API is the ecosystem API gateway and aggregator layer. It receives inbound requests from
external clients and internal services, applies auth passthrough, and routes to downstream Nexus
services. MVP scope is the minimal gateway surface: health, route dispatch, and gateway status.

## Contract Baseline

### Service identity
- name: nexus-api
- role: API gateway and aggregator
- default port: 3036

### Endpoints (MVP v1)

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness probe — returns service identity and uptime |
| POST | /api/v1/gateway/route | Route a request to a downstream Nexus service |
| GET | /api/v1/gateway/status | Return gateway counters (requests routed, errors, uptime) |

### POST /api/v1/gateway/route — request shape
```json
{
  "target": "nexus-monitor",
  "method": "GET",
  "path": "/health",
  "payload": {}
}
```

### POST /api/v1/gateway/route — response shape
```json
{
  "ok": true,
  "target": "nexus-monitor",
  "routed": true,
  "requestId": "uuid"
}
```

### GET /api/v1/gateway/status — response shape
```json
{
  "service": "nexus-api",
  "routedTotal": 0,
  "errorsTotal": 0,
  "uptimeSeconds": 0
}
```

## Cloud Registration Compatibility

- Registers with Nexus-Systems-API at startup
- Cloud registration path: POST /api/v1/systems/register
- Registration payload: `{ "service": "nexus-api", "port": 3036, "role": "gateway" }`

## Smoke Test Plan

1. GET /health → 200 + `{ service: "nexus-api" }`
2. POST /api/v1/gateway/route with valid target → 200 + `{ ok: true, routed: true }`
3. GET /api/v1/gateway/status → 200 + `{ service: "nexus-api", routedTotal: ≥1 }`

## Gating Checklist

- [x] API gateway/aggregation role defined
- [x] minimal routing/auth passthrough contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

## Evidence

- Gate opened: 2026-05-08
- Runtime shell: Nexus-API/src/server.js
- Smoke test: Nexus-API/tests/api.smoke.test.js
- Contract draft: Nexus-API/docs/MVP_CONTRACT_V1_DRAFT.md
