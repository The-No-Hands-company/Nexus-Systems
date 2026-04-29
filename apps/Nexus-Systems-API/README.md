# Nexus Systems API

Status: Cloud-internal stream (hosted by Nexus-Cloud)
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Maintain the canonical integration contract, examples, and SDK artifacts for the Nexus Systems API runtime hosted inside `Nexus-Cloud`.

## Dual Mode Target

- Standalone: publishable contract/spec/SDK package for local integration testing
- Orchestrated: runtime implementation in `Nexus-Cloud/src/systems-api`

## Initial Scaffold Layout

- src/: SDK/client helpers and contract utilities
- docs/: API spec, examples, and versioning notes
- tests/: contract and compatibility tests

## Next Steps

1. Pin v1 request/response DTOs and endpoint contract tables.
2. Add example payloads for registration, heartbeat, status, route, and compliance flows.
3. Create minimal SDK/client skeleton used by P0 services.
4. Add contract tests that validate compatibility with `Nexus-Cloud` Systems API endpoints.
