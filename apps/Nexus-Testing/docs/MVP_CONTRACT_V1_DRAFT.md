# Nexus-Testing MVP Contract V1 Draft

Generated: 2026-05-08
Status: draft

## Service Identity

- name: nexus-testing
- role: test orchestration service
- default port: 3037
- runtime: Node.js ESM (node:http, no external deps at MVP)

## Endpoint Contracts

### GET /health

Response 200:
```json
{
  "service": "nexus-testing",
  "status": "ok",
  "uptimeSeconds": 0
}
```

### POST /api/v1/testing/runs

Request body:
```json
{
  "suite": "nexus-monitor-smoke",
  "target": "nexus-monitor",
  "tags": ["smoke", "contract"]
}
```

Response 200:
```json
{
  "ok": true,
  "runId": "uuid",
  "suite": "nexus-monitor-smoke",
  "status": "queued"
}
```

Response 400 (missing suite):
```json
{ "error": "suite is required" }
```

### GET /api/v1/testing/status

Response 200:
```json
{
  "service": "nexus-testing",
  "submittedTotal": 0,
  "passedTotal": 0,
  "failedTotal": 0,
  "uptimeSeconds": 0
}
```

## Cloud Registration

- Endpoint: POST /api/v1/systems/register (Nexus-Systems-API)
- Payload: `{ "service": "nexus-testing", "port": 3037, "role": "test-runner" }`

## Smoke Test Coverage

| Test | Endpoint | Assertion |
|------|----------|-----------|
| 1 | GET /health | status 200, service === "nexus-testing" |
| 2 | POST /api/v1/testing/runs | status 200, ok === true, status === "queued" |
| 3 | GET /api/v1/testing/status | status 200, submittedTotal >= 1 |

## Versioning

- Contract version: v1
- Breaking change policy: new major version required for shape changes
