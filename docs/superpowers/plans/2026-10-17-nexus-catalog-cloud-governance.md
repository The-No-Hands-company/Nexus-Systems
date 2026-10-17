# Nexus Cloud Catalog Governance Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to execute this plan task by task with verification after each task.

**Goal:** Add a durable, auditable Cloud catalog projection where app registrations submit metadata proposals and only a platform-scoped catalog permission can publish approved shelf placement.

**Architecture:** Keep runtime tool identity, health, delivery, and routing in the existing Systems API registry. Add a separate catalog projection keyed by tool id, with the latest proposal and last approved record stored together. Registration remains self-service; approval is a Cloud control-plane mutation that resolves the caller through Nexus-Auth and records a durable audit event.

**Tech Stack:** Bun, strict TypeScript, JSON atomic persistence, Nexus-Auth session/API-key validation, `bun:test`, Biome.

**Spec:** `docs/superpowers/specs/2026-10-17-nexus-app-catalog-and-shelves-design.md`

## Global Constraints

- Keep catalog metadata separate from authorization: shelf placement never grants launch access.
- Use the controlled values `everyday | work | create | develop | operate | personal | explore` for shelves.
- Use the controlled audiences `general | business | creator | developer | operations | power-user`.
- Preserve registration, heartbeat, delivery, health, route, and SSO behavior for records without catalog metadata.
- Treat an unreviewed or deferred proposal as `reviewStatus: "pending" | "deferred"` with Explore fallback; never replace the previous approved record until approval succeeds.
- Restrict global approval mutations to the scope-qualified `platform:catalog:approve` permission; a node or tenant credential must receive `403`.
- Persist audit entries with the platform-scoped actor, timestamp, previous value, and new value.

---

### Task 1: Add the catalog contract and pure validation module

**Files:**
- Create: `apps/Nexus-Cloud/src/systems-api/catalog.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/index.ts`
- Modify: `apps/Nexus-Cloud/src/api/dto.ts`
- Test: `apps/Nexus-Cloud/src/systems-api/catalog.test.ts`

**Interfaces:**
- Produces `CatalogShelf`, `CatalogAudience`, `CatalogMaturity`, `CatalogVisibility`, `CatalogReviewStatus`, `CatalogProposal`, `ApprovedCatalog`, and `CatalogRecord`.
- Produces `validateCatalogProposal(value: unknown): CatalogProposal | null` and `validateApprovedCatalog(value: unknown): ApprovedCatalog | null`.
- Produces `exploreFallbackCatalog(reviewStatus: CatalogReviewStatus): ApprovedCatalog` for records without approved metadata.

- [ ] **Step 1: Write failing validation tests.** Cover every allowed shelf/audience/maturity/visibility value, duplicate secondary shelves, a primary shelf repeated in secondary shelves, missing required proposal fields, unknown enum values, and a valid proposal preserving a version number.

```ts
test("accepts a proposal with controlled values", () => {
  expect(validateCatalogProposal({
    suggestedShelves: ["everyday"],
    suggestedAudiences: ["general"],
    capabilities: ["messaging"],
    keywords: ["chat"],
    submittedAt: "2026-10-17T00:00:00.000Z",
    version: 1,
  })).toEqual(expect.objectContaining({ suggestedShelves: ["everyday"] }));
});
```

- [ ] **Step 2: Run the focused test and verify it fails because the module is missing.**

Run: `cd apps/Nexus-Cloud && bun test src/systems-api/catalog.test.ts`

Expected: FAIL with a module or export error for `./catalog`.

- [ ] **Step 3: Implement the immutable controlled-value arrays, types, and validators.** Reject empty shelf arrays, unknown identifiers, duplicate shelves, duplicate audiences, non-finite versions, and malformed timestamps. Keep capabilities and keywords string arrays with trimmed non-empty values.

```ts
export const CATALOG_SHELVES = ["everyday", "work", "create", "develop", "operate", "personal", "explore"] as const;
export type CatalogShelf = (typeof CATALOG_SHELVES)[number];
export const CATALOG_AUDIENCES = ["general", "business", "creator", "developer", "operations", "power-user"] as const;
export type CatalogAudience = (typeof CATALOG_AUDIENCES)[number];
```

- [ ] **Step 4: Run the focused test and the Cloud type-check.**

Run: `cd apps/Nexus-Cloud && bun test src/systems-api/catalog.test.ts && bun run typecheck`

Expected: all catalog tests pass and TypeScript reports no errors.

- [ ] **Step 5: Commit the contract.**

```bash
git add apps/Nexus-Cloud/src/systems-api/catalog.ts apps/Nexus-Cloud/src/systems-api/index.ts apps/Nexus-Cloud/src/api/dto.ts apps/Nexus-Cloud/src/systems-api/catalog.test.ts
git commit -m "feat(cloud): add catalog metadata contract"
```

