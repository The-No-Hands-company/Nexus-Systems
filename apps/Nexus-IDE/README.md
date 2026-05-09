# Nexus IDE

Status: MVP shell implemented
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus IDE provides the first workspace session and remote tooling bridge surface for ecosystem services.

This MVP gate focuses on:

- a minimal IDE service contract (session + command bridge)
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
- `POST /api/v1/ide/session`
- `GET /api/v1/ide/status`

## Local Commands

- `npm run dev`
- `npm test`

## Next Steps

1. Add invalid payload smoke coverage for IDE session endpoint.
2. Add cloud heartbeat registration integration hook.
3. Extend status endpoint with active session metadata.
