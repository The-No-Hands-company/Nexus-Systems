# Task 4 — Internal Viewer and Editor Sharing

## Implementation

- Added `EventPermission` (`viewer` or `editor`) and `EventShare` contracts.
- Added idempotent `event_shares` persistence with `(event_id, grantee_subject)` primary key, event foreign-key cascade, grantee lookup index, and SQLite foreign-key enforcement.
- Added transactional owner-scoped `listShares`, `upsertShare`, and `deleteShare` engine methods. Each mutation repeats its ownership predicate in the engine; route prechecks are not trusted as authorization.
- Expanded event access resolution in SQL: owners, editors, and viewers can read; only editors and owners can patch; only owners can delete. Lists use the same join and return each event once.
- Added owner-only share routes: `GET /events/:eventId/shares`, `PUT /events/:eventId/shares/:subject`, and `DELETE /events/:eventId/shares/:subject`.
- Added strict permission-body validation, owner self-share rejection, `401` for unauthenticated share routes, and `404` for inaccessible event IDs. No public-link capability was added.

## TDD evidence

RED before production implementation:

```text
cd apps/Nexus-Calendar && bun test tests/sharing.test.ts
0 pass, 5 fail
- share routes returned 404 instead of required 401/200/400 behavior
- viewers and editors could not read grants because persistence/access joins were absent
- shared events were missing from recipient listings
```

GREEN after minimal implementation:

```text
cd apps/Nexus-Calendar && bun test tests/sharing.test.ts
5 pass, 0 fail, 34 expectations

cd apps/Nexus-Calendar && bun test tests/sharing.test.ts tests/server.test.ts
17 pass, 0 fail, 63 expectations

cd apps/Nexus-Calendar && bun test
39 pass, 0 fail, 106 expectations
```

## Files

- `apps/Nexus-Calendar/src/calendar-engine.ts`
- `apps/Nexus-Calendar/src/validation.ts`
- `apps/Nexus-Calendar/src/server.ts`
- `apps/Nexus-Calendar/tests/sharing.test.ts`
- `.superpowers/sdd/2026-08-25-nexus-calendar-sharing/task-4-report.md`

## Commit

`feat(calendar): add explicit user event sharing`

## Self-review

- Confirmed the owner/editor/viewer/stranger matrix and duplicate-free shared listings are exercised through real HTTP behavior.
- Confirmed share listing and grant mutation return `404` to viewers, editors, and strangers; the engine repeats owner authorization for every share method.
- Confirmed editor patch authority is guarded both before merging data and in the SQL `UPDATE` predicate, while deletion remains owner-only.
- Independent read-only review found no critical, important, or minor findings; focused 24-test suite, app check, and whitespace check were clean.

## Concerns

- `bun run check` remains blocked by existing Bun DOM type declaration conflicts in `auth.ts`, `cloud.ts`, `server.ts`, and HTTP tests (`Request`/`Response` members missing). The Task 4 implementation did not change dependencies or mask this baseline issue.