### Task 2: Persist proposals and approved records without breaking old registries

**Files:**
- Modify: `apps/Nexus-Cloud/src/systems-api/store.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/registry.ts`
- Modify: `apps/Nexus-Cloud/src/test/systems-api-harness.ts`
- Test: `apps/Nexus-Cloud/src/systems-api/store.test.ts`
- Test: `apps/Nexus-Cloud/src/systems-api/registry.test.ts` (create if absent)

**Interfaces:**
- Produces `SystemsApiCatalogState` with `proposals` and `approved` maps keyed by tool id.
- Extends `SystemsApiRegistryData` with a backward-compatible `catalog` field.
- Produces `getCatalogRecord(toolId)`, `saveCatalogProposal(toolId, proposal)`, and `saveApprovedCatalog(toolId, approved)` through the registry service boundary.

- [ ] **Step 1: Add failing persistence tests for old and new snapshots.** Load a snapshot without `catalog` and assert empty maps; write/read a proposal and approved record and assert every field survives; verify a corrupt catalog entry is discarded while valid tools still load.

- [ ] **Step 2: Run the focused persistence tests to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/systems-api/store.test.ts src/systems-api/registry.test.ts`

Expected: FAIL because `SystemsApiRegistryData.catalog` and catalog functions do not exist.

- [ ] **Step 3: Extend the store sanitizer and registry snapshot cloning.** Default `catalog` to `{ proposals: {}, approved: {} }`; validate each stored record through the pure catalog validators; preserve the existing atomic JSON write and recovery behavior.

```ts
export type SystemsApiCatalogState = {
  proposals: Record<string, CatalogProposal>;
  approved: Record<string, ApprovedCatalog>;
};

export type SystemsApiRegistryData = {
  tools: SystemsApiTool[];
  publicUrls: SystemsApiPublicUrl[];
  addresses: SystemsApiAddress[];
  history: SystemsApiToolHistoryEntry[];
  exposures: SystemsApiExposureRecord[];
  domains: SystemsApiDomainBinding[];
  catalog: SystemsApiCatalogState;
};
```

- [ ] **Step 4: Update all test harness fixtures and reset helpers to include the empty catalog state.** Existing callers that construct registry snapshots must remain source-compatible by using an exported `emptySystemsApiCatalogState` constant.

- [ ] **Step 5: Run Cloud registry and persistence tests plus the full Cloud check.**

Run: `cd apps/Nexus-Cloud && bun test src/systems-api/store.test.ts src/systems-api/registry.test.ts && bun run check`

Expected: focused tests and the existing Cloud check pass.

- [ ] **Step 6: Commit the persistence boundary.**

```bash
git add apps/Nexus-Cloud/src/systems-api/store.ts apps/Nexus-Cloud/src/systems-api/registry.ts apps/Nexus-Cloud/src/test/systems-api-harness.ts apps/Nexus-Cloud/src/systems-api/store.test.ts apps/Nexus-Cloud/src/systems-api/registry.test.ts
git commit -m "feat(cloud): persist catalog proposal and approval state"
```

### Task 3: Accept catalog proposals during tool registration and patching

**Files:**
- Modify: `apps/Nexus-Cloud/src/systems-api/registry.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/service.ts`
- Modify: `apps/Nexus-Cloud/src/api/dto.ts`
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts`
- Modify: `apps/Nexus-Cloud/src/api/routes.ts`
- Test: `apps/Nexus-Cloud/src/api/catalog.routes.test.ts`

**Interfaces:**
- Extends `SystemsApiToolRegistrationInput` and `SystemsApiToolPatchInput` with `catalogProposal?: CatalogProposal`.
- Produces `GET /api/v1/catalog/:toolId` for trusted catalog consumers.
- Produces `POST /api/v1/tools/:toolId/catalog-proposal` for an explicit proposal update, while registration remains compatible with embedded proposals.
- Produces response shape `{ catalog: CatalogRecord }` and `{ tools, catalog }` only where the caller is already authorized to read the tool registry.

- [ ] **Step 1: Write failing route tests.** Assert a valid registration proposal is stored as pending, a malformed proposal returns `400`, a proposal update does not alter an existing approved record, and a tool without metadata returns an Explore fallback.

```ts
test("registration stores a pending proposal without self-approval", async () => {
  const response = await handleRequest(new Request("http://localhost/api/v1/tools", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({
      id: "nexus-chat",
      name: "Nexus Chat",
      description: "Messaging",
      catalogProposal: {
        suggestedShelves: ["everyday"], suggestedAudiences: ["general"],
        capabilities: ["messaging"], keywords: ["chat"],
        submittedAt: "2026-10-17T00:00:00.000Z", version: 1,
      },
    }),
  }));
  expect(response.status).toBe(201);
  expect((await response.json()).catalog.reviewStatus).toBe("pending");
});
```

