# Nexus-Project

Project management for one person or a team, with the same code: boards and
lists for day-to-day work, and a real critical-path schedule for plans.

- **Workspaces.** Everyone gets a personal workspace on first use. Teams share
  a team workspace with roles: viewer, member, admin, owner.
- **Projects** have their own statuses (each mapped to backlog, unstarted,
  started, completed or canceled), a working-day calendar, and a schedule mode.
- **Tasks** nest to any depth (a WBS). A task with subtasks is a summary whose
  dates and progress come from its subtasks.
- **Dependencies** of all four types (FS, SS, FF, SF) with lag or lead, within
  a project, checked for cycles.
- **Scheduling.** Manual mode never moves your dates and reports broken links as
  violations; auto mode schedules every task as early as its links allow. Both
  compute float and the critical path; deadlines show as negative float.

The engine in `src/schedule/` is pure (no database, clock or I/O) so the
frontend can run the same code for instant previews.

## Running

```bash
npm install --no-audit --no-fund   # not bun install: it hangs in this monorepo
bun run src/index.ts               # 127.0.0.1:3152
bash check.sh                      # typecheck + every test file must run
```

| Variable | Default | Purpose |
|---|---|---|
| `PORT` | 3152 | Listen port |
| `NEXUS_BIND_HOST` | 127.0.0.1 | Keep on loopback in production |
| `NEXUS_PROJECT_DB` | data/project.sqlite | SQLite path |
| `NEXUS_PROJECT_DASHBOARD_SECRET` | (none) | Secret for Dashboard's private hop; without it that hop is refused |
| `NEXUS_PROJECT_JWT_AUDIENCE` | project.tnhc.dev | Required audience on `x-nexus-identity` |
| `NEXUS_AUTH_INTERNAL_URL` | http://127.0.0.1:4310 | Where Auth's JWKS lives |
| `NEXUS_PROJECT_BASE_URL` | http://localhost:$PORT | Address registered with Cloud |
| `NEXUS_CLOUD_URL` / `NEXUS_CLOUD_API_KEY` | http://localhost:8787 / (none) | Cloud registration |
| `NEXUS_PROJECT_ENABLE_CLOUD_INTEGRATION` | true | Turn Cloud registration off |
| `NEXUS_PROJECT_CLOUD_HEARTBEAT_INTERVAL_MS` | 30000 | Cloud heartbeat interval in ms (minimum 5000) |

## API

Everything below is under `/api/v1/project` and needs an identity from a
trusted front door (see `src/auth.ts`). Errors are
`{ "error": "<code>", "message": "…" }`. Something you cannot see is `404`;
something you can see but may not change is `403`.

| Method | Path | Notes |
|---|---|---|
| GET | /workspaces | Your workspaces; the personal one exists from your first request of any kind |
| POST | /workspaces | `{ name }` → team workspace, you are owner |
| PATCH / DELETE | /workspaces/:id | Rename (admin) / delete (owner, team only) |
| GET | /workspaces/:id/members | |
| PUT / DELETE | /workspaces/:id/members/:subject | `{ role }`; a workspace keeps one owner |
| GET / POST | /workspaces/:id/projects | `?archived=true`; `{ key, name, startDate, … }` |
| GET / PATCH | /projects/:id | Settings: admin |
| POST | /projects/:id/move | `{ workspaceId }`, admin in both |
| GET / PUT / DELETE | /projects/:id/members[/:subject] | Who sees a restricted project |
| GET / POST | /projects/:id/statuses | |
| PATCH / DELETE | /statuses/:id | `DELETE ?moveTasksTo=<statusId>` when in use |
| GET / POST | /projects/:id/tasks | `?status=&assignee=<subject or none>&parent=<id or root>` |
| GET / PATCH / DELETE | /tasks/:id | `PATCH` needs `If-Match: <version>` |
| POST | /tasks/:id/move | `{ parentId, statusId, afterId }`, needs `If-Match` |
| GET / POST | /projects/:id/dependencies | `{ predecessorId, successorId, type, lagDays }` |
| PATCH / DELETE | /dependencies/:id | |
| GET / PUT | /projects/:id/calendar | `{ workingWeekdays: [1..5], exceptions: [{ date, working }] }` |
| GET | /projects/:id/schedule | Dates, float, critical path, violations, roll-ups, warnings |
| GET | /me/tasks | Open tasks assigned to you, soonest deadline first |

Dates are `YYYY-MM-DD`; durations and lags are working days. A milestone is
shown on the working day at whose start it occurs.

### Side effects: `rescheduled`

A write can change tasks other than the one you sent: auto mode moves
successors, every summary stores its children's roll-up, a task gaining its
first child becomes a summary, and a calendar change re-derives finishes. Each
of those bumps the other task's `version`. So every schedule-affecting write
returns a top-level `rescheduled` array alongside its own fields:

```json
{ "id": "…", "version": 4, "…": "…",
  "rescheduled": [{ "id": "…", "version": 7, "startDate": "2026-09-14",
                    "finishDate": "2026-09-16", "progress": 50 }] }
```

It lists every other task the write changed, with its new version, dates and
progress (empty when nothing else moved); the edited task itself is not
repeated. Routes: `POST /projects/:id/tasks`, `PATCH /tasks/:id`,
`POST /tasks/:id/move`, `DELETE /tasks/:id` (`{ deleted: true, rescheduled }`),
`POST /projects/:id/dependencies`, `PATCH` and `DELETE /dependencies/:id`,
`PUT /projects/:id/calendar`, `PATCH /projects/:id`, `PATCH` and
`DELETE /statuses/:id`. Update your copies from it and the next `If-Match`
will not conflict.
