# Nexus API foundation

## Objective

Make `api.tnhc.dev` the stable platform facade for the released Nexus
ecosystem while preserving Auth, Cloud, and app service ownership.

## Execution order

1. Publish the v13 public-surface audit as a repository artifact.
2. Record the ownership and credential boundary in Nexus API docs.
3. Inventory Auth, Cloud, Mail, Terminal, Calendar, and Dashboard routes.
4. Add `/api/v1/me`, `/api/v1/apps`, `/api/v1/catalog`, and OpenAPI discovery.
5. Route Dashboard catalog and identity reads through Nexus API in production.
6. Put `api.tnhc.dev` behind the production proxy and run the service on a
   loopback-only upstream.
7. Validate the first vertical slice, then add app facades one at a time.

## Non-goals for this slice

- Moving Auth or Cloud state into Nexus API.
- Replacing app-local ownership checks.
- Exposing dynamic arbitrary proxy routes to ordinary users.
- Publishing unfinished applications in the launcher.

## Acceptance gates

- `apps/Nexus-API` strict typecheck, lint, and tests pass.
- OpenAPI and contract endpoints agree on the stable v1 resources.
- Dashboard tests prove the API is used when configured and Cloud credentials
  never reach the browser.
- Proxy tests prove `api.tnhc.dev` reaches only the loopback API service.
- Production status reports API and health endpoints without adding API to the
  end-user app shelf.