- [ ] **Step 2: Run the new route tests to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/api/catalog.routes.test.ts`

Expected: FAIL because the request validator, handler, and catalog response do not exist.

- [ ] **Step 3: Add DTO validation and registry/service methods.** Registration stores only the proposal; it creates no approved record. Patch and heartbeat paths must not clear catalog state when the catalog field is omitted.

- [ ] **Step 4: Add the read and proposal routes to the manifest and dispatcher.** Keep proposal mutations behind the existing API-key gate for service registrations, and return `404` for unknown tool ids.

- [ ] **Step 5: Run route tests, manifest tests, and the full Cloud suite.**

Run: `cd apps/Nexus-Cloud && bun test src/api/catalog.routes.test.ts src/api/routes.manifest.test.ts && bun test src`

Expected: all existing route behavior remains green and proposal tests pass.

- [ ] **Step 6: Commit proposal ingestion.**

```bash
git add apps/Nexus-Cloud/src/systems-api/registry.ts apps/Nexus-Cloud/src/systems-api/service.ts apps/Nexus-Cloud/src/api/dto.ts apps/Nexus-Cloud/src/api/handlers.ts apps/Nexus-Cloud/src/api/routes.ts apps/Nexus-Cloud/src/api/catalog.routes.test.ts
git commit -m "feat(cloud): accept app catalog proposals"
```

### Task 4: Add scoped catalog approval authorization and audited mutations

**Files:**
- Modify: `apps/Nexus-Auth/src/types.ts`
- Modify: `apps/Nexus-Auth/src/users.ts`
- Modify: `apps/Nexus-Auth/src/server.ts`
- Modify: `apps/Nexus-Auth/src/types.test.ts` (create if absent)
- Modify: `apps/Nexus-Cloud/src/nexus-auth.ts`
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts`
- Modify: `apps/Nexus-Cloud/src/api/routes.ts`
- Test: `apps/Nexus-Auth/tests/permissions.test.ts`
- Test: `apps/Nexus-Cloud/src/api/catalog.routes.test.ts`

**Interfaces:**
- Adds `platform:catalog:approve` to Auth’s `Permission` union and grants it to `founder` and `admin` only during this migration.
- Extends `GET /api/v1/auth/check` with `permissions: string[]`, derived server-side from the authenticated user or API-key owner; never trusts a request header.
- Extends `NexusAuthUser` with `permissions?: readonly string[]` and adds `checkSessionPermission(credential, permission)` in `apps/Nexus-Cloud/src/nexus-auth.ts`.
- Produces `POST /api/v1/catalog/:toolId/approve`, `POST /api/v1/catalog/:toolId/defer`, `POST /api/v1/catalog/:toolId/reject`, and `POST /api/v1/catalog/:toolId/rollback`.

- [ ] **Step 1: Write Auth permission tests.** Founder and admin must have `platform:catalog:approve`; operator, user, and viewer must not. `/auth/check` must return permissions for a valid session and must omit them for an unauthorized request.

- [ ] **Step 2: Run the Auth permission tests to verify failure.**

Run: `cd apps/Nexus-Auth && bun test tests/permissions.test.ts`

Expected: FAIL because the permission identifier and check response field do not exist.

- [ ] **Step 3: Implement the permission union, role map, and check response.** Keep the existing `operator` identity role for compatibility; do not use it as the catalog authority. The new permission is the authority boundary.

- [ ] **Step 4: Write Cloud approval route tests.** Cover unauthenticated `401`, authenticated user without permission `403`, founder approval `200`, proposal-to-approved field mapping, audit event contents, deferred/rejected review status, and rollback restoring the previous approved record.

- [ ] **Step 5: Implement Cloud permission checks and mutations.** Resolve the actor from the session/API key through Auth, require `platform:catalog:approve`, write the approved record with `approvedBy` equal to the Auth user id, and call `recordEvent` with `eventType: "catalog-approval"`, action, actor, previous value, and new value.

- [ ] **Step 6: Run Auth and Cloud focused tests, then the full package gates.**

Run: `cd apps/Nexus-Auth && bun test tests/permissions.test.ts && cd ../Nexus-Cloud && bun run check && bun test src`

Expected: both packages pass; a non-platform role cannot publish catalog metadata.

- [ ] **Step 7: Commit scoped approval.**

