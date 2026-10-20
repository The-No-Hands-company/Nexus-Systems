# Future App Onboarding — Platform Facade Admission Checklist

Every service that exposes routes through `api.tnhc.dev` must pass all nine
steps in order. A partial record is not a release. Each step is a gate, not a
suggestion.

## Admission checklist

1. **Register the service identity and owner in Cloud.**
   The service must have an entry in the Cloud catalog with a stable `id`,
   a declared `owner`, and a service-identity credential scoped to its own
   routes. No shared credentials between services.

2. **Publish the owner contract and explicit route allowlist.**
   Add a `forwardX()` function in `apps/Nexus-API/src/server.ts` that contains
   an explicit allowlist regex. The allowlist must be the narrowest expression
   that covers only the reviewed public surface — never a wildcard passthrough.
   The `forwardX()` function is the service's owner contract: it defines what
   the facade will and will not forward.

3. **Declare required permissions, audience, exposure, health, and readiness.**
   Add a route entry to `api-contract.ts` for every path the allowlist admits.
   Each entry carries `method`, `path`, `access` (`"public"` | `"session"` |
   `"operator"`), `owner` (the service id), and a one-line `description`.
   A route absent from the contract is not part of the public surface.

4. **Obtain authorized review approval for surface and release state.**
   The Cloud catalog record must carry `releaseState: "released"`,
   `maturity: "stable"`, `reviewStatus: "approved"`, and
   `visibility !== "hidden"` before the facade forwards any request. The
   `releasedCatalogBody()` filter in `server.ts` enforces this and must not be
   weakened. The facade's promotion review is separate from the app's own
   release process.

5. **Mark the record stable and released only after the production canary.**
   Flip `releaseState` and `maturity` only after the canary in step 9 is
   clean. Do not pre-mark in staging; the catalog filter runs in production
   against whatever Cloud returns.

6. **Add the same route metadata and OpenAPI operation to Nexus API.**
   For every new contract route, add a matching operation to `openapi.ts`:
   `operationId`, `summary`, `security` (`[{}]` for public, `[{ session: [] }]`
   for session), and at minimum a `200` and `401`/`403` error schema. The
   `contract-parity.test.ts` suite asserts one-to-one parity between contract
   routes and OpenAPI operations; a mismatch is a test failure.

7. **Add owner authorization, facade delegation, CORS, and failure tests.**
   In `public-api.test.ts`: a mock-owner server must verify the facade
   correctly sets `x-nexus-subject` from Auth (not from the browser), and the
   Cloud API key must not appear in any response body. In
   `delegation-boundary.test.ts`: an unreachable upstream must return `503`
   with a machine-readable `error` field, never a gateway timeout HTML page.
   CORS tests must confirm the `app.tnhc.dev` origin receives credentials
   headers and a blocked origin receives no CORS headers.

8. **Add Dashboard navigation only from the released `/api/v1/apps` projection.**
   The Dashboard shell launcher reads the catalog from `/api/v1/apps`. A new
   app appears in the launcher only when its Cloud catalog entry passes the
   `releasedCatalogBody()` filter. Do not add hardcoded navigation links or
   sidebar entries before step 5.

9. **Retire compatibility paths only after supported-client telemetry is clear.**
   The `/ipa/` internal proxy routes stay live until access logs show zero
   traffic from non-test clients. Remove them in a separate commit that
   includes the corresponding Caddy route removal, a note in the commit
   message naming the telemetry window, and a version bump to the contract.

## Contract invariants (enforced by tests)

These invariants are asserted by `contract-parity.test.ts` and must remain
green for every commit that touches `api-contract.ts` or `openapi.ts`:

- Every contract route has a matching OpenAPI operation (`operationId` derived
  from method + path).
- Every OpenAPI operation maps back to exactly one contract route.
- Every route entry has a non-empty `owner` and a non-empty `access` value
  from the allowed set (`"public"` | `"session"` | `"operator"`).
- No contract route carries `access: "public"` with an `owner` other than
  `"nexus-api"` unless the route's description explicitly documents why the
  public surface is safe without session authentication.

## Canary verification

Before removing the gate (step 9), run:

```bash
# API suite — must be green
cd apps/Nexus-API
bun test
bun run check

# Dashboard frontend — must be green
cd apps/Nexus-Dashboard/frontend
env -u DATABASE_URL bun run vitest run

# Smoke: new route reachable through the facade
curl -sI -H "Cookie: nexus_session=<valid>" https://api.tnhc.dev/api/v1/<new-route>
```
