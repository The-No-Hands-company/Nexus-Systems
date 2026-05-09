# Nexus AI Hub

Status: MVP shell implemented
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus AI Hub provides the first AI orchestration and provider routing surface for ecosystem services.

This MVP gate focuses on:

- a minimal AI-Hub service contract (route + provider status)
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
- `GET /api/v1/hub/providers/status`
- `POST /api/v1/hub/route`

## Local Commands

- `npm run dev`
- `npm test`

## Next Steps

1. Add invalid payload smoke coverage for route endpoint.
2. Add cloud heartbeat registration integration hook.
3. Extend providers status endpoint with provider health metadata.
