# Nexus Catalog Scoped Resolution and Preferences Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to execute this plan task by task with verification after each task.

**Goal:** Add explicit tenant, node, and user layers to catalog resolution while keeping platform approval authoritative and user preferences presentation-only.

**Architecture:** A pure resolver consumes the approved global catalog, tenant entitlements, node overlay, runtime health/access state, and user preferences in that order. Tenant and node stores are separate from global approval records. User pins, recents, and preferred shelf are account preferences with local-storage fallback during migration.

**Tech Stack:** Bun, strict TypeScript, JSON persistence for Cloud overlays, Nexus-Auth account storage, React hooks, `bun:test`, Vitest.

**Spec:** `docs/superpowers/specs/2026-10-17-nexus-app-catalog-and-shelves-design.md`

## Global Constraints

- Scope names are explicit: platform, tenant, node, user, and publisher.
- Node or tenant administrators can change availability and presentation only inside their scope; they cannot edit global approved metadata.
- Shelf placement never grants access; the resolver must preserve an explicit access decision even when a shelf is selected.
- Local apps default to Explore and can receive a node-local assignment without entering the global catalog.
- User preferences never mutate global, tenant, or node state.
- Existing SSO, account sessions, and local-storage behavior remain usable during migration.

---

### Task 1: Define the effective-resolution model and precedence rules

**Files:**
- Create: `apps/Nexus-Cloud/src/catalog/resolution.ts`
- Create: `apps/Nexus-Cloud/src/catalog/resolution.test.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/catalog.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/index.ts`

**Interfaces:**
- `CatalogResolutionContext` contains `tenantEntitlements`, `nodeOverlay`, `runtime`, and `userPreferences`.
- `EffectiveCatalogEntry` contains the app identity, final shelf assignment, `availability: "available" | "disabled" | "offline" | "restricted"`, and `canLaunch: boolean`.
- `resolveEffectiveCatalog(globalEntries, context): EffectiveCatalogEntry[]` applies the approved-catalog → tenant → node → runtime/access → user-preference order.

- [ ] **Step 1: Write failing resolver tests.** Cover a globally approved app disabled by tenant entitlement, re-enabled by neither node nor user preference, a node-local app appearing only in that node’s Explore list, offline health preserving `canLaunch: false`, user pinning changing order only, and shelf selection never changing `canLaunch`.

```ts
test("a preferred shelf cannot grant access", () => {
  const [entry] = resolveEffectiveCatalog([approved("nexus-vault", "operate")], {
    runtime: { "nexus-vault": { health: "healthy", canLaunch: false } },
    userPreferences: { preferredShelf: "operate", pinnedIds: [], recentIds: [] },
  });
  expect(entry).toMatchObject({ primaryShelf: "operate", canLaunch: false });
});
```

- [ ] **Step 2: Run the focused resolver test to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/resolution.test.ts`

Expected: FAIL because the catalog resolver module does not exist.

- [ ] **Step 3: Implement the pure resolver with explicit default layers.** Use empty tenant/node/user layers when omitted; apply node shelf overrides only to the node’s returned view; keep global `ApprovedCatalog` immutable.

- [ ] **Step 4: Run the resolver suite and Cloud type-check.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/resolution.test.ts && bun run typecheck`

Expected: all precedence tests pass.

- [ ] **Step 5: Commit the resolution model.**

```bash
git add apps/Nexus-Cloud/src/catalog/resolution.ts apps/Nexus-Cloud/src/catalog/resolution.test.ts apps/Nexus-Cloud/src/systems-api/catalog.ts apps/Nexus-Cloud/src/systems-api/index.ts
git commit -m "feat(cloud): add scoped catalog resolution"
```

### Task 2: Add tenant entitlements as a separate availability layer

**Files:**
- Create: `apps/Nexus-Cloud/src/catalog/tenant-entitlements.ts`
- Create: `apps/Nexus-Cloud/src/catalog/tenant-entitlements.test.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/resolution.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/resolution.test.ts`
- Modify: `apps/Nexus-Cloud/src/api/dto.ts`
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts`
- Modify: `apps/Nexus-Cloud/src/api/routes.ts`

**Interfaces:**
- `TenantCatalogEntitlements` stores `{ tenantId, enabledAppIds, disabledAppIds, defaults, updatedAt }`.
- `resolveTenantEntitlements(tenantId)` returns an immutable snapshot; missing tenant state means all globally approved apps remain eligible for tenant policy filtering.
- Tenant endpoints are scoped under `/api/v1/tenants/:tenantId/catalog` and cannot mutate `/api/v1/catalog/:toolId` approval records.

- [ ] **Step 1: Write failing entitlement tests.** Cover enable/disable precedence, tenant isolation, unknown app ids, and `403` for a request attempting to submit global approval through tenant routes.

- [ ] **Step 2: Run the focused tests to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/tenant-entitlements.test.ts`

