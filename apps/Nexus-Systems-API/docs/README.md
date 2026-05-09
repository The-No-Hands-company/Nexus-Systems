# Nexus Systems API Docs

## Internal stream artifacts

This workspace tracks the canonical v1 contract artifacts for the Nexus Systems API runtime hosted in Nexus-Cloud.

Implemented and pinned 2026-05-08:

- `src/contracts.ts`: pinned endpoint map, core DTO shapes, batch service registry, compatibility notes
- `src/examples.ts`: stable registration, heartbeat, status, route, compliance, and batch service payloads
- `src/client.ts`: SDK client skeleton (registerTool, heartbeat, getStatus, getComplianceSummary)
- `tests/contracts.test.ts`: 4 contract tests — all passing

## V1 Endpoint Contract Table

| Endpoint | Method | Path | Purpose |
|----------|--------|------|---------|
| registerTool | POST | /api/v1/tools | Register a service with the Systems API |
| heartbeat | POST | /api/v1/tools/:toolId/heartbeat | Send a liveness heartbeat |
| status | GET | /api/v1/status | Systems API aggregate status |
| routes | GET | /api/v1/routes | Registered service route table |
| phantomCompliance | GET | /api/v1/compliance/phantom | Phantom compliance full report |
| phantomComplianceSummary | GET | /api/v1/compliance/phantom/summary | Phantom compliance summary |

## Batch Service Registry (pinned 2026-05-08)

| Service | Port | Role |
|---------|------|------|
| nexus-monitor | 3030 | monitoring |
| nexus-compliance | 3031 | compliance |
| nexus-ai-hub | 3032 | ai-hub |
| nexus-files | 3033 | file-storage |
| nexus-search | 3034 | search |
| nexus-ide | 3035 | ide |
| nexus-api | 3036 | gateway |
| nexus-testing | 3037 | test-runner |
| nexus-forge | 8090 | code-forge |

## Compatibility Notes

### Registration path
- Canonical: `POST /api/v1/tools` (all batch services, Forge)
- Batch service MVP contract drafts reference `POST /api/v1/systems/register` as a placeholder;
  this should be treated as an alias. Resolution in next hardening cycle.

### Heartbeat path
- Canonical: `POST /api/v1/tools/:toolId/heartbeat`
- This matches Forge's existing pattern. Batch service follow-ups should adopt this path.

### Health response shape delta
- Batch services (items 1–8): `{ service, status, uptimeSeconds }`
- Nexus-Forge: `{ service, status, timestamp }` — `service` field added 2026-05-08; `timestamp` is acceptable equivalent for `uptimeSeconds` this cycle.

### Registration payload shape delta
- SDK canonical: `{ id, displayName, description, mode, upstreamUrl, capabilityTags }`
- Nexus-Forge actual: `{ id, name, description, mode, upstreamUrl, exposed, health, capabilities }`
- Delta tracked in `Nexus-Forge/docs/BATCH_ALIGNMENT_NOTE.md`. Resolution deferred to Forge alignment sprint.

- `src/client.ts`: minimal typed client skeleton for service integration
- `tests/contracts.test.ts`: contract stability checks

## Compatibility notes

- Nexus-Auth consumes registration and heartbeat contracts.
- Nexus-Vault should consume service metadata and tool identity fields.
- Nexus-Guardian and Nexus-Tunnel should consume compliance and route metadata fields.
- Nexus-Edge should consume health and route surfaces for exposure decisions.
