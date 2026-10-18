# Nexus Auth Docs

## MVP architecture note

Nexus Auth runs as a small Bun HTTP service with in-memory state for local wave execution.

Current modules:

- `src/state.ts`: seed identities and in-memory identity catalog
- `src/token.ts`: service token issue and validate helpers
- `src/contracts.ts`: Nexus-Cloud registration payload contract helper
- `src/server.ts`: HTTP routes and JSON API behavior

## API contract

- `GET /health`
- `GET /api/v1/auth/readiness`
- `GET /api/v1/auth/contract`
- `POST /api/v1/auth/tokens/issue`
- `POST /api/v1/auth/tokens/validate`
- `GET /api/v1/auth/contracts/registration`
- `GET|POST /api/v1/auth/api-keys`
- `POST /api/v1/auth/api-keys/:id/revoke`

## Integration guidance

- Seed identities once during local bootstrap.
- Use issued bearer tokens for service-to-service auth stubs.
- Consume the systems registration contract endpoint to register with Nexus-Cloud consistently.

## Known limitations

- Persistence is in-memory only.
- The hosted session and API-key stores are still in-memory in this MVP.
- API-key scope enforcement remains an Auth policy task; current service
  permissions derive from the owning account role.
