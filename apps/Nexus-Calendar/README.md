# Nexus-Calendar

## Purpose

Personal and explicitly shared calendars with revocable public read-only links.

Calendar is a single proxied-app frontend: it is available at `/calendar` in
the Dashboard shell and directly at `https://calendar.tnhc.dev`. Both origins
serve the same frontend artifact; the Dashboard route is not an iframe.

## Quick Start

```bash
bun install
bun run dev
```

## Endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | /health | Health check |
| GET | /api/v1/status | Service status and contracts |
| GET | /api/v1/calendar/events | List events visible to the signed-in caller |
| POST | /api/v1/calendar/events | Create a private event |
| GET | /api/v1/calendar/public/:token | Read a public event link |
| GET | /share/:token | Public read-only event page |

## Configuration

| Variable | Default | Purpose |
|----------|---------|---------|
| PORT | 3068 | Listen port |
| NEXUS_CLOUD_URL | http://localhost:8787 | Cloud control plane |
| NEXUS_CLOUD_API_KEY | (none) | Cloud API key |
| `NEXUS_CALENDAR_ENABLE_CLOUD_INTEGRATION` | true | Enable/disable cloud heartbeat |
| `NEXUS_CALENDAR_DB` | `data/calendar.sqlite` | SQLite event database |
| `NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT` | (none) | Required to migrate a populated pre-ownership database; legacy events are assigned to this explicit subject |

## Event ownership migration

Events are private to an owner subject. New events store `owner_subject`; API responses include
`ownerSubject` and the caller's current `access` (`owner`, `editor`, or `viewer`). This release
returns `owner` for owned events and reserves editor/viewer grants for the sharing layer.

On startup, Calendar migrates SQLite transactionally. A populated legacy `events` table without
`owner_subject` fails closed with `legacy_owner_required` unless
`NEXUS_CALENDAR_LEGACY_OWNER_SUBJECT` is configured. Empty legacy tables migrate without an
implicit owner.

## Sharing and public links

Owners may share an event with another Nexus subject as `viewer` or `editor`.
Viewers can read; editors can update event details; only the owner can delete
or change sharing. Public links are bearer capabilities: anyone with the URL
can read the filtered event fields, so treat them like sensitive invitations.
Creating a replacement link invalidates the previous token, and owners can
revoke a link at any time. The public page and API never expose ownership,
sharing metadata, or the internal event ID.

## Local two-origin startup

Run the backend on `127.0.0.1:3068`, build `frontend`, and serve
`frontend/dist` through the production Caddyfile on `:8092`. For Dashboard
testing, configure the Dashboard Calendar proxy to target
`http://127.0.0.1:8092`; for direct testing, use `http://localhost:8092`.
Set `NEXUS_CALENDAR_DASHBOARD_SECRET` consistently in Dashboard and Calendar.
