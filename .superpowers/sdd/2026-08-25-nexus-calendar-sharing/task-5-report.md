# Task 5 — Revocable Public Read-Only Links

## Implementation

- Added a `public_event_shares` SQLite table with one active row per event, a unique SHA-256 `token_digest`, and foreign-key cascade deletion. Bearer tokens are 32 random bytes encoded as 43-character base64url values; the raw token is returned only by public-link creation.
- Added transactional, owner-scoped `createPublicShare`, `revokePublicShare`, and digest-based `getPublicEvent` engine methods. Replacing a link updates the single active digest, so the old token becomes unreachable.
- Added authenticated owner-only `POST` and `DELETE /api/v1/calendar/events/:eventId/public-share` endpoints and anonymous `GET /api/v1/calendar/public/:token`.
- Public reads return only `title`, `startTime`, `endTime`, `allDay`, `location`, and `description`; malformed, unknown, and revoked tokens return `404`; non-GET requests to a token resource return `405`.
- Added a structural production-gate exception only for `GET https://calendar.tnhc.dev/api/v1/calendar/public/<43-character-base64url-token>` with no query string. All other Calendar APIs, methods, hosts, missing tokens, encoded slash paths, and query variants remain gated. Calendar was not added to either general public allowlist.
- Tightened the pre-existing SPA fallback so `/` is not anonymously admitted; static assets and `index.html` remain public.

## TDD evidence

RED before production implementation:

```text
cd apps/Nexus-Calendar && bun test tests/public-sharing.test.ts
0 pass, 4 fail
- POST /events/:eventId/public-share returned 404 where the creation contract requires 201.

cd deploy/production && bun test tests/gate.test.ts
3 pass, 3 fail
- the exact Calendar public GET was denied;
- the pre-existing app/cloud root gate tests also exposed the generic SPA fallback admitting / without a session.
```

GREEN after minimal implementation:

```text
cd apps/Nexus-Calendar && bun test tests/public-sharing.test.ts tests/calendar-engine.test.ts tests/server.test.ts tests/sharing.test.ts
28 pass, 0 fail, 109 expectations

cd deploy/production && bun test tests/gate.test.ts
6 pass, 0 fail, 10 expectations

cd apps/Nexus-Calendar && bun run lint
$ bunx biome check src tests
```

The Calendar HTTP tests were run outside the command sandbox because sandboxed Bun binds consistently fail with `EADDRINUSE`; the same commands passed unrestricted.

## Files

- `apps/Nexus-Calendar/src/calendar-engine.ts`
- `apps/Nexus-Calendar/src/server.ts`
- `apps/Nexus-Calendar/tests/public-sharing.test.ts`
- `deploy/production/gate.ts`
- `deploy/production/tests/gate.test.ts`
- `.superpowers/sdd/2026-08-25-nexus-calendar-sharing/task-5-report.md`

## Commit

`feat(calendar): add revocable public event links`

## Concerns

- `bun run check` remains blocked by the known Bun DOM type declaration conflict: existing Calendar `Request`/`Response` usages are reported as missing DOM members. The new HTTP test inherits the same project-level typing limitation; runtime tests and Biome lint pass.
