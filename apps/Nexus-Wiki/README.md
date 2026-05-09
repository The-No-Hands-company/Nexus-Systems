# Nexus Wiki

Status: MVP foundation in progress
Blueprint source: Nexus Systems Ecosystem Blueprint v1.2

## Purpose

Nexus Wiki is a Wikipedia-like knowledge platform implemented with Nexus design goals:

- Open article contribution and collaborative editing workflows.
- Strong API-first service boundaries for Nexus-Cloud compatibility.
- Modular runtime that works in standalone and orchestrated deployments.

## Dual Mode Target

- Standalone: docker compose based local runtime
- Orchestrated: integrated with Nexus-Cloud via Nexus Systems API

## Initial Scaffold Layout

- src/: implementation code
- docs/: design and architecture notes
- tests/: automated tests

## Implemented In This Iteration

- FastAPI service entrypoint with health endpoint at `/health`.
- Wiki article API (create, list, get, update) under `/api/v1/pages`.
- Wiki revision history endpoint under `/api/v1/pages/{slug}/revisions`.
- Search endpoint under `/api/v1/search`.
- Storage adapter with in-memory and PostgreSQL backends behind the same store interface.
- Nexus-Cloud registration client endpoint under `/api/v1/integrations/nexus-cloud/register`.
- Automated API tests for health, CRUD path, conflict path, and search.

## Local Development

1. Install dependencies:
	 - `pip install -e .[dev]`
2. Run the service:
	 - `uvicorn nexus_wiki.main:app --reload --app-dir src`
3. Run tests:
	 - `pytest`

## One-Command Persistent Mode (Docker Compose)

Run PostgreSQL + API with persistent storage profile:

- `docker compose --profile persistent up --build`

This starts:

- PostgreSQL at `localhost:5432`
- Nexus Wiki API at `http://localhost:9000`

On API startup, a migration/readiness check runs automatically (`store.ensure_ready()`), which validates DB connectivity and ensures required tables exist.

## Configuration

- `NEXUS_WIKI_STORE_BACKEND`
	- `auto` (default): uses PostgreSQL when `DATABASE_URL` starts with `postgresql`, else in-memory.
	- `postgres`: force PostgreSQL backend.
	- any other value: in-memory backend.
- `DATABASE_URL`
	- PostgreSQL connection string used by the persistence adapter.
- `NEXUS_CLOUD_SYSTEMS_API_URL`
	- Target URL for Nexus-Cloud registration endpoint forwarding.

## Next Steps

1. Persist articles with Nexus-backed storage instead of in-memory state.
2. Add authentication and authorization hooks from Nexus identity layer.
3. Add service heartbeat/status sync with Nexus-Cloud Systems API.
