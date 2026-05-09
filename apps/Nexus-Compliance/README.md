# Nexus Compliance

Status: MVP shell implemented
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus Compliance provides the first compliance event and status surface for ecosystem services.

This MVP gate focuses on:

- a minimal compliance service contract (event ingest + report status)
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
- `GET /api/v1/compliance/status`
- `POST /api/v1/compliance/events`

## Local Commands

- `npm run dev`
- `npm test`

## Next Steps

1. Add invalid payload smoke coverage for compliance events endpoint.
2. Add cloud heartbeat registration integration hook.
3. Extend status endpoint with policy summary counters once policy engine lands.
