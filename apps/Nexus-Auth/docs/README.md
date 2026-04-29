# Nexus Auth Docs

## MVP architecture note

Nexus Auth runs as a small Bun HTTP service with in-memory state for local wave execution.

Current modules:

- `src/state.ts`: seed identities and in-memory identity catalog
- `src/token.ts`: service token issue and validate helpers
- `src/contracts.ts`: Nexus-Cloud registration payload contract helper
- `src/server.ts`: HTTP routes and JSON API behavior

## MVP API contract

- `GET /health`
- `GET /api/v1/auth/readiness`
- `POST /api/v1/auth/seed`
- `POST /api/v1/auth/token/issue`
- `POST /api/v1/auth/token/validate`
- `GET /api/v1/auth/contracts/systems-api-registration`

## Integration guidance

- Seed identities once during local bootstrap.
- Use issued bearer tokens for service-to-service auth stubs.
- Consume the systems registration contract endpoint to register with Nexus-Cloud consistently.

## Known limitations

- Persistence is in-memory only.
- No password login flow in this MVP.
- Token revocation list is not implemented yet.
