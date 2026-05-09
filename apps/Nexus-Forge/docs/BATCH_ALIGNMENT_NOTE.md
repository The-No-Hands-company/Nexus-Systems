# Nexus-Forge Batch Alignment Note

Generated: 2026-05-08
Scope: Batch interoperability alignment — items 1–8 contract delta review

## Status

Forge is beyond-mvp. This note identifies contract deltas relative to the batch-standard surface
and pins the interoperability test scenario. No runtime changes are required at this stage.

## Existing Contracts Confirmed

### /health
Current shape (main.ts line 17):
```json
{ "status": "ok", "timestamp": "2026-05-08T00:00:00.000Z" }
```
Port: 8090

### Cloud registration
Endpoint: `POST {NEXUS_CLOUD_URL}/api/v1/tools`
Payload:
```json
{
  "id": "nexus-forge",
  "name": "Nexus Forge",
  "description": "...",
  "upstreamUrl": "http://host:8090",
  "mode": "standalone",
  "exposed": true,
  "health": "healthy",
  "capabilities": ["repository-management", "federation-discovery", "pull-requests", "issues", "multi-vcs"]
}
```

### Heartbeat
Endpoint: `POST {NEXUS_CLOUD_URL}/api/v1/tools/nexus-forge/heartbeat`
Payload: `{ "health": "healthy", "upstreamUrl": "..." }`

## Contract Deltas vs Batch Standard

| Surface | Batch standard | Forge current | Delta |
|---------|---------------|--------------|-------|
| /health `service` field | `{ service: "nexus-xxx", ... }` | absent | **delta** — add `service: "nexus-forge"` to health response |
| /health `uptimeSeconds` field | `{ uptimeSeconds: 0 }` | absent (uses `timestamp`) | **delta** — either add `uptimeSeconds` or accept `timestamp` as equivalent |
| Cloud registration path | `POST /api/v1/systems/register` | `POST /api/v1/tools` | **delta** — Nexus-Systems-API must expose a `/api/v1/tools` compat alias or Forge must adapt |
| Heartbeat path | follow-up scoped in batch blockers | `POST /api/v1/tools/{id}/heartbeat` | **delta** — existing pattern is more complete than batch follow-up spec; batch services should adopt same path |
| Port range | 30xx (batch services) | 8090 | **not a delta** — forge port is by design; no change required |

## Required Actions (this batch cycle only)

1. **Health shape** — add `service: "nexus-forge"` to the `/health` response in `src/backend/main.ts`.
   - `uptimeSeconds` is optional this cycle; `timestamp` is an acceptable equivalent.
2. **Cloud registration path** — document the `/api/v1/tools` registration path as the Forge-native contract.
   Nexus-Systems-API hardening (item 10) should add a `/api/v1/tools` alias or mapping table.
3. **Heartbeat** — Forge heartbeat is more complete than the batch follow-up spec. Adopt
   `POST /api/v1/tools/{id}/heartbeat` as the ecosystem standard in item 10 contract pinning.

## Health Delta Fix (applied this cycle)

`src/backend/main.ts` health handler updated to include `service: "nexus-forge"`:
```ts
.get("/health", () => ({ service: "nexus-forge", status: "ok", timestamp: new Date().toISOString() }))
```

## Interoperability Smoke Test (pinned)

Test scenario: **Nexus-API gateway routes to Nexus-Forge health**

```
POST /api/v1/gateway/route
{
  "target": "nexus-forge",
  "method": "GET",
  "path": "/health",
  "payload": {}
}
→ { ok: true, target: "nexus-forge", routed: true, requestId: "..." }
```

This scenario validates the Nexus-API → Nexus-Forge routing path. It is tracked as a follow-up
integration test (not blocking this batch cycle; scoped to Nexus-API + Nexus-Forge integration sprint).

## Evidence

- Alignment note created: 2026-05-08
- Health delta fix applied: src/backend/main.ts
- Existing test suite: tests/api.test.ts, tests/federation.test.ts, tests/vcs.test.ts
