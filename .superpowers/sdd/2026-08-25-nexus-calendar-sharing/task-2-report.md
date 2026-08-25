# Task 2 — Owned Event Schema and Fail-Closed Migration

## Implementation

- Added `ownerSubject` and `access: "owner" | "editor" | "viewer"` to `CalEvent`, plus explicit `EventCreate` and `EventRange` interfaces.
- Added `CalendarEngine(path, { legacyOwnerSubject? })`, `createEvent(ownerSubject, input)`, owner-filtered `listEvents(callerSubject, range)`, and `close()`.
- Added a `PRAGMA user_version = 1` SQLite migration under one `BEGIN IMMEDIATE` transaction. New databases receive the owned table; legacy tables are rebuilt with `owner_subject TEXT NOT NULL` and the `(owner_subject, start_time, end_time)` index.
- The migration throws the exact `legacy_owner_required` error before modifying a populated legacy database with no explicit owner. It backfills all legacy rows only when the supplied subject is nonempty; empty legacy databases migrate without an owner.
- Preserved pre-Task-3 HTTP CRUD using the explicit `legacy-local-subject`; server shutdown now stops heartbeat, closes the engine, then stops Bun.
- Documented database and legacy-owner configuration in the Calendar README.

## TDD evidence

RED, before implementation:

```text
cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts
0 pass, 4 fail
- expected legacy_owner_required, constructor did not throw
- new listEvents signature caused SQLite binding TypeError
- new createEvent signature caused NOT NULL title failure
- close() was not defined
```

GREEN after implementation:

```text
cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts tests/server.test.ts
10 pass, 0 fail, 26 expectations

cd apps/Nexus-Calendar && bun test
11 pass, 0 fail, 28 expectations
```

`git diff --check` completed without whitespace errors.

## Fix round 1 — nullable partial ownership schema

The migration now treats `owner_subject` as current only when SQLite reports both `TEXT` affinity and `NOT NULL`. Any other shape is rebuilt transactionally. For a partial ownership schema, existing non-null owners are preserved and only `NULL` owners require the explicit `legacyOwnerSubject`; without it, startup fails with `legacy_owner_required` before any change.

RED:

```text
cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts
4 pass, 1 fail
- nullable owner_subject schema containing an unowned row: expected legacy_owner_required; constructor did not throw
```

GREEN:

```text
cd apps/Nexus-Calendar && bun test tests/calendar-engine.test.ts tests/server.test.ts
11 pass, 0 fail, 29 expectations

cd apps/Nexus-Calendar && bun test
12 pass, 0 fail, 31 expectations
```

## Files

- `apps/Nexus-Calendar/src/calendar-engine.ts`
- `apps/Nexus-Calendar/src/server.ts`
- `apps/Nexus-Calendar/tests/calendar-engine.test.ts`
- `apps/Nexus-Calendar/tests/server.test.ts`
- `apps/Nexus-Calendar/README.md`

## Commit

`feat(calendar): migrate events to private ownership`

## Self-review

An independent read-only review reported no findings. Manual review confirmed migration rollback on failure, no implicit owner for populated legacy rows, owner/time query filtering, migration index creation, and the required shutdown ordering.

## Concerns

- `bun run check` is blocked because this isolated worktree has no installed `bun-types`; `bun run lint` is blocked because Bun cannot download the declared dependency in this environment. Attempts with workspace-local temporary directories and a no-save dependency install did not create `node_modules`.
- The explicit server subject is a temporary compatibility bridge only; Task 3 must replace it with authenticated caller identity before exposing user-owned events.
