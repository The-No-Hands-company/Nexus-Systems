# Nexus Systems API Docs

## Internal stream artifacts

This workspace tracks the canonical v1 contract artifacts for the Nexus Systems API runtime hosted in Nexus-Cloud.

Implemented in this wave:

- `src/contracts.ts`: pinned endpoint map and core DTO shapes
- `src/examples.ts`: stable registration, heartbeat, status, route, and compliance sample payloads
- `src/client.ts`: minimal typed client skeleton for service integration
- `tests/contracts.test.ts`: contract stability checks

## Compatibility notes

- Nexus-Auth consumes registration and heartbeat contracts.
- Nexus-Vault should consume service metadata and tool identity fields.
- Nexus-Guardian and Nexus-Tunnel should consume compliance and route metadata fields.
- Nexus-Edge should consume health and route surfaces for exposure decisions.
