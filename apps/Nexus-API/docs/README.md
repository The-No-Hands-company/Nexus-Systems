# Nexus API Docs

## Stable platform facade

Nexus-API is the hosted platform facade at `api.tnhc.dev`. It owns versioning,
discovery, common errors, and compatibility routing while Auth, Cloud, and each
application keep ownership of identity, control-plane state, and app data.

- [API boundary and ownership](./API_BOUNDARY_V1.md)
- [Route inventory](./ROUTE_INVENTORY.md)
- [Legacy route inventory OpenAPI](./legacy-openapi.yaml)
- Runtime OpenAPI: `GET /api/v1/openapi.json`
- Runtime contract: `GET /api/v1/contract`

## Stable endpoint surface (v1)

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness probe |
| GET | /api/v1/me | Authenticated platform identity |
| GET | /api/v1/apps | Approved stable released app catalog |
| GET | /api/v1/catalog | Compatibility alias for the app catalog |
| GET/POST | /api/v1/mail/* | Auth-scoped Nexus Email facade |
| GET | /api/v1/gateway/status | Gateway counters |

The `/api/v1/gateway/*` routes are operator-protected compatibility routes. New
apps join through Cloud approval and the release metadata described in the
boundary document; arbitrary dynamic proxy routes are not part of the user API.
