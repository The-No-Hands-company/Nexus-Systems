# Next Batch Item 8 — Nexus-Testing Gating

Generated: 2026-05-08
Status: mvp-complete

## Gate Scope

Nexus-Testing is the ecosystem test orchestration service. It accepts test job submissions,
tracks run state, and surfaces results. MVP scope is the minimal test-runner surface: health,
test job trigger, and run status.

## Contract Baseline

### Service identity
- name: nexus-testing
- role: test orchestration service
- default port: 3037

### Endpoints (MVP v1)

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness probe — returns service identity and uptime |
| POST | /api/v1/testing/runs | Submit a test job |
| GET | /api/v1/testing/status | Return runner counters (jobs submitted, passed, failed, uptime) |

### POST /api/v1/testing/runs — request shape
```json
{
  "suite": "nexus-monitor-smoke",
  "target": "nexus-monitor",
  "tags": ["smoke", "contract"]
}
```

### POST /api/v1/testing/runs — response shape
```json
{
  "ok": true,
  "runId": "uuid",
  "suite": "nexus-monitor-smoke",
  "status": "queued"
}
```

### GET /api/v1/testing/status — response shape
```json
{
  "service": "nexus-testing",
  "submittedTotal": 0,
  "passedTotal": 0,
  "failedTotal": 0,
  "uptimeSeconds": 0
}
```

## Cloud Registration Compatibility

- Registers with Nexus-Systems-API at startup
- Cloud registration path: POST /api/v1/systems/register
- Registration payload: `{ "service": "nexus-testing", "port": 3037, "role": "test-runner" }`

## Smoke Test Plan

1. GET /health → 200 + `{ service: "nexus-testing" }`
2. POST /api/v1/testing/runs with valid suite → 200 + `{ ok: true, status: "queued" }`
3. GET /api/v1/testing/status → 200 + `{ service: "nexus-testing", submittedTotal: ≥1 }`

## Gating Checklist

- [x] test job model defined
- [x] minimal test-run trigger/status contract documented
- [x] /health endpoint contract documented
- [x] one smoke contract test planned and scoped
- [x] cloud registration compatibility documented

## Evidence

- Gate opened: 2026-05-08
- Runtime shell: Nexus-Testing/src/server.js
- Smoke test: Nexus-Testing/tests/testing.smoke.test.js
- Contract draft: Nexus-Testing/docs/MVP_CONTRACT_V1_DRAFT.md
