# Nexus Wiki Docs

## MVP Architecture

- API Layer: FastAPI app in `src/nexus_wiki/main.py`.
- Domain Models: Pydantic models in `src/nexus_wiki/models.py`.
- Storage Adapter: backend-selected adapter in `src/nexus_wiki/store.py`.
	- In-memory adapter for local fallback/dev.
	- PostgreSQL adapter for persistence.
	- Startup readiness/migration check via `ensure_ready()`.
- Tests: API contract tests in `tests/test_api.py`.

## API Contract (v1)

- `GET /health`
	- Returns service liveness shape:
	- `{ "status": "ok", "service": "nexus-wiki" }`

- `GET /api/v1/pages`
	- Returns page summaries and total count.

- `POST /api/v1/pages`
	- Creates page from title/content.
	- Slug is derived from title.
	- Conflict if slug already exists.

- `GET /api/v1/pages/{slug}`
	- Returns full page by slug.

- `PATCH /api/v1/pages/{slug}`
	- Partial update for title/content.
	- Creates a versioned revision record with metadata.

- `GET /api/v1/pages/{slug}/revisions`
	- Returns descending revision history for a page.

- `POST /api/v1/integrations/nexus-cloud/register`
	- Forwards service registration payload to Nexus-Cloud Systems API.

- `GET /api/v1/search?q=...`
	- Full-text match over title and content.

## Compatibility Notes

- API pathing uses explicit version prefix: `/api/v1/...`.
- Health payload includes stable `service` identifier for upstream orchestration.
- Registration forwarding URL is configurable via `NEXUS_CLOUD_SYSTEMS_API_URL`.
- Persistence backend is configurable via `NEXUS_WIKI_STORE_BACKEND` and `DATABASE_URL`.
- Error semantics are explicit for expected contract cases:
	- 400 for invalid slug derivation.
	- 404 for missing article.
	- 409 for slug conflicts.

## Rollout Plan (Early)

1. Add authentication and authorization hooks from Nexus identity layer.
2. Add service heartbeat and periodic status sync with Nexus-Cloud.
3. Add migration strategy tooling for PostgreSQL schema evolution.
4. Add revision diff storage and rollback primitives.

## Local Persistent Runtime

- Compose file: `docker-compose.yml`
- Profile: `persistent`
- Command: `docker compose --profile persistent up --build`

The API container runs startup validation against PostgreSQL and auto-creates required schema (`wiki_pages`, `wiki_page_revisions`) if missing.
