# Nexus-Calendar

## Purpose

Shared calendars with CalDAV, reminders, contacts, and federation

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
