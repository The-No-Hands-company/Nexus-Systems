# Nexus Files

Status: MVP shell implemented
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus Files provides the first file metadata, write, and listing surface for ecosystem services.

This MVP gate focuses on:

- a minimal files service contract (write + list)
- a health/readiness surface for orchestration
- a contract-first shell compatible with Nexus-Cloud registration
- one smoke or contract test target

## Dual Mode Target

- Standalone: docker compose based local runtime
- Orchestrated: integrated with Nexus-Cloud via Nexus Systems API

## Initial Scaffold Layout

- src/: implementation code
- docs/: design and architecture notes
- tests/: automated tests

## Runtime Shell (Implemented)

- `GET /health`
- `POST /api/v1/files`
- `GET /api/v1/files`

## Local Commands

- `npm run dev`
- `npm test`

## Next Steps

1. Add invalid payload smoke coverage for file write endpoint.
2. Add cloud heartbeat registration integration hook.
3. Extend file listing metadata fields for future indexing.
