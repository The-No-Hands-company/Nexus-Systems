# Nexus Monitor

Status: MVP shell implemented
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus Monitor provides the first observability surface for Nexus ecosystem services.

This MVP gate focuses on:

- a minimal monitor service contract (ingest + status)
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
- `GET /api/v1/monitor/status`
- `POST /api/v1/monitor/ingest`

## Local Commands

- `npm run dev`
- `npm test`

## Next Steps

1. Add registration heartbeat bridge to Nexus-Cloud endpoint once cloud URL is configured.
2. Extend ingest validation with event schema versions.
3. Add one failure-path smoke test for invalid ingest payload.
