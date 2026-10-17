# Nexus App Catalog and Curated Shelves

**Status:** Design approved in chat; implementation not started

**Date:** 2026-10-17

## Summary

Nexus will present a large ecosystem of applications through a curated catalog
rather than a flat launcher. Applications can suggest how they should be
classified, while Nexus Cloud approves the metadata that becomes visible to
users. The hosted `tnhc.dev` platform, hosted tenants, self-hosted nodes, and
individual users each control a separate layer of the result.

The first user-facing shelves are:

- **Everyday** — mail, calendar, chat, files, and other frequent tasks.
- **Work** — project, documents, tasks, meetings, and collaboration.
- **Create** — design, graphic, photos, video, draw, and other creative tools.
- **Develop** — Forge, Deploy, Terminal, APIs, and development tools.
- **Operate** — Cloud, Auth, Network, Vault, monitoring, and administration.
- **Personal** — health, finance, home, and quality-of-life tools.
- **Explore** — new, unreviewed, experimental, or long-tail applications.

The shelf list is controlled metadata. It is not a permission system and it
does not grant access to an application.

## Goals

1. Make the most common apps easy to find without showing hundreds of tiles.
2. Keep the full ecosystem searchable and browsable.
3. Give application authors a clear way to propose catalog metadata.
4. Give Nexus Cloud a controlled, auditable approval workflow.
5. Keep global, tenant, node, and user decisions separate.
6. Preserve the existing single sign-on and delivery model.
7. Let the taxonomy evolve from real usage and user feedback.

## Non-goals

- Replacing application authorization with catalog metadata.
- Giving hosted customers control over the global TNHC catalog.
- Creating arbitrary user-defined global shelves in the first release.
- Rebuilding the application delivery, health, or routing systems.
- Requiring every existing application to update before the catalog works.

## Authority boundaries

Every catalog action has an explicit scope.

| Scope | Authority | Responsibilities |
|---|---|---|
| Platform | TNHC platform owner, platform admins, catalog stewards | Global taxonomy, global app metadata, platform users, platform policy |
| Tenant | Tenant owner and tenant admins | Membership, entitlements, tenant defaults, approved-app availability |
| Node | Self-hosted or federated node owner and node admins | Local app installation, local visibility, local ordering, local apps |
| User | Individual account | Pins, recents, preferred shelf, personal layout |
| Publisher | App owner or app registration | Catalog proposal only |

The new permission names are scope-qualified:

```text
platform:users:manage
platform:catalog:approve
tenant:members:manage
tenant:apps:manage
node:apps:manage
user:preferences:write
```

Existing `founder` and `admin` roles remain compatible during migration. New
catalog code must not introduce an unqualified `operator` permission. A
platform catalog steward is an internal platform responsibility; a node admin
is the administrator of one self-hosted or federated node.

## Catalog metadata

### Controlled values

The initial shelf enum is:

```text
everyday | work | create | develop | operate | personal | explore
```

The initial audience enum is:

```text
general | business | creator | developer | operations | power-user
```

Capabilities are controlled identifiers owned by the catalog contract. Examples
include `messaging`, `calendar`, `files`, `project-management`,
`image-editing`, `video-editing`, `repository-management`, `deployment`,
`secrets`, and `monitoring`. Free-form keywords are allowed for search, but
they do not affect placement or authorization.

### Proposal and approval records

An app may submit a proposal containing:

```ts
type CatalogProposal = {
  suggestedShelves: CatalogShelf[];
  suggestedAudiences: CatalogAudience[];
  capabilities: string[];
  keywords: string[];
  submittedAt: string;
  version: number;
};
```

The approved global record contains:

```ts
type ApprovedCatalog = {
  primaryShelf: CatalogShelf;
  secondaryShelves: CatalogShelf[];
  audiences: CatalogAudience[];
  capabilities: string[];
  maturity: "stable" | "preview" | "experimental";
  visibility: "default" | "discoverable" | "restricted" | "hidden";
  approvedAt: string;
  approvedBy: string;
  version: number;
};
```

The stored catalog state keeps the latest proposal and the last approved
record. A proposal update does not replace an existing approved placement until
it is reviewed.

## Approval workflow

1. Registration or an app metadata update submits a proposal.
2. Cloud validates enum values and stores the proposal.
3. The proposal enters the platform catalog review queue.
4. A catalog steward approves, edits, defers, or rejects it.
5. The approved record becomes the global catalog projection.
6. Every change records the reviewer, timestamp, previous value, and new value.

Unreviewed or deferred applications resolve to Explore. Rejected applications
follow their approved visibility policy; they never become default-shelf apps
without approval.

The hosted platform may have several platform administrators with different
permissions. This does not create several global catalogs. One approved global
record is shared by the hosted platform. `approvedBy` identifies the
platform-scoped principal that performed the review; it is never a tenant or
node administrator acting through an implicit global role.

## Effective catalog resolution

The effective app list is resolved in this order:

```text
global approved catalog
→ tenant entitlements
→ node-local policy and local apps
→ runtime health and access state
→ user preferences
```

Each layer has a single responsibility:

