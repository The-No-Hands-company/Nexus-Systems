# Nexus Auth

Status: Wave 2 MVP shell in progress
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus Auth provides ecosystem identity and service authentication primitives for Nexus apps.

This MVP shell focuses on:

- seed admin and user identities for local development
- issuing service-to-service bearer tokens
- validating issued service tokens
- exposing readiness and health surfaces for orchestration
- publishing a Nexus-Cloud Systems API registration payload shape
- auto-registering with Nexus-Cloud and sending periodic systems heartbeat

## Dual Mode Target

- Standalone: docker compose based local runtime
- Orchestrated: integrated with Nexus-Cloud via Nexus Systems API

## Initial Scaffold Layout

- src/: Bun HTTP runtime and auth contracts
- docs/: architecture note and integration guidance
- tests/: smoke and contract tests

## Next Steps

1. Run `bun install` then `bun run dev` for local startup.
2. Call `POST /api/v1/auth/seed` to initialize seed identities.
3. Issue service tokens with `POST /api/v1/auth/token/issue`.
4. Validate service tokens with `POST /api/v1/auth/token/validate`.
5. Use `GET /api/v1/auth/contracts/systems-api-registration` for Cloud registration payload wiring.
6. Set `NEXUS_CLOUD_URL` (default `http://localhost:8787`) to enable automatic registration and heartbeat on startup.