Expected: FAIL because tenant catalog state and routes do not exist.

- [ ] **Step 3: Implement a tenant-scoped JSON store and pure entitlement filter.** Use the existing atomic JSON store helper; key records by tenant id and never write into the global catalog map.

- [ ] **Step 4: Add tenant read/update handlers behind `tenant:apps:manage`.** Until tenant membership is backed by a full tenant service, tests use a scoped test principal; production handlers must reject a platform-less or node-only actor.

- [ ] **Step 5: Run Cloud tenant tests and the complete Cloud suite.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/tenant-entitlements.test.ts src/catalog/resolution.test.ts && bun test src`

Expected: tenant policy tests pass and global catalog tests remain green.

- [ ] **Step 6: Commit tenant availability.**

```bash
git add apps/Nexus-Cloud/src/catalog/tenant-entitlements.ts apps/Nexus-Cloud/src/catalog/tenant-entitlements.test.ts apps/Nexus-Cloud/src/catalog/resolution.ts apps/Nexus-Cloud/src/catalog/resolution.test.ts apps/Nexus-Cloud/src/api/dto.ts apps/Nexus-Cloud/src/api/handlers.ts apps/Nexus-Cloud/src/api/routes.ts
git commit -m "feat(cloud): add tenant catalog entitlements"
```

### Task 3: Add node-local overlays and federated disconnected behavior

**Files:**
- Create: `apps/Nexus-Cloud/src/catalog/node-overlay.ts`
- Create: `apps/Nexus-Cloud/src/catalog/node-overlay.test.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/resolution.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/resolution.test.ts`
- Modify: `apps/Nexus-Cloud/src/api/dto.ts`
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts`
- Modify: `apps/Nexus-Cloud/src/api/routes.ts`
- Modify: `apps/Nexus-Cloud/src/federation/service.ts` (only for catalog snapshot transport)

**Interfaces:**
- `NodeCatalogOverlay` stores `{ nodeId, disabledAppIds, shelfOverrides, localApps, defaults, connectedAt, updatedAt }`.
- `upsertNodeCatalogOverlay(nodeId, patch, actor)` changes only node-local state and records the node id in audit metadata.
- `mergeNodeCatalog(globalCatalog, overlay)` adds local apps as `reviewStatus: "pending"`, `primaryShelf: "explore"` unless a node-local assignment exists, and marks them `scope: "node"`.

- [ ] **Step 1: Write failing node-overlay tests.** Cover local apps remaining absent from global results, local shelf assignments not mutating global approval, disabled global apps disappearing only for one node, and a disconnected node serving its last approved snapshot without inventing new global approvals.

- [ ] **Step 2: Run the focused tests to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/node-overlay.test.ts`

Expected: FAIL because node overlay storage and merge behavior do not exist.

- [ ] **Step 3: Implement node overlay persistence and merge logic.** Reuse the JSON store recovery path, bound local app metadata to safe path/delivery values, and reject a local record that claims `scope: "platform"`.

- [ ] **Step 4: Add node-scoped routes protected by `node:apps:manage`.** Approval routes must continue to require `platform:catalog:approve`; the handlers must return `403` when a node actor targets the global approval route.

- [ ] **Step 5: Add the federation snapshot adapter.** Serialize only the approved global projection plus node overlay metadata required for disconnected operation; never send platform audit secrets or tenant-private data to an untrusted peer.

- [ ] **Step 6: Run node, federation, and Cloud regression tests.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/node-overlay.test.ts src/catalog/resolution.test.ts src/test/trust.lifecycle.test.ts && bun run check`

Expected: local overlay tests and existing federation/trust tests pass.

- [ ] **Step 7: Commit node-local catalog behavior.**

```bash
git add apps/Nexus-Cloud/src/catalog/node-overlay.ts apps/Nexus-Cloud/src/catalog/node-overlay.test.ts apps/Nexus-Cloud/src/catalog/resolution.ts apps/Nexus-Cloud/src/catalog/resolution.test.ts apps/Nexus-Cloud/src/api/dto.ts apps/Nexus-Cloud/src/api/handlers.ts apps/Nexus-Cloud/src/api/routes.ts apps/Nexus-Cloud/src/federation/service.ts
git commit -m "feat(cloud): add node-local catalog overlays"
```

### Task 4: Add account-backed user catalog preferences

