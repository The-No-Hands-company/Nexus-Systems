# Nexus-IDE MVP Contract v1 (Draft)

Status: draft
Updated: 2026-05-08

## Scope

This draft defines the minimum API contract required to pass item 6 gating for Nexus-IDE.

## Endpoints

## GET /health

Purpose:
- liveness and service identity check

Response 200:

```json
{
  "status": "ok",
  "service": "nexus-ide",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

## POST /api/v1/ide/session

Purpose:
- create a minimal IDE session record

Request:

```json
{
  "workspace": "nexus-default",
  "user": "system",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

Response 201:

```json
{
  "status": "ok",
  "sessionId": "ide-000001"
}
```

## GET /api/v1/ide/status

Purpose:
- IDE service status summary for local development

Response 200:

```json
{
  "status": "ok",
  "ide": {
    "mode": "standalone",
    "sessionsCreated": 0
  }
}
```

## Contract Rules

- workspace is required and must be non-empty
- user is required and must be non-empty
- timestamp should be ISO 8601 string

## Nexus-Cloud Registration Compatibility (Draft)

Expected payload shape for registration contract endpoint:

```json
{
  "id": "nexus-ide",
  "name": "Nexus IDE",
  "serviceUrl": "http://localhost:<PORT>",
  "healthEndpoint": "/health",
  "capabilities": ["ide-session", "command-bridge", "status-summary"]
}
```

## Smoke Test Target (Draft)

- verify GET /health returns 200 and service identity field
- verify POST /api/v1/ide/session returns expected sessionId
- verify GET /api/v1/ide/status returns expected summary keys
