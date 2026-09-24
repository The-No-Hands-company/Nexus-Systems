# Nexus Project — Scheduling Core and Views (Increment 1)

**Date:** 2026-09-25
**Status:** Approved in brainstorming; awaiting spec review

## Goal

Give the ecosystem one project management system that serves a solo project
manager and a team with the same code. Kanban-style issue tracking and
plan-driven scheduling are equal citizens: every task can sit on a board and
on a Gantt timeline, and the schedule is computed correctly (true critical
path, float, four link types, working-day calendars), not approximated.

A solo user signs in and has a personal workspace with no setup. A team shares
a team workspace with roles. The difference is membership, not a separate
product.

## Scope

Increment 1 delivers: workspaces with membership and roles, projects, an
arbitrarily nested work breakdown structure (WBS), per-project statuses,
dependencies of all four types with lag, cycle detection, working-day
calendars, critical path and float, manual and auto scheduling, the Board,
List and Timeline views, trusted SSO identity, replacement of the existing
scaffold, and delivery at `project.tnhc.dev` and `/project` in the shell.

Out of scope, planned for increment 2: comments, activity log, notifications,
live updates (teams see each other's changes on refresh; optimistic
concurrency prevents silent overwrites), baselines, links into Chat and
Calendar. Later: CLI (blocked on non-browser credentials in Auth), resource
levelling, hour-granularity scheduling, cross-project dependencies, AI
insights, federation.

## The existing scaffold

`apps/Nexus-Project` (untracked until this work) does not run: 0 of 2 tests
pass, the server references an undefined `PhantomApp`, the list route the test
calls does not exist, task filters are never bound to the query,
`criticalPathAnalysis` ignores dependencies, federation sync assigns new IDs
and duplicates data, and `project-engine.js` is a diverging copy of the TS
engine. It is replaced, keeping only the service name, port 3152 and the
Cloud registration mechanism.

## Architecture

- **Backend:** Bun + strict TypeScript, `bun:sqlite`, port 3152, bound to
  `127.0.0.1`. The service is the authority for all project data.
- **Scheduling engine:** `src/schedule/`, a pure module (no database, clock or
  I/O). The server runs it on every schedule-affecting write and persists the
  result; the frontend bundles the same module for instant drag previews. The
  server's response is always authoritative.
- **Frontend:** React 18, Vite, Tailwind 4 in `frontend/`, using
  `@nexus/design` and `@nexus/app-shell`. One build artifact served on both
  origins.
- **Delivery:** `proxied-app`, the Calendar pattern. Dashboard proxies
  `/project` (web) and `/ipa/project/*` (API) without an iframe.

## Data Model

SQLite with `PRAGMA foreign_keys = ON`; schema version in `PRAGMA
user_version`, each migration in one immediate transaction. IDs are UUIDs;
tasks also carry a per-project `number` rendered as `KEY-n` (e.g. `WEB-12`).
Every row records `created_at`, `updated_at`, `created_by`, `updated_by`.

| Table | Columns (beyond audit fields) |
|---|---|
| `workspaces` | `id`, `name`, `kind` (`personal`/`team`) |
| `workspace_members` | `workspace_id`, `subject`, `role` (`owner`/`admin`/`member`/`viewer`); unique per pair |
| `projects` | `id`, `workspace_id`, `key` (unique per workspace, `[A-Z][A-Z0-9]{1,9}`), `name`, `description`, `start_date`, `schedule_mode` (`manual`/`auto`), `visibility` (`workspace`/`restricted`), `archived`, `next_number` |
| `project_members` | `project_id`, `subject` — grants visibility of a restricted project |
| `statuses` | `id`, `project_id`, `name`, `category` (`backlog`/`unstarted`/`started`/`completed`/`canceled`), `position` |
| `tasks` | `id`, `project_id`, `number`, `parent_id`, `position`, `title`, `description`, `status_id`, `priority` (`none`/`low`/`medium`/`high`/`urgent`), `assignee_subject`, `kind` (`task`/`milestone`), `duration_days`, `start_date`, `finish_date`, `constraint_type` (`asap`/`start_no_earlier_than`), `constraint_date`, `deadline`, `progress` (0–100), `version` |
| `dependencies` | `id`, `project_id`, `predecessor_id`, `successor_id`, `type` (`FS`/`SS`/`FF`/`SF`), `lag_days` (negative = lead); unique per pair |
| `calendar_days` | `project_id`, `working_weekdays` (bitmask) plus dated exceptions `(date, working)` |

A personal workspace is created on a subject's first request and has exactly
one member. New projects are seeded with five statuses, one per category.

