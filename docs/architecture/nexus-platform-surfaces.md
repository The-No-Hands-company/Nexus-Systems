# Nexus platform surfaces

This document is the naming and access contract for the hosted TNHC platform and
for self-hosted or federated Nexus nodes. It keeps an application, a service,
and an administrative console from being presented as the same kind of thing.

## Hosted routes

| Route | Canonical meaning | Surface | Who uses it |
| --- | --- | --- | --- |
| `/` | Nexus Dashboard home and curated launcher | user-app shell | every signed-in user |
| `/chat` | Nexus Chat, the hosted messaging application backed by `apps/Nexus` | user-app | users, teams, communities |
| `/mail` | Nexus Mail shell-native mailbox | user-app | users |
| `/calendar` | Nexus Calendar | user-app | users, teams |
| `/project` | Nexus Project | user-app | teams, developers |
| `/design`, `/graphic`, `/photos`, `/video`, `/draw` | creative applications | user-app | creators |
| `/cloud/*` | Nexus Cloud control-plane views inside the Dashboard shell | admin-console | platform admins and self-hosted node operators |
| `/terminal` | audited operator and developer terminal | developer-tool | platform admins, node operators, approved developers |
| `/auth` | Nexus Auth identity service and onboarding pages | platform-service | the platform and identity clients; never a normal app tile |

`/nexus` is retained only as a compatibility redirect to `/chat`. The repository
directory `apps/Nexus` is the historical name of the Chat service; it is not a
second hosted product.

## Hosted roles and node roles

Hosted TNHC uses **platform admin** for a person who administers the shared
`tnhc.dev` service. A self-hosted or federated installation uses **node
operator** for the person who controls that node's runtime, storage, trust, and
local app selection. A node operator is not automatically a hosted platform
admin, and a hosted user does not become a node operator by opening Chat.

Nexus Auth owns identity and sessions in both modes. Nexus Cloud owns registry,
policy, routing, federation, and node control in both modes, with the authority
limited to its scope. Neither service is an everyday application.

## Catalog surfaces and shelves

Every catalog entry has two separate concepts:

- `surface`: `user-app`, `admin-console`, `platform-service`, or
  `developer-tool`.
- `primaryShelf` and `secondaryShelves`: presentation placement such as
  Everyday, Work, Create, Develop, Operate, Personal, or Explore.

A shelf never grants access. The effective resolver applies global approval,
tenant entitlements, node policy, runtime health, and the app's access decision
before user pins or recents change ordering. Auth is filtered as a platform
service; Cloud and Terminal are labelled as operator/developer surfaces and are
shown only where the caller's role permits them.

## API boundaries

- `/api/v1/catalog` on Nexus Cloud is the global approved catalog projection.
- `/api/v1/tenants/:tenantId/catalog` is tenant availability and presentation
  policy; it cannot approve a global app.
- Node overlays are node-scoped and may add local apps under Explore without
  turning them into globally approved applications.
- `/api/v1/auth/preferences/catalog` is the self-scoped account preference
  contract for pins, recents, and preferred shelf. It changes presentation only.
- Auth `/api/v1/auth/contract` is the public identity-service manifest. It
  describes Auth as a `platform-service` at hosted `/auth`; it is not a user
  launcher entry and the manifest itself grants no access.
- Dashboard `/apps` is the browser-facing projection. It combines Cloud's
  catalog with shell-native Mail and Terminal entries and never exposes Cloud
  credentials to the browser.
- AI Hub `/api/v1/knowledge` is the ecosystem-grounded retrieval contract. It
  carries source and trust metadata and falls back to its local platform index
  when Cloud catalog reads are unavailable.
- Nexus API `/api/v1/contract` is the versioned service manifest for clients
  that need the stable API boundary. It describes authentication and routes;
  it does not grant access by itself.
- Terminal `/api/v1/terminal/contract` is a separate guarded-shell manifest.
  It is public metadata, while attach and audit remain founder/admin-only and
  the shell stays disabled by default.

## Nexus AI starting point

Nexus AI should first be an ecosystem-grounded assistant. Its initial knowledge
source is a versioned, permission-filtered index of Nexus and TNHC contracts:
app identities, routes, surface roles, API manifests, deployment and federation
guides, and current release notes. Retrieval must carry source path, revision,
scope, and visibility so a user cannot ask the assistant to reveal an operator
or tenant-private document. External LLM providers are a later execution layer,
not the source of truth. The old VersaAI code is legacy until it is explicitly
adopted behind this contract.

## Terminal and CLI

Nexus Terminal is the browser-facing audited shell. `packages/nexus-cli` is the
local CLI prototype for power users and operators. The CLI should call the same
versioned API contracts as the Dashboard, authenticate with a scoped Auth API
key or service token, and make destructive operations explicit. It must not
reach into Cloud's JSON stores or bypass platform approval. The next CLI scope
is read-only discovery and health, then catalog and node administration, then
development workflows with separate scopes and audit events.
