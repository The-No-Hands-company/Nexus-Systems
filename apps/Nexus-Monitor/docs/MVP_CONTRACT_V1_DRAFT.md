# Nexus-Monitor MVP Contract v1 (Draft)

Status: draft
Updated: 2026-05-08

## Scope

This draft defines the minimum API contract required to pass item 1 gating for Nexus-Monitor.

## Endpoints

## GET /health

Purpose:
- liveness and service identity check

Response 200:

```json
{
  "status": "ok",
  "service": "nexus-monitor",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

## GET /api/v1/monitor/status

Purpose:
- monitor status summary for local development

Response 200:

```json
{
  "status": "ok",
  "monitor": {
    "mode": "standalone",
    "eventsReceived": 0,
    "heartbeatsReceived": 0
  }
}
```

## POST /api/v1/monitor/ingest

Purpose:
- accept a minimal event or heartbeat from ecosystem services

Request:

```json
{
  "source": "nexus-auth",
  "kind": "heartbeat",
  "state": "ok",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

Response 202:

```json
{
  "status": "accepted",
  "id": "evt-000001"
}
```

## Contract Rules

- source is required and must be non-empty
- kind is required and must be one of: heartbeat, alert, event
- timestamp should be ISO 8601 string
- state is optional for event kind and required for heartbeat kind

## Nexus-Cloud Registration Compatibility (Draft)

Expected payload shape for registration contract endpoint:

```json
{
  "id": "nexus-monitor",
  "name": "Nexus Monitor",
  "serviceUrl": "http://localhost:<PORT>",
  "healthEndpoint": "/health",
  "capabilities": ["monitor", "heartbeat-ingest", "status-summary"]
}
```

## Smoke Test Target (Draft)

- verify GET /health returns 200 and service identity field
- verify POST /api/v1/monitor/ingest accepts valid payload with 202
- verify GET /api/v1/monitor/status returns expected summary keys