### Model rules

- A task with children is a **summary task**. Its dates span its children,
  its progress is the duration-weighted average of its children, and its own
  duration, dates and progress cannot be set.
- A **milestone** has duration 0 and no children.
- **Dependencies** connect leaf tasks and milestones within one project. A
  link touching a summary task, crossing projects, or linking a task to itself
  is rejected.
- `parent_id` must stay in the same project and must not create a cycle in the
  WBS.
- Deleting a task deletes its subtree and its links, in one transaction.
- Deleting a status requires `moveTasksTo`, a status in the same project.
- **Granularity is whole working days.**
- In auto mode the engine writes `start_date`/`finish_date` for leaf tasks.
  Float and the critical path are never stored; they are computed on read.
- Every task write increments `version`.

## Scheduling Engine

`schedule(input) → result`, with input = the project's tasks, links, calendar,
start date and mode.

### Time model

Dates are integer day numbers (days since 1970-01-01, UTC). The calendar maps
dates to working-day indices (working weekdays, minus non-working exceptions,
plus working exceptions). A task occupies the half-open working-day interval
`[ES, EF)`, `EF = ES + duration`; its displayed finish is the last working day
of the interval. Milestones have `ES = EF`.

### Validation (on write, before persisting)

`dependency_cycle` (with the cycle as task keys, e.g. `WEB-3 → WEB-7 → WEB-3`),
`summary_link`, `cross_project_link`, `self_link`, `wbs_cycle`. All return
`422`.

### Forward pass

Topological order over leaf tasks and milestones. `ES` is the maximum of the
project start, the task's `start_no_earlier_than` date, and for each incoming
link:

| Type | Constraint on the successor's ES |
|---|---|
| FS | `pred.EF + lag` |
| SS | `pred.ES + lag` |
| FF | `pred.EF + lag − duration` |
| SF | `pred.ES + lag − duration` |

In manual mode a task's stored start also acts as a start-no-earlier-than, so
the user's dates are never pulled earlier.

### Backward pass

Project finish is the maximum `EF`. `LF` for a task with no successors is the
project finish; otherwise the minimum over successors of the mirror of the
table above. A task's `deadline` caps its `LF`. Outputs: `LS`, `LF`,
**total float** `LS − ES`, **free float**, and **critical** when total float
≤ 0. A task that cannot meet its deadline has negative float.

### Modes

- **Auto:** leaf `start_date`/`finish_date` are set from `ES`/`EF` in one
  transaction; only changed rows are written and versioned.
- **Manual (default):** stored dates are never moved. Wherever the logic
  requires a later start than stored, the engine reports a **violation**
  naming both tasks, the link type and the gap in working days.

### Unestimated tasks

A leaf with no duration and no dates is unscheduled: it appears on the board
and list but not in the schedule. A link touching it is ignored by the
engine and reported as a warning.

### Roll-up

Summary start = earliest child start, finish = latest child finish. Progress
= Σ(duration × progress) / Σ duration over children; unestimated children
count 0 or 100 by status category with weight 1.

### Determinism and performance

Ties break by `(ES, number)`. Each pass is O(tasks + links). A benchmark test
requires a 5,000-task, 10,000-link project to schedule in under 50 ms.

## API

All routes under `/api/v1/project`, JSON, authenticated. `/health` and
`/api/v1/status` are public and read no project data.

