# Nexus-Files MVP Contract v1 (Draft)

Status: draft
Updated: 2026-05-08

## Scope

This draft defines the minimum API contract required to pass item 4 gating for Nexus-Files.

## Endpoints

## GET /health

Purpose:
- liveness and service identity check

Response 200:

```json
{
  "status": "ok",
  "service": "nexus-files",
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

## GET /api/v1/files

Purpose:
- files listing summary for local development

Response 200:

```json
{
  "status": "ok",
  "files": [
    { "id": "file-000001", "name": "example.txt", "size": 128 }
  ]
}
```

## POST /api/v1/files

Purpose:
- accept a minimal file write metadata payload

Request:

```json
{
  "name": "example.txt",
  "contentType": "text/plain",
  "size": 128,
  "timestamp": "2026-05-08T00:00:00.000Z"
}
```

Response 202:

```json
{
  "status": "accepted",
  "id": "file-000001"
}
```

## Contract Rules

- name is required and must be non-empty
- contentType is required and must be non-empty
- size is required and must be a non-negative number
- timestamp should be ISO 8601 string

## Nexus-Cloud Registration Compatibility (Draft)

Expected payload shape for registration contract endpoint:

```json
{
  "id": "nexus-files",
  "name": "Nexus Files",
  "serviceUrl": "http://localhost:<PORT>",
  "healthEndpoint": "/health",
  "capabilities": ["file-write", "file-list", "metadata-index"]
}
```

## Smoke Test Target (Draft)

- verify GET /health returns 200 and service identity field
- verify POST /api/v1/files accepts valid payload with 202
- verify GET /api/v1/files returns expected listing keys
