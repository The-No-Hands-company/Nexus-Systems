# Nexus Auth

Status: Wave 2 MVP shell in progress
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus Auth provides ecosystem identity and service authentication primitives for
Nexus apps. In the hosted platform it is a platform service used by the
Dashboard and other clients; it is not an everyday launcher application.

This MVP shell focuses on:

- seed admin and user identities for local development
- issuing service-to-service bearer tokens
- validating issued service tokens
- exposing readiness and health surfaces for orchestration
- publishing a Nexus-Cloud Systems API registration payload shape
- publishing a versioned platform-service contract for clients and operators
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
2. Local development identities are seeded at service startup; there is no public seed route.
3. Issue service tokens with `POST /api/v1/auth/tokens/issue`.
4. Validate service tokens with `POST /api/v1/auth/tokens/validate`.
5. Use `GET /api/v1/auth/contracts/registration` for Cloud registration payload wiring.
6. Set `NEXUS_CLOUD_URL` (default `http://localhost:8787`) to enable automatic registration and heartbeat on startup.

## Client contract

`GET /api/v1/auth/contract` is the public discovery document for the Auth
service. It identifies the hosted surface as `/auth`, lists the session and
Bearer credential routes, and does not grant access by itself. Use the hosted
login flow for browser SSO; use an Auth-issued Bearer credential for CLI or
service clients.