| Area | Routes |
|---|---|
| Workspaces | `GET /workspaces` (ensures the caller's personal workspace), `POST /workspaces`, `PATCH /workspaces/:id`, `PUT /workspaces/:id/members/:subject`, `DELETE /workspaces/:id/members/:subject` |
| Projects | `GET/POST /workspaces/:id/projects`, `GET/PATCH /projects/:id`, `POST /projects/:id/move` |
| Statuses | `GET/POST /projects/:id/statuses`, `PATCH /statuses/:id`, `DELETE /statuses/:id?moveTasksTo=` |
| Tasks | `GET/POST /projects/:id/tasks` (filters `status`, `assignee`, `parent`), `GET/PATCH/DELETE /tasks/:id`, `POST /tasks/:id/move` (`parentId`, `position`, `statusId`) |
| Links | `POST /projects/:id/dependencies`, `PATCH/DELETE /dependencies/:id` |
| Calendar | `GET/PUT /projects/:id/calendar` |
| Schedule | `GET /projects/:id/schedule` — per-task ES/EF/LS/LF, total and free float, critical flag, summary roll-ups, violations, warnings |

`GET /me/tasks` returns tasks assigned to the caller across every visible
project (the solo home view).

### Identity

Same contract as Calendar: an `x-nexus-identity` token verified by
`@nexus/identity` with audience `project.tnhc.dev` (`NEXUS_PROJECT_JWT_AUDIENCE`),
or `x-nexus-subject` accompanied by `NEXUS_PROJECT_DASHBOARD_SECRET`, compared
in constant time. Anything else is anonymous and receives `401`.

### Authorization

| Role | Permissions |
|---|---|
| viewer | read |
| member | + create, edit, move and delete tasks; create and delete links |
| admin | + project settings, statuses, calendar, schedule mode, restricted-project membership, workspace members below owner |
| owner | + delete or transfer the workspace |

A restricted project is visible to workspace owners and admins and to its
`project_members`. Anything the caller cannot see returns `404`; anything they
can see but not change returns `403`. A personal workspace cannot gain
members; collaboration happens by moving a project into a team workspace,
which requires admin in both.

### Validation and errors

- Envelope: `{ "error": "<code>", "message": "<human text>" }`.
- Unknown body fields → `400`. Title ≤ 512 characters, description ≤ 50,000,
  names ≤ 200.
- `PATCH` on tasks requires `If-Match: <version>`; a mismatch returns `409
  version_conflict` with the current task.
- Schedule rule violations → `422` with the codes listed above.
- A `5xx` never carries internal detail.
- Responses send `cache-control: no-store` and `referrer-policy: no-referrer`.

## Frontend

Views: **Home** (workspace switcher, projects, My tasks), **Board** (status
columns, drag between and within, summaries shown or hidden, filters),
**List** (WBS tree table, inline editing, MS Project predecessor notation such
as `3FS+2d`, Tab/Shift+Tab indent), **Timeline** (Gantt: tree plus bars,
day/week/month zoom, drag to move, drag edge to resize, drag bar-to-bar for an
FS link, link arrows, critical path, non-working days, violations, deadlines,
today line), and a **Task drawer** with every field.

- Timeline renders SVG with row virtualization.
- During a drag the bundled engine recomputes locally; the server response on
  drop is authoritative.
- Mutations are optimistic and roll back on failure; `409` opens a conflict
  dialog showing both versions; `422` explains the rule.
- Loading, empty, error and content are visually distinct in every view; an
  API failure never renders as empty.
- Every drag interaction has a keyboard or menu alternative.
- Below 640 px: Board and List; Timeline scrolls horizontally.

## Delivery

- A Caddy front door serves `frontend/dist` on a loopback port chosen during
  planning.
- Dashboard's `calendar-proxy.ts` is generalized into a proxied-app factory
  (route allow-list, identity attachment, header hygiene, 5xx collapse);
  Calendar and Project both use it, and Calendar's existing proxy tests pass
  unchanged.
- The Dashboard registry gains `id=nexus-project`, `path=/project`,
  `publicUrl=https://project.tnhc.dev`, `delivery=proxied-app`.
- `deploy.sh` starts `project` and `nexus-project-web` and registers both in
  `service_identity`/`service_port`.
- The SSO gate needs no change: Project has no public routes.
- The `project.tnhc.dev` DNS record must be verified. The Cloudflare token is
  known to have expired; if the record cannot be published automatically, the
  manual step is documented and `/project` inside the shell remains the
  working route.

## Testing

- **Engine:** property tests over generated graphs — every link satisfied, no
  task before its constraint, total float ≥ 0 without deadlines, the critical
  path is a connected zero-float chain ending at the finish task, cycle
  detection rejects exactly the cyclic graphs, calendar arithmetic never lands
  on a non-working day and round-trips; the 5,000-task benchmark.
- **API:** per-route tests for the authorization table (including 404 vs 403
  and restricted projects), `If-Match` conflicts, every `422` code, strict
  validation, and migration from an empty database.
- **Frontend:** vitest + testing-library for each view's four states, the drag
  and keyboard paths, and conflict handling.
- **Dashboard:** the generalized proxy keeps Calendar's tests green and adds
  the same set for Project.
- **Deploy:** the start-contract test covers the two new services.
- **Acceptance:** a runtime check against the real API through Dashboard's
  proxy.

Every suite's file count is asserted alongside its test count, so a file that
fails to import cannot hide behind a green total.

## Open verification during planning

- Whether Nexus Auth exposes a username-to-subject lookup Project may call.
  If it does, the member picker uses it. If it does not, the plan either adds
  the smallest such endpoint to Auth (exact username → subject, authenticated
  callers only, no listing) or adds members by subject ID, and states which.
- The free loopback port for the web front door.