```bash
git add apps/Nexus-Auth/src/types.ts apps/Nexus-Auth/src/users.ts apps/Nexus-Auth/src/server.ts apps/Nexus-Auth/tests/permissions.test.ts apps/Nexus-Cloud/src/nexus-auth.ts apps/Nexus-Cloud/src/api/handlers.ts apps/Nexus-Cloud/src/api/routes.ts apps/Nexus-Cloud/src/api/catalog.routes.test.ts
git commit -m "feat: add scoped catalog approval permission"
```

### Task 5: Expose a normalized catalog projection for Dashboard consumers

**Files:**
- Modify: `apps/Nexus-Cloud/src/api/dto.ts`
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts`
- Modify: `apps/Nexus-Cloud/src/api/routes.ts`
- Modify: `apps/Nexus-Dashboard/src/apps.ts`
- Modify: `apps/Nexus-Dashboard/src/server.ts`
- Test: `apps/Nexus-Cloud/src/api/catalog.routes.test.ts`
- Test: `apps/Nexus-Dashboard/tests/apps.test.ts`
- Test: `apps/Nexus-Dashboard/tests/server.test.ts`

**Interfaces:**
- Cloud exposes `GET /api/v1/catalog` returning `{ apps: DashboardCatalogEntry[] }`, combining tool runtime fields with approved catalog fields and `reviewStatus`.
- Dashboard `toAppEntries()` accepts the new catalog payload and maps missing metadata to `primaryShelf: "explore"`, empty secondary/audience/capability arrays, `maturity: "experimental"`, `visibility: "discoverable"`, and `reviewStatus: "pending"`.
- Dashboard keeps the existing `GET /ipa/apps` response and route delivery behavior unchanged while adding normalized catalog fields.

- [ ] **Step 1: Add failing projection tests.** Verify approved, pending, offline, restricted, hidden, and old-record entries normalize deterministically; verify hidden entries are excluded from ordinary user results but remain available to platform catalog consumers.

- [ ] **Step 2: Run projection tests to verify failure.**

Run: `cd apps/Nexus-Dashboard && bun test tests/apps.test.ts tests/server.test.ts`

Expected: FAIL because `AppEntry` has no catalog fields and Cloud payloads are still read from `/api/v1/tools` only.

- [ ] **Step 3: Add the Cloud catalog DTO and handler.** Join tool records to catalog records by id, omit private upstream addresses using the existing authentication/redaction rules, and return stable alphabetical output.

- [ ] **Step 4: Update Dashboard fetch and normalization.** Request `/api/v1/catalog` first, fall back to `/api/v1/tools` if the endpoint returns `404`, and preserve shell-native mail/terminal merging and role filtering.

- [ ] **Step 5: Run focused Cloud/Dashboard tests and both package checks.**

Run: `cd apps/Nexus-Cloud && bun test src/api/catalog.routes.test.ts && cd ../Nexus-Dashboard && bun run check && bun test tests/apps.test.ts tests/server.test.ts`

Expected: approved metadata arrives through `/ipa/apps`, old Cloud servers still populate Explore entries, and existing health/delivery tests remain green.

- [ ] **Step 6: Commit the projection.**

```bash
git add apps/Nexus-Cloud/src/api/dto.ts apps/Nexus-Cloud/src/api/handlers.ts apps/Nexus-Cloud/src/api/routes.ts apps/Nexus-Cloud/src/api/catalog.routes.test.ts apps/Nexus-Dashboard/src/apps.ts apps/Nexus-Dashboard/src/server.ts apps/Nexus-Dashboard/tests/apps.test.ts apps/Nexus-Dashboard/tests/server.test.ts
git commit -m "feat: expose normalized app catalog projection"
```

### Task 6: Run the Cloud governance release gate

**Files:**
- Modify: `apps/Nexus-Cloud/docs/cloud-contract.md`
- Modify: `apps/Nexus-Cloud/docs/architecture.md`
- Modify: `apps/Nexus-Dashboard/README.md` (only if the existing file documents `/ipa/apps`)

- [ ] **Step 1: Document the catalog endpoints, controlled enums, permission boundary, Explore fallback, and audit event shape.** State clearly that platform catalog stewards are hosted-platform staff and are separate from self-hosted node administrators.

- [ ] **Step 2: Run the complete Cloud and Auth gates.**

Run: `cd apps/Nexus-Auth && bun run check && bun test && cd ../Nexus-Cloud && bun run check && bun test src`

Expected: all tests pass with no new warnings treated as failures.

- [ ] **Step 3: Run `git diff --check` and inspect only planned files.**

- [ ] **Step 4: Commit documentation and the release gate evidence.**

```bash
git add apps/Nexus-Cloud/docs/cloud-contract.md apps/Nexus-Cloud/docs/architecture.md apps/Nexus-Dashboard/README.md
git commit -m "docs: document Cloud catalog governance"
```
