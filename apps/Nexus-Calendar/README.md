# Nexus-Calendar

## Purpose

Personal calendars with events, reminders, and month/week views.

Events are **private to their owner**. There is no global calendar: every read
and write is scoped to the subject the request arrives with, and an event
belonging to somebody else is indistinguishable from one that does not exist.

## Quick Start

```bash
bun install
bun run dev
```

## Identity

The service is loopback-only and has no session of its own. It accepts a caller
from exactly two places, and the browser can forge neither:

| Source | Header | Trusted when |
|--------|--------|--------------|
| Ecosystem proxy | `x-nexus-identity` | RS256 signature verifies against Auth's JWKS **and** the audience matches `NEXUS_CALENDAR_JWT_AUDIENCE` |
| Dashboard hop | `x-nexus-subject` | the request also carries `x-nexus-dashboard-secret` matching `NEXUS_CALENDAR_DASHBOARD_SECRET` |

A bare `x-nexus-subject` with no secret is ignored. Everything except `/health`
and `/api/v1/status` answers `401` without a trusted caller.

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Liveness. Public. |
| GET | /api/v1/status | Service status and capabilities. Public. |
| GET | /api/v1/calendar/events?from=&to= | The caller's events overlapping the window |
| POST | /api/v1/calendar/events | Create, owned by the caller |
| GET | /api/v1/calendar/events/:id | One event, `404` if not the caller's |
| PATCH | /api/v1/calendar/events/:id | Update, `404` if not the caller's |
| DELETE | /api/v1/calendar/events/:id | Delete, `404` if not the caller's |

Ranges use **interval overlap**: an event is returned when it starts before the
window ends and ends after the window begins. A multi-day event that began last
month appears in this month, which containment (`start >= from AND end <= to`)
would have dropped. A date-only bound means the whole of that day.

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| `PORT` | 3068 | Listen port |
| `NEXUS_BIND_HOST` | 127.0.0.1 | Bind address — keep on loopback in production |
| `NEXUS_CALENDAR_DB` | data/calendar.sqlite | SQLite path |
| `NEXUS_CALENDAR_DASHBOARD_SECRET` | (none) | Shared secret for the private Dashboard hop. Without it the hop is refused entirely. |
| `NEXUS_CALENDAR_JWT_AUDIENCE` | calendar.tnhc.dev | Required audience on `x-nexus-identity` |
| `NEXUS_AUTH_INTERNAL_URL` | http://127.0.0.1:4310 | Where to fetch Auth's JWKS |
| `NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT` | (none) | See migration below |
| `NEXUS_CALENDAR_BASE_URL` | http://localhost:$PORT | Address advertised to Cloud |
| `NEXUS_CLOUD_URL` | http://localhost:8787 | Cloud control plane |
| `NEXUS_CLOUD_API_KEY` | (none) | Cloud API key |
| `NEXUS_CALENDAR_ENABLE_CLOUD_INTEGRATION` | true | Enable/disable the cloud heartbeat |

## Migrating a pre-ownership database

Schema version is tracked in `PRAGMA user_version`. Version 0 is the original
table, which had no owner column at all.

Events created before ownership have **no recorded owner and none can be
inferred**, so startup fails closed:

```
legacy_owner_required: 1 event(s) predate ownership and have no recorded owner.
Set NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT to the subject that should own them.
```

Set that variable to the Auth user id those events belong to (`usr-…`) and start
the service once. The migration runs in a single transaction: the table is
rebuilt with `owner_subject NOT NULL`, every existing row is backfilled to that
subject, and `user_version` becomes 1. After that the variable is never read
again and can be removed.

Nothing is ever made globally visible as a migration shortcut, and no owner is
guessed — a wrong guess hands one person's calendar to another.