**Files:**
- Create: `apps/Nexus-Auth/src/preferences.ts`
- Create: `apps/Nexus-Auth/tests/preferences.test.ts`
- Modify: `apps/Nexus-Auth/src/server.ts`
- Modify: `apps/Nexus-Auth/src/types.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/useCatalogPreferences.ts`
- Test: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/useCatalogPreferences.test.ts`

**Interfaces:**
- `CatalogPreferences` is `{ pinnedIds: string[]; recentIds: string[]; preferredShelf: CatalogShelf }` with max eight recents.
- Auth exposes `GET /api/v1/auth/preferences/catalog` and `PUT /api/v1/auth/preferences/catalog`; both are self-scoped and require a valid session, while the update validates shelf ids and string-array limits.
- Dashboard API exports `getCatalogPreferences()` and `saveCatalogPreferences(preferences)`; the hook reads account preferences first and falls back to existing local keys on `401`, network failure, or an older Auth server.

- [ ] **Step 1: Write failing preference tests.** Cover default Everyday, validation of unknown shelves, pin/recent deduplication, user isolation, persistence across Auth reload, and Dashboard fallback to local storage.

- [ ] **Step 2: Run Auth and Dashboard focused tests to verify failure.**

Run: `cd apps/Nexus-Auth && bun test tests/preferences.test.ts && cd ../Nexus-Dashboard/frontend && bun test src/shell/__tests__/useCatalogPreferences.test.ts`

Expected: FAIL because the account preference store and hook do not exist.

- [ ] **Step 3: Implement Auth preference persistence and self-scoped routes.** Store preferences under the user id in the existing Auth data directory, use atomic writes, and never accept a user id from the request body or query string.

- [ ] **Step 4: Implement the Dashboard preference hook and migration.** On successful account read, preserve local pins/recents only when the account record is empty; write subsequent changes to the account endpoint and local storage together until migration is complete.

- [ ] **Step 5: Run focused tests and package gates.**

Run: `cd apps/Nexus-Auth && bun run check && bun test tests/preferences.test.ts && cd ../Nexus-Dashboard/frontend && bunx tsc --noEmit && bun test src/shell/__tests__/useCatalogPreferences.test.ts src/shell/__tests__/AppsDrawer.test.tsx`

Expected: account-backed and fallback paths both pass.

- [ ] **Step 6: Commit user preferences.**

```bash
git add apps/Nexus-Auth/src/preferences.ts apps/Nexus-Auth/tests/preferences.test.ts apps/Nexus-Auth/src/server.ts apps/Nexus-Auth/src/types.ts apps/Nexus-Dashboard/frontend/src/api.ts apps/Nexus-Dashboard/frontend/src/shell/useCatalogPreferences.ts apps/Nexus-Dashboard/frontend/src/shell/__tests__/useCatalogPreferences.test.ts
git commit -m "feat: persist catalog preferences per account"
```

### Task 5: Verify scope boundaries and migration behavior end to end

**Files:**
- Create: `apps/Nexus-Cloud/src/catalog/effective-catalog.integration.test.ts`
- Modify: `apps/Nexus-Dashboard/tests/server.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/AppsDrawer.test.tsx`
- Modify: `apps/Nexus-Auth/tests/routes.test.ts`

- [ ] **Step 1: Add an integration fixture with one approved global app, one tenant-disabled app, one node-local app, one offline app, and two users with different preferences.**

- [ ] **Step 2: Assert the effective output for every scope.** A platform approval is visible to eligible tenants; tenant disablement affects only that tenant; node local apps stay node-scoped; user pins reorder only one user; no shelf or pin changes `canLaunch`.

- [ ] **Step 3: Run all scoped-resolution and Dashboard preference tests.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/effective-catalog.integration.test.ts src/catalog/*.test.ts && cd ../Nexus-Auth && bun test tests/preferences.test.ts tests/routes.test.ts && cd ../Nexus-Dashboard && bun test tests/server.test.ts && cd frontend && bun test src/pages/Home.test.tsx src/shell/__tests__/AppsDrawer.test.tsx`

Expected: every scope boundary is asserted and no existing SSO or app-list behavior regresses.

- [ ] **Step 4: Run `git diff --check` and commit the end-to-end evidence.**

```bash
git add apps/Nexus-Cloud/src/catalog/effective-catalog.integration.test.ts apps/Nexus-Dashboard/tests/server.test.ts apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx apps/Nexus-Dashboard/frontend/src/shell/__tests__/AppsDrawer.test.tsx apps/Nexus-Auth/tests/routes.test.ts
git commit -m "test: verify catalog scope precedence"
```