- Global metadata defines official shelves and capabilities.
- Tenant policy controls which approved apps an organization offers.
- Node policy controls local installations and local overrides.
- Health reports current availability.
- Access reports whether the current user may launch the app.
- User preferences change only that user’s presentation.

Shelf placement never grants access. Authorization remains enforced by the
platform and the application.

## API shape

The existing tool registry remains the runtime source for identity, health,
delivery, and routing. Catalog metadata is added as a separate projection of
that registry.

### Cloud

- Registration accepts an optional catalog proposal.
- Tool reads include the latest proposal and approved catalog state for trusted
  catalog consumers.
- A dedicated catalog approval operation is protected by
  `platform:catalog:approve`.
- Node-local overrides use a separate node-scoped operation protected by
  `node:apps:manage`.
- Catalog changes are persisted with audit history and are backward compatible
  with records that have no metadata.

### Dashboard

`/ipa/apps` continues to return normalized app entries, extended with:

```ts
type DashboardCatalog = {
  primaryShelf: CatalogShelf;
  secondaryShelves: CatalogShelf[];
  audiences: CatalogAudience[];
  capabilities: string[];
  maturity: "stable" | "preview" | "experimental";
  visibility: "default" | "discoverable" | "restricted" | "hidden";
  reviewStatus: "approved" | "pending" | "deferred" | "rejected";
};
```

Older Dashboard servers that do not return catalog metadata map apps to
Explore. Existing `path`, `delivery`, and `health` behavior remains unchanged.

### User preferences

Pins, recents, and the preferred shelf become user-scoped preferences so they
can follow the user across devices. The current local-storage preferences stay
as a migration fallback while the account preference store is introduced.

## Dashboard experience

### Home

Home shows:

1. Essentials: pins and recently opened apps.
2. A compact Everyday shelf.
3. A recommended shelf based on the user’s preferred workspace.
4. A route to Explore and the full catalog.

Home never renders the full ecosystem as a tile wall.

### Apps drawer

The current drawer becomes the fast launcher:

- Search across every app.
- Browse by shelf.
- Filter by audience and capability.
- Show pins and recents first.
- Keep offline apps searchable but visibly unavailable.
- Keep the empty-query result bounded.

### All Apps

`/apps` becomes the complete catalog view:

- Grouped by approved shelf.
- Searchable and filterable.
- Clear maturity, health, and access states.
- Explore contains pending and long-tail apps.

### Role-aware ordering

The initial default is Everyday. Users may choose a preferred shelf. Future
role-aware defaults can place Develop, Create, or Operate first without changing
the global catalog or the user’s permissions.

## Self-hosted and federated behavior

Self-hosted nodes consume the global catalog when connected and may maintain a
node-local overlay when disconnected or operating independently.

Node admins may:

- Enable or disable approved apps for their node.
- Reorder approved shelves locally.
- Add local applications under Explore or a node-approved local shelf.
- Set local defaults for their node’s users.

Node admins cannot edit the global approved metadata. A local shelf assignment
is node metadata for that node's local app overlay; it does not become a global
catalog record. Local changes are clearly scoped to that node and do not appear
as global changes on `tnhc.dev`.

## Rollout plan

### Phase 1: Contract and fallback

- Add proposal and approved metadata types.
- Store metadata without changing existing launch behavior.
- Map missing metadata to Explore.
- Add Cloud validation and audit tests.

### Phase 2: Dashboard projection

- Extend `/ipa/apps` with catalog metadata.
- Add shelf grouping to `/apps`.
- Add drawer shelf filters while preserving search.
- Seed metadata for the current core apps.

### Phase 3: Platform governance

- Add the internal catalog review queue.
- Add approval, override, defer, reject, and rollback actions.
- Enforce `platform:catalog:approve`.

### Phase 4: Tenant, node, and user layers

- Add tenant app entitlements.
- Add node-local overrides and local apps.
- Move pins, recents, and preferred shelf into account preferences.
- Add migration from local storage.

## Testing and acceptance criteria

### Cloud tests

- Validate allowed shelf, audience, maturity, and visibility values.
- Verify app proposals cannot approve themselves.
- Verify only platform catalog permissions can publish global metadata.
- Verify node permissions cannot modify global records.
- Verify audit history and rollback.
- Verify old records without metadata resolve safely to Explore.

### Dashboard tests

- Group approved apps into the correct shelves.
- Keep the drawer’s default result bounded.
- Search reaches healthy, offline, pending, and Explore apps according to
  visibility policy.
- Apply tenant, node, health, access, and user-preference precedence.
- Keep existing routes, delivery modes, and SSO behavior intact.
- Verify keyboard navigation and screen-reader labels for shelf controls.

### Product acceptance

- A normal user can reach Mail, Calendar, Chat, and other Everyday apps quickly.
- A developer can choose Develop without changing the global catalog.
- A creator can choose Create without seeing the full ecosystem on Home.
- A self-hosted node admin can manage local availability without changing
  `tnhc.dev`.
- A platform catalog steward can review and audit global placement.

## Feedback and evolution

Shelf definitions and default ordering are versioned catalog data. Usage and
explicit feedback can inform revisions without changing app URLs, identity, or
authorization. Taxonomy changes should be reviewed as product changes and
rolled out through the same audited platform process.
