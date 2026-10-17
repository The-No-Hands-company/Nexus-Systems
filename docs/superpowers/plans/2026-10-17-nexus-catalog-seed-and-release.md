# Nexus Catalog Seed and Release Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to execute this plan task by task with verification after each task.

**Goal:** Seed the first approved shelves for the current core apps and prove the Cloud-to-Dashboard catalog flow without leaving unclassified apps, broken routes, or silent access regressions.

**Architecture:** Seed data is a versioned Cloud migration applied idempotently by tool id. Existing runtime registry records remain the source of truth for health and delivery. Release tests exercise proposal, approval, Dashboard projection, shelf grouping, offline visibility, and old-record Explore fallback in disposable stores.

**Tech Stack:** Bun, strict TypeScript, Cloud JSON store harness, Dashboard Bun/Vitest tests, production builds.

**Spec:** `docs/superpowers/specs/2026-10-17-nexus-app-catalog-and-shelves-design.md`

## Global Constraints

- Seed only records that have a verified current app id and route; unknown ids remain Explore.
- Seed data must be idempotent and must never overwrite a newer approved record or an edited proposal.
- Existing app URLs, delivery modes, health checks, and SSO behavior must remain unchanged.
- Pending, deferred, rejected, offline, and restricted states must be visible in tests and in the catalog UI.
- Release verification must use disposable data paths and clean them up on success or failure.

---

### Task 1: Create the versioned core-app seed map

**Files:**
- Create: `apps/Nexus-Cloud/src/catalog/seed.ts`
- Create: `apps/Nexus-Cloud/src/catalog/seed.test.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/registry.ts`
- Modify: `apps/Nexus-Cloud/src/index.ts`

**Interfaces:**
- `CATALOG_SEED_VERSION` is a positive integer stored with the catalog state.
- `CORE_CATALOG_SEED` maps current ids to `{ primaryShelf, secondaryShelves, audiences, capabilities, maturity, visibility }`.
- `applyCoreCatalogSeed(registry): { applied: string[]; skipped: string[] }` applies only when no newer approved record exists for an id.

- [ ] **Step 1: Write failing seed tests.** Cover the exact first-wave assignments for Mail, Calendar, Chat, Project, Design, Graphic, Photos, Video, Draw, Forge, Deploy, Terminal, Cloud, Auth, Network, and Vault; verify unknown ids are skipped; verify rerunning the same version is a no-op.

```ts
test("seeds Everyday core apps with stable visibility", () => {
  const result = applyCoreCatalogSeed(registryWithTools(["nexus-email", "nexus-calendar"]));
  expect(result.applied).toEqual(["nexus-calendar", "nexus-email"]);
  expect(readApproved("nexus-email")).toMatchObject({ primaryShelf: "everyday", maturity: "stable" });
});
```

- [ ] **Step 2: Run the focused seed suite to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/seed.test.ts`

Expected: FAIL because no seed map or seed application function exists.

- [ ] **Step 3: Implement the explicit seed map and idempotent application.** Use the runtime tool set as the allowlist; preserve any approved record with a higher version; record the applied seed version separately from proposal versions.

- [ ] **Step 4: Call the seed migration at Cloud startup after registry load and before serving requests.** The migration must write only when it changes state and must be safe when the registry file is read-only or unavailable.

- [ ] **Step 5: Run seed tests and Cloud type-check.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/seed.test.ts && bun run typecheck`

Expected: seed assignments, idempotence, and old-record behavior pass.

- [ ] **Step 6: Commit the seed migration.**

```bash
git add apps/Nexus-Cloud/src/catalog/seed.ts apps/Nexus-Cloud/src/catalog/seed.test.ts apps/Nexus-Cloud/src/systems-api/registry.ts apps/Nexus-Cloud/src/index.ts
git commit -m "feat(cloud): seed core app catalog shelves"
```

### Task 2: Add Cloud-to-Dashboard catalog contract fixtures

**Files:**
- Create: `apps/Nexus-Cloud/src/catalog/release-fixture.ts`
- Create: `apps/Nexus-Cloud/src/catalog/release-fixture.test.ts`
- Modify: `apps/Nexus-Dashboard/tests/apps.test.ts`
- Modify: `apps/Nexus-Dashboard/tests/server.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Grid.test.tsx`

**Interfaces:**
- `buildCatalogReleaseFixture()` returns a disposable Cloud response with approved Everyday/Create/Develop/Operate entries, pending Explore entry, offline entry, and an old record with no metadata.
- Dashboard tests consume the fixture through the same `/ipa/apps` wire shape used in production.

- [ ] **Step 1: Write failing fixture tests.** Assert every controlled field is represented, all route paths remain unchanged, and old entries normalize to Explore.

