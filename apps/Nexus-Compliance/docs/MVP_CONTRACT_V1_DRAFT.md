# Nexus-Compliance MVP Contract v1 (Draft)

Status: draft
Updated: 2026-05-08

## Scope

This draft defines the minimum API contract required to pass item 2 gating for Nexus-Compliance.

## Endpoints

## GET /health

Purpose:
- liveness and service identity check

Response 200:

```json
{
  "status": "ok",
  "service": "nexus-compliance",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

## GET /api/v1/compliance/status

Purpose:
- compliance status summary for local development

Response 200:

```json
{
  "status": "ok",
  "compliance": {
    "mode": "standalone",
    "eventsReceived": 0,
    "violationsOpen": 0
  }
}
```

## POST /api/v1/compliance/events

Purpose:
- accept a minimal compliance event payload from ecosystem services

Request:

```json
{
  "source": "nexus-auth",
  "policy": "auth.mfa.required",
  "outcome": "pass",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

Response 202:

```json
{
  "status": "accepted",
  "id": "cmp-000001"
}
```

## Contract Rules

- source is required and must be non-empty
- policy is required and must be non-empty
- outcome is required and must be one of: pass, fail, warn
- timestamp should be ISO 8601 string

## Nexus-Cloud Registration Compatibility (Draft)

Expected payload shape for registration contract endpoint:

```json
{
  "id": "nexus-compliance",
  "name": "Nexus Compliance",
  "serviceUrl": "http://localhost:<PORT>",
  "healthEndpoint": "/health",
  "capabilities": ["compliance", "policy-events", "status-summary"]
}
```

## Smoke Test Target (Draft)

- verify GET /health returns 200 and service identity field
- verify POST /api/v1/compliance/events accepts valid payload with 202
- verify GET /api/v1/compliance/status returns expected summary keys
