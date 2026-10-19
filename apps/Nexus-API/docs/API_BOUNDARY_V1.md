# Nexus platform API boundary — v1

Status: implementation baseline
Owner: Nexus Systems platform
Public host: `api.tnhc.dev`

## Purpose

`api.tnhc.dev` is the public platform facade for hosted Nexus clients, the
Dashboard shell, SDKs, and future applications. It provides stable platform
contracts and forwards a small number of user-facing resources to the service
that owns them. It is not a replacement for Auth, Cloud, or an application's
private service API.

## Ownership

| Surface | Owner | Public API responsibility |
| --- | --- | --- |
| Identity, sessions, tokens, roles, permissions | Nexus Auth | Authenticate the caller and issue/validate credentials. The API never creates a second identity authority. |
| App catalog, release approval, node placement, federation, exposure, domains, infrastructure | Nexus Cloud | Own the control-plane records and operator actions. The API does not duplicate Cloud's state. |
| Platform facade, versioning, SDK-facing discovery, common errors | Nexus API | Present stable `/api/v1` resources, authenticate through Auth, and delegate to the owning service. |
| Mailbox data and delivery | Nexus Email | Own mailbox authorization and message state. The API exposes only the declared namespaced facade and keeps mailbox ownership in Mail. |
| Terminal sessions and audited shell access | Nexus Terminal | Own session authorization, command audit, and WebSocket attach. The API must never expose a generic command runner. |
| Calendar events and sharing | Nexus Calendar | Own event ownership, public share tokens, and calendar persistence. |
| Dashboard presentation and app shelves | Nexus Dashboard | Render the catalog and native views. It must consume the platform API rather than carry Cloud credentials. |

## Credential rules

1. Hosted browser requests carry the shared `nexus_session` cookie to the API.
2. API clients may send `Authorization: Bearer <credential>`.
3. Nexus API forwards the credential to Auth for `/api/v1/me`; it does not trust a
   browser-supplied identity or subject header.
4. The Cloud API key is server-side only. It is used by Nexus API when it
   delegates catalog reads and is never returned to a caller.
5. Operator-only API routes require the configured API operator token. A
   missing token is permitted only in development and tests; production fails
   closed.
6. Every application keeps ownership checks in its own service. A platform
   facade must not turn an app route into a cross-user read or write.

## Stable v1 resources

| Method | Path | Access | Delegates to |
| --- | --- | --- | --- |
| GET | `/health` | public | Nexus API |
| GET | `/api/v1/contract` | public | Nexus API |
| GET | `/api/v1/openapi.json` | public | Nexus API |
| GET | `/api/v1/me` | session or bearer | Auth `/api/v1/auth/me` |
| GET | `/api/v1/apps` | session or bearer | Cloud `/api/v1/catalog`, filtered to approved stable released user apps |
| GET | `/api/v1/catalog` | session or bearer | Alias of `/api/v1/apps` |
| GET/POST | `/api/v1/mail/*` | session or bearer | Nexus Email mailbox facade; the gateway resolves the Auth subject and forwards only declared Mail paths |

The existing `/api/v1/gateway/*` routes remain a compatibility surface for
local operators. They are not the application API and are operator-protected.
New app integrations must not add arbitrary dynamic proxy routes.

## App release admission

Every future app follows the same sequence:

1. The app registers with Cloud and publishes its owned service contract.
2. Cloud records its catalog proposal and an authorized platform reviewer
   approves the placement, surface, audience, and release metadata.
3. The app is eligible for `/api/v1/apps` only when it is a stable, approved,
   released `user-app` and is not hidden.
4. The app's own service remains responsible for request authorization and data
   ownership after the platform facade admits it to the catalog.

Staged, retired, hidden, admin-console, developer-tool, and platform-service
records remain discoverable only through their owning control-plane surfaces.

## Versioning rules

- `/api/v1` is the first stable public namespace.
- Additive fields and new resources are compatible changes.
- Removing or changing the meaning of a field requires `/api/v2` and a
  deprecation period.
- Every error is JSON with a stable `error` code and optional `requestId`.
- OpenAPI is published at `/api/v1/openapi.json`; the contract endpoint is a
  small machine-readable discovery document for clients that cannot load the
  full description.

## Hosted and federated modes

The hosted API and self-hosted node APIs share resource vocabulary but do not
share operator assumptions. `api.tnhc.dev` serves the hosted platform. A
self-hosted node may expose the same protocol from its own origin and may add
operator routes, but it must declare its mode and capabilities in its contract.
An operator is a node administrator in federated mode; a hosted user or
founder is not silently converted into a node operator by this API.