- [ ] **Step 2: Run the focused fixture tests to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/release-fixture.test.ts && cd ../Nexus-Dashboard/frontend && bun test src/pages/Grid.test.tsx`

Expected: FAIL because the release fixture and catalog-aware test assertions do not exist.

- [ ] **Step 3: Implement the fixture and use it in server/frontend tests.** Keep it in test-only code; it must never become runtime seed data.

- [ ] **Step 4: Run Cloud and Dashboard focused tests.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/release-fixture.test.ts && cd ../Nexus-Dashboard && bun test tests/apps.test.ts tests/server.test.ts && cd frontend && bun test src/pages/Grid.test.tsx`

Expected: projection and UI tests pass against one shared contract fixture.

- [ ] **Step 5: Commit the release fixture.**

```bash
git add apps/Nexus-Cloud/src/catalog/release-fixture.ts apps/Nexus-Cloud/src/catalog/release-fixture.test.ts apps/Nexus-Dashboard/tests/apps.test.ts apps/Nexus-Dashboard/tests/server.test.ts apps/Nexus-Dashboard/frontend/src/pages/Grid.test.tsx
git commit -m "test: add catalog release contract fixture"
```

### Task 3: Exercise the disposable end-to-end catalog release

**Files:**
- Create: `apps/Nexus-Cloud/src/catalog/release.integration.test.ts`
- Create: `apps/Nexus-Dashboard/tests/catalog-release.test.ts`
- Modify: `apps/Nexus-Cloud/src/test/systems-api-harness.ts`

- [ ] **Step 1: Write the integration test harness.** Create temporary Cloud registry, audit, and Auth data paths; register a proposal; approve it with a platform-scoped test principal; request the normalized catalog; pass that response through Dashboard normalization; and remove temporary directories in a `finally` block.

- [ ] **Step 2: Add the failure-path assertions.** Verify Cloud unavailable returns shell-native entries, a `401` approval cannot publish, a non-platform role receives `403`, offline apps remain searchable but not launchable, and a pending app appears under Explore.

- [ ] **Step 3: Run the integration tests to verify failure.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/release.integration.test.ts && cd ../Nexus-Dashboard && bun test tests/catalog-release.test.ts`

Expected: FAIL until the fixture, approval, projection, and fallback paths are wired together.

- [ ] **Step 4: Implement the disposable harness and assertions.** Do not use production data, network services, or a fixed port. Restore environment variables and current working directory in cleanup.

- [ ] **Step 5: Run the end-to-end catalog tests serially.**

Run: `cd apps/Nexus-Cloud && bun test src/catalog/release.integration.test.ts && cd ../Nexus-Dashboard && bun test tests/catalog-release.test.ts`

Expected: all release-path assertions pass with no leftover temporary files.

- [ ] **Step 6: Commit the end-to-end release proof.**

```bash
git add apps/Nexus-Cloud/src/catalog/release.integration.test.ts apps/Nexus-Dashboard/tests/catalog-release.test.ts apps/Nexus-Cloud/src/test/systems-api-harness.ts
git commit -m "test: prove Cloud to Dashboard catalog release"
```

### Task 4: Run the complete product verification matrix

**Files:**
- Modify: `docs/superpowers/specs/2026-10-17-nexus-app-catalog-and-shelves-design.md` only if the verified contract needs a factual correction
- Create: `docs/superpowers/reports/2026-10-17-nexus-catalog-verification.md`

- [ ] **Step 1: Run Cloud and Auth gates.**

Run: `cd apps/Nexus-Auth && bun run check && bun test && cd ../Nexus-Cloud && bun run check && bun test src`

- [ ] **Step 2: Run Dashboard server and frontend gates.**

Run: `cd apps/Nexus-Dashboard && bun run check && bun test tests/ && cd frontend && bun test && bunx tsc --noEmit && bun run build`

- [ ] **Step 3: Run the existing regression suites listed in the user’s verification matrix.**

Run the established package-local commands for Design, Graphic, Photos, Video, Draw, Chat, Auth, Calendar, and Project. Record expected warnings separately from assertion failures; do not suppress a new failure under a pre-existing warning.

- [ ] **Step 4: Perform a source and route audit.** Confirm every app has exactly one catalog identity, every approved entry has a valid shelf, pending entries are Explore, no node/tenant route can call global approval, every interactive shelf control has a focus-visible state, and no app path or delivery mode changed.

- [ ] **Step 5: Write the verification report.** Include commands, pass counts, build outputs, known expected warnings, commit ids, and the final `HEAD`/`origin/main` equality.

- [ ] **Step 6: Run `git diff --check`, inspect unrelated working-tree changes, and commit the report.**

```bash
git diff --check
git add docs/superpowers/reports/2026-10-17-nexus-catalog-verification.md
git commit -m "test: record Nexus catalog verification matrix"
```
