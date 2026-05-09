# Nexus-API MVP Contract V1 Draft

Generated: 2026-05-08
Status: draft

## Service Identity

- name: nexus-api
- role: API gateway and aggregator
- default port: 3036
- runtime: Node.js ESM (node:http, no external deps at MVP)

## Endpoint Contracts

### GET /health

Response 200:
```json
{
  "service": "nexus-api",
  "status": "ok",
  "uptimeSeconds": 0
}
```

### POST /api/v1/gateway/route

Request body:
```json
{
  "target": "nexus-monitor",
  "method": "GET",
  "path": "/health",
  "payload": {}
}
```

Response 200:
```json
{
  "ok": true,
  "target": "nexus-monitor",
  "routed": true,
  "requestId": "uuid"
}
```

Response 400 (missing target):
```json
{ "error": "target is required" }
```

### GET /api/v1/gateway/status

Response 200:
```json
{
  "service": "nexus-api",
  "routedTotal": 0,
  "errorsTotal": 0,
  "uptimeSeconds": 0
}
```

## Cloud Registration

- Endpoint: POST /api/v1/systems/register (Nexus-Systems-API)
- Payload: `{ "service": "nexus-api", "port": 3036, "role": "gateway" }`

## Smoke Test Coverage

| Test | Endpoint | Assertion |
|------|----------|-----------|
| 1 | GET /health | status 200, service === "nexus-api" |
| 2 | POST /api/v1/gateway/route | status 200, ok === true, routed === true |
| 3 | GET /api/v1/gateway/status | status 200, routedTotal >= 1 |

## Versioning

- Contract version: v1
- Breaking change policy: new major version required for shape changes
