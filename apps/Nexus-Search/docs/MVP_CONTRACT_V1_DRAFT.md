# Nexus-Search MVP Contract v1 (Draft)

Status: draft
Updated: 2026-05-08

## Scope

This draft defines the minimum API contract required to pass item 5 gating for Nexus-Search.

## Endpoints

## GET /health

Purpose:
- liveness and service identity check

Response 200:

```json
{
  "status": "ok",
  "service": "nexus-search",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

## POST /api/v1/search/query

Purpose:
- accept a minimal search query and return ranked results

Request:

```json
{
  "query": "auth policy",
  "limit": 5,
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

Response 200:

```json
{
  "status": "ok",
  "results": [
    { "id": "res-000001", "title": "Auth Policy", "score": 0.98 }
  ]
}
```

## GET /api/v1/search/status

Purpose:
- search service status summary for local development

Response 200:

```json
{
  "status": "ok",
  "search": {
    "mode": "standalone",
    "queriesHandled": 0
  }
}
```

## Contract Rules

- query is required and must be non-empty
- limit is optional and defaults to 10 if omitted
- limit must be a positive integer when provided
- timestamp should be ISO 8601 string

## Nexus-Cloud Registration Compatibility (Draft)

Expected payload shape for registration contract endpoint:

```json
{
  "id": "nexus-search",
  "name": "Nexus Search",
  "serviceUrl": "http://localhost:<PORT>",
  "healthEndpoint": "/health",
  "capabilities": ["search-query", "ranked-results", "status-summary"]
}
```

## Smoke Test Target (Draft)

- verify GET /health returns 200 and service identity field
- verify POST /api/v1/search/query returns expected results keys
- verify GET /api/v1/search/status returns expected summary keys
