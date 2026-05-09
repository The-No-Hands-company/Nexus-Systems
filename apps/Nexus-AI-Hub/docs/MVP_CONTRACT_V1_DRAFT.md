# Nexus-AI-Hub MVP Contract v1 (Draft)

Status: draft
Updated: 2026-05-08

## Scope

This draft defines the minimum API contract required to pass item 3 gating for Nexus-AI-Hub.

## Endpoints

## GET /health

Purpose:
- liveness and service identity check

Response 200:

```json
{
  "status": "ok",
  "service": "nexus-ai-hub",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

## GET /api/v1/hub/providers/status

Purpose:
- provider status summary for local development

Response 200:

```json
{
  "status": "ok",
  "hub": {
    "mode": "standalone",
    "providers": [
      { "name": "local-default", "available": true }
    ]
  }
}
```

## POST /api/v1/hub/route

Purpose:
- accept a minimal routing request and return selected provider

Request:

```json
{
  "task": "summarize",
  "tenant": "nexus-internal",
  "priority": "normal",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

Response 200:

```json
{
  "status": "ok",
  "provider": "local-default",
  "routeId": "hub-000001"
}
```

## Contract Rules

- task is required and must be non-empty
- tenant is required and must be non-empty
- priority is required and must be one of: low, normal, high
- timestamp should be ISO 8601 string

## Nexus-Cloud Registration Compatibility (Draft)

Expected payload shape for registration contract endpoint:

```json
{
  "id": "nexus-ai-hub",
  "name": "Nexus AI Hub",
  "serviceUrl": "http://localhost:<PORT>",
  "healthEndpoint": "/health",
  "capabilities": ["ai-routing", "provider-status", "hub-orchestration"]
}
```

## Smoke Test Target (Draft)

- verify GET /health returns 200 and service identity field
- verify POST /api/v1/hub/route returns expected provider and routeId keys
- verify GET /api/v1/hub/providers/status returns expected summary keys
