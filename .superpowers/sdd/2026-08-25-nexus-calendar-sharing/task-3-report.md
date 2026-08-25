# Task 3 — Trusted Identity, Validation, and Owner CRUD

## Implementation

- Added `resolveCaller(req)`: Calendar accepts only a verified `x-nexus-identity` RS256 JWT for audience `calendar.tnhc.dev` (or `NEXUS_CALENDAR_JWT_AUDIENCE`) or a Dashboard-only `x-nexus-subject` accompanied by the constant-time checked `x-nexus-dashboard-secret` value configured in `NEXUS_CALENDAR_DASHBOARD_SECRET`.
- Added five-minute bounded JWKS caching against Auth's `/api/v1/auth/oauth/jwks`; wrong audience, expiry, algorithm, key, signature, malformed payload, and missing subject fail closed.
- Removed the `legacy-local-subject` HTTP compatibility bridge entirely. Calendar event routes now return `401` without a trusted caller.
- Added allow-listed create/patch/range validation: title and text bounds, ISO-parsable times, strictly increasing intervals, bounded ranges, non-empty patches, and UUID event IDs. Invalid input returns `400`.
- Changed event GET/PATCH/DELETE signatures to take `callerSubject` and enforce ownership directly in each SQL operation. Listing uses overlap semantics: `start_time < requested_end AND end_time > requested_start`.
- Added route tests covering browser-header rejection, Dashboard hop acceptance, JWT failure cases, validation, and cross-owner list/GET/PATCH/DELETE denial.

## TDD evidence

RED before production implementation:

```text
cd apps/Nexus-Calendar && bun test tests/auth.test.ts tests/validation.test.ts tests/server.test.ts
6 pass, 8 fail, 2 errors
- auth.ts and validation.ts were absent
- unauthenticated events list returned 200 instead of 401
- foreign list/GET/PATCH/DELETE leaked or modified the owner's event
```

An additional invalid-percent-encoded ID test was written after review:

```text
cd apps/Nexus-Calendar && bun test tests/server.test.ts --test-name-pattern 'rejects invalid IDs'
0 pass, 1 fail
URIError at decodeURIComponent for /events/%ZZ
```

GREEN after minimal implementation:

```text
cd apps/Nexus-Calendar && bun test tests/server.test.ts --test-name-pattern 'rejects invalid IDs'
1 pass, 0 fail

cd apps/Nexus-Calendar && bun test tests/auth.test.ts tests/validation.test.ts tests/server.test.ts
25 pass, 0 fail, 46 expectations

cd apps/Nexus-Calendar && bun test
31 pass, 0 fail, 63 expectations
```

`git diff --check` passed with no whitespace errors.

## Files

- `apps/Nexus-Calendar/src/auth.ts`
- `apps/Nexus-Calendar/src/validation.ts`
- `apps/Nexus-Calendar/src/calendar-engine.ts`
- `apps/Nexus-Calendar/src/server.ts`
- `apps/Nexus-Calendar/tests/auth.test.ts`
- `apps/Nexus-Calendar/tests/validation.test.ts`
- `apps/Nexus-Calendar/tests/server.test.ts`
- `apps/Nexus-Calendar/tests/calendar-engine.test.ts`

## Commit

`feat(calendar): enforce owner-scoped event access`

## Self-review

- Confirmed the server never trusts a bare browser `x-nexus-subject` and the legacy local subject is removed.
- Confirmed inaccessible IDs consistently produce `404`, including PATCH and DELETE, while ownership predicates live in SQL rather than route-only checks.
- Confirmed malformed encoded IDs produce `400` instead of throwing.
- Confirmed POST derives ownership only from `resolveCaller`; no client-provided ownership field is accepted.

## Concerns

- `bun run check` cannot complete in this isolated worktree: initially `bun-types` was absent. The available cached `bun-types` 1.3.14 is incompatible with this Bun 1.3.12 project's type declarations and reports pre-existing `Request`/`Response` errors in `cloud.ts` and `server.ts`; no source or dependency manifest was changed to mask that environment issue.

## Fix round 1 — Canonical timestamps for overlap queries

SQLite compares the `TEXT` timestamp columns lexically. The original validation accepted any `Date.parse`-able string and retained its source spelling, so `2026-09-01T11:00:00-05:00` incorrectly sorted before a `2026-09-01T14:30:00.000Z` range boundary despite being 16:00 UTC.

RED:

```text
cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts tests/validation.test.ts
10 pass, 3 fail
- offset event overlapping 14:30Z–15:30Z returned zero events
- 09/01/2026 was accepted
- accepted offset timestamps were not normalized
```

GREEN:

```text
cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts tests/validation.test.ts tests/server.test.ts
25 pass, 0 fail, 59 expectations

cd apps/Nexus-Calendar && bun test
33 pass, 0 fail, 68 expectations
```

Validation now accepts only ISO-shaped timestamps and returns canonical `toISOString()` UTC values. The engine also canonicalizes create/update timestamps before SQLite persistence, so its lexical overlap SQL remains correct for internal callers as well as HTTP requests. Identity and ownership behavior are unchanged.
