# Nexus route inventory

Inventory date: 2026-10-19

This is the ownership map used while moving clients to `api.tnhc.dev`. The
service contracts remain authoritative for exact request and response shapes.

## Platform facade

| Service | Source of truth | Current role |
| --- | --- | --- |
| Nexus API | `apps/Nexus-API/src/server.ts`, `src/api-contract.ts` | Public v1 facade, contract discovery, and compatibility gateway. |
| Nexus Auth | `apps/Nexus-Auth/src/auth-contract.ts` | Identity provider and session/token authority. |
| Nexus Cloud | `apps/Nexus-Cloud/src/api/routes.ts`, `src/api/cloud-control-contract.ts` | Cloud control plane and catalog owner. |
| Dashboard | `apps/Nexus-Dashboard/src/server.ts` | Browser shell and temporary same-origin proxies during migration. |

## Auth-owned routes

Auth owns `/api/v1/auth/*` and account identity operations. The platform API
may delegate identity reads, but it must not duplicate login, session, token,
JWKS, role, or permission logic.

| Group | Routes |
| --- | --- |
| Public | `/health`, readiness, contract, registration metadata, login, access requests, claim, OAuth JWKS, service-token validation |
| Session | `/api/v1/auth/me`, trust, catalog preferences, API-key listing, auth check, OAuth userinfo |
| Permissioned | API-key issue/revoke, service-token issue |
| Founder/admin | Hosted user listing |

Exact manifest: `apps/Nexus-Auth/src/auth-contract.ts`.

## Cloud-owned routes

Cloud owns the control-plane state. The route manifest currently covers health
and contract discovery; architecture and state; node trust and workload
placement; federation; users and tools; the approved catalog and tenant/node
overlays; exposure, address, domain, routing, DNS, Guardian, audit, storage,
and deployment operations.

The catalog routes used by the Dashboard are:

- `GET /api/v1/catalog` — approved catalog records.
- `GET /api/v1/tools` — legacy registry compatibility.
- `GET /api/v1/apps` — canonical Cloud app catalog alias.
- `GET /api/v1/connections` and `GET /api/v1/topology` — topology metadata.

Exact manifest: `apps/Nexus-Cloud/src/api/routes.ts` with access derivation in
`apps/Nexus-Cloud/src/api/cloud-control-contract.ts`.

## Mail-owned routes

The Rust mail API owns mailbox data and message operations:

- `GET /api/v1/health`
- `GET /api/v1/folders`
- `GET /api/v1/folders/:id/messages`
- `GET /api/v1/messages/:id`
- `POST /api/v1/messages/:id/seen`
- `POST /api/v1/messages`
- `GET /api/v1/search`
- `GET /api/v1/threads/:id/messages`
- `GET /api/v1/messages/:id/attachments/:index`

Dashboard's `/ipa/mail/*` shell path now calls the platform facade when
configured; Nexus API resolves Auth's subject and forwards only this allowlist.
The direct loopback path remains as a rollout fallback and retains the same
subject header protection.

## Terminal-owned routes

Terminal owns health, status, contract, audited attach, audit records, and
internal session/command records:

- `GET /health`
- `GET /api/v1/status`
- `GET /api/v1/terminal/contract`
- `WebSocket /api/v1/terminal/attach`
- `GET /api/v1/terminal/audit`
- Internal `/api/v1/terminal/sessions` and `/api/v1/terminal/commands`

The shell-only attach path remains separately authorized by Dashboard and
Terminal. Nexus API must not become a generic terminal proxy.

## Calendar-owned routes

Calendar owns event persistence and sharing:

- `GET /health`
- `GET /api/v1/status`
- `GET|POST /api/v1/calendar/events`
- `GET /api/v1/calendar/events/:id`
- `PATCH|DELETE /api/v1/calendar/events/:id`
- `GET /api/v1/calendar/public/:token`
- `GET /share/:token`

Owner checks remain in Calendar. Public share tokens are the only public event
read surface.

## Other application routes

These applications remain owners of their runtime APIs. They are listed here so
new platform facade work does not accidentally treat an app shell or a private
service endpoint as a platform resource.

| Application | Browser or service surface | Ownership rule |
| --- | --- | --- |
| Nexus Chat | `chat.<domain>` web shell and Rust `/api/v1/*` channels, DMs, reactions, attachments, and WebSocket gateway | Chat validates membership and message permissions in its own service. |
| Nexus Draw | `draw.<domain>` editor shell and collaboration endpoints | Draw owns board/document access; the public host exception is explicit in the production gate. |
| Nexus Project | `project.<domain>` shell and project/task endpoints | Project owns project membership, task state, and board permissions. |
| Nexus Dashboard | `app.<domain>` shell, `/ipa/apps`, `/ipa/me`, and app-specific compatibility proxies | Dashboard is a presentation shell; it must not become a second identity or catalog authority. |
| Nexus Cloud admin console | `cloud.<domain>` browser surface and Cloud control-plane routes | Cloud owns operator and federation workflows; hosted users are not implicitly operators. |

The route inventory is intentionally separate from the release catalog. A route
can exist in source while its app remains unreleased and therefore absent from
the Dashboard shelves. Only an approved, healthy release is eligible for the
hosted app catalog.

## Migration status

- Dashboard catalog and identity migration: first vertical slice complete; the
  shell uses the facade when `NEXUS_API_INTERNAL_URL` is configured.
- Dashboard and Nexus Mail: first `/api/v1` facade slice is implemented and
  covered by gateway and Dashboard integration tests.
- Terminal and Calendar: inventory complete; facade work follows their own
  contract review.
- Cloud and Auth: remain owners; no state is being moved into Nexus API.
