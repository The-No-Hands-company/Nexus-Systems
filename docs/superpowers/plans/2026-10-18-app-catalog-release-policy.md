# App Catalog Release Policy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints.

**Goal:** Make the Dashboard show only released, stable, healthy user apps while keeping platform services and developer tools in role-appropriate control surfaces.

**Architecture:** Cloud owns explicit catalog `surface` and `releaseState` metadata and validates it at the persistence/API boundary. Dashboard receives the catalog, merges native entries, and applies one fail-closed launcher projection before shelves, search, pins, or public summaries are built. Admin and health views continue to use their broader service inventory.

**Tech Stack:** Bun, TypeScript, Bun test, React, React Router, Dashboard server, Nexus Cloud registry, Git submodules.

**Spec:** `docs/superpowers/specs/2026-10-18-app-catalog-release-policy-design.md`

## Global Constraints

- Normal launcher eligibility requires `surface == "user-app"`, `releaseState == "released"`, `reviewStatus == "approved"`, `maturity == "stable"`, `visibility != "hidden"`, `health == "healthy"`, and a valid launch route or URL.
- Offline released apps remain in authorized health/release inventory but are omitted from normal launcher, search, pins, and signed-out summaries.
- Auth, Network, and Vault are `platform-service`; Cloud is `admin-console`; Terminal, Forge, and Deploy are `developer-tool`.
- Existing registry, health, federation, and admin inventories remain available.
- Unknown or missing catalog metadata fails closed and cannot produce a launcher tile.
- Do not stage, commit, or alter unrelated pre-existing dirty files or submodule changes.

---

### Task 1: Add and validate the Cloud catalog contract

**Files:**
- Modify: `apps/Nexus-Cloud/src/systems-api/catalog.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/catalog.test.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/registry.test.ts`
- Modify: `apps/Nexus-Cloud/src/systems-api/store.test.ts`
- Modify: `apps/Nexus-Cloud/src/api/catalog.routes.test.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/seed.test.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/resolution.test.ts`

**Interfaces:**
- Produce `CatalogSurface`, `CatalogReleaseState`, `CATALOG_SURFACES`, and `CATALOG_RELEASE_STATES` from `systems-api/catalog.ts`.
- Extend `ApprovedCatalog` with required `surface: CatalogSurface` and `releaseState: CatalogReleaseState`.
- `validateApprovedCatalog(value)` returns `null` for missing, unknown, or malformed new fields.

- [ ] **Step 1: Write failing contract tests**

Add to `apps/Nexus-Cloud/src/systems-api/catalog.test.ts`:

Define the test fixture immediately above these cases so the rejected variants are complete objects:

```ts
const validApprovedCatalog = {
  primaryShelf: "everyday" as const,
  secondaryShelves: [],
  audiences: ["general" as const],
  capabilities: ["mail"],
  maturity: "stable" as const,
  visibility: "default" as const,
  surface: "user-app" as const,
  releaseState: "released" as const,
  approvedAt: "2026-10-18T00:00:00.000Z",
  approvedBy: "platform",
  version: 1,
};
```

```ts
it("accepts an explicit user-app release", () => {
  expect(validateApprovedCatalog({
    primaryShelf: "everyday",
    secondaryShelves: [],
    audiences: ["general"],
    capabilities: ["mail"],
    maturity: "stable",
    visibility: "default",
    surface: "user-app",
    releaseState: "released",
    approvedAt: "2026-10-18T00:00:00.000Z",
    approvedBy: "platform",
    version: 1,
  })).toMatchObject({ surface: "user-app", releaseState: "released" });
});

it.each([
  {},
  { surface: "user-app" },
  { releaseState: "released" },
  { surface: "unknown", releaseState: "released" },
  { surface: "user-app", releaseState: "unknown" },
])("rejects incomplete release metadata: %j", (change) => {
  expect(validateApprovedCatalog({ ...validApprovedCatalog, ...change })).toBeNull();
});
```

Update the existing valid approval fixture in that file with both new fields.

- [ ] **Step 2: Run the focused test and confirm it fails**

```bash
cd apps/Nexus-Cloud
bun test src/systems-api/catalog.test.ts
```

Expected: failures because the new fields do not exist or are not validated.

- [ ] **Step 3: Implement the contract**

Add the constants and types, add required fields to `ApprovedCatalog`, and require both fields in `validateApprovedCatalog`. Update every `ApprovedCatalog` literal in the listed test files with explicit metadata. Do not add a permissive default in the validator.

- [ ] **Step 4: Run persistence and route tests**

```bash
bun test src/systems-api/catalog.test.ts src/systems-api/registry.test.ts src/systems-api/store.test.ts src/api/catalog.routes.test.ts src/catalog/seed.test.ts src/catalog/resolution.test.ts
```

Expected: all focused tests pass.

- [ ] **Step 5: Commit the contract**

```bash
git add apps/Nexus-Cloud/src/systems-api/catalog.ts apps/Nexus-Cloud/src/systems-api/catalog.test.ts apps/Nexus-Cloud/src/systems-api/registry.test.ts apps/Nexus-Cloud/src/systems-api/store.test.ts apps/Nexus-Cloud/src/api/catalog.routes.test.ts apps/Nexus-Cloud/src/catalog/seed.test.ts apps/Nexus-Cloud/src/catalog/resolution.test.ts
git commit -m "feat(cloud): add explicit catalog release contract"
```

### Task 2: Classify and seed the first-wave catalog

**Files:**
- Modify: `apps/Nexus-Cloud/src/catalog/seed.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/seed.test.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/release-fixture.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/release-fixture.test.ts`
- Modify: `apps/Nexus-Cloud/src/catalog/release.integration.test.ts`

**Interfaces:**
- `CoreCatalogSeed` includes required `surface` and `releaseState`.
- `CORE_CATALOG_SEED` assigns surfaces according to the approved spec.
- `applyCoreCatalogSeed()` persists both fields and still skips unregistered tools.

- [ ] **Step 1: Write failing seed assertions**

Extend `apps/Nexus-Cloud/src/catalog/seed.test.ts`:

```ts
expect(records["nexus-auth"]).toMatchObject({ surface: "platform-service", releaseState: "released" });
expect(records["nexus-cloud"]).toMatchObject({ surface: "admin-console", releaseState: "released" });
expect(records["nexus-terminal"]).toMatchObject({ surface: "developer-tool", releaseState: "released" });
expect(records["nexus-calendar"]).toMatchObject({ surface: "user-app", releaseState: "staged" });
```

Set the initially released user-app entries from deployed evidence; only apps satisfying the release gates may be marked `released`. Non-integrated user apps remain `staged` even if their source quality gate is green.

- [ ] **Step 2: Run the focused seed suite and confirm it fails**

```bash
cd apps/Nexus-Cloud
bun test src/catalog/seed.test.ts src/catalog/release-fixture.test.ts src/catalog/release.integration.test.ts
```

- [ ] **Step 3: Add classifications and persistence**

Add `surface` and `releaseState` to the seed type and every seed entry. Include both in `saveApprovedCatalog()` and the disposable release fixture. Keep seed versioning intact so newer approvals are not overwritten.

- [ ] **Step 4: Verify effective catalog behavior**

```bash
bun test src/catalog/seed.test.ts src/catalog/release-fixture.test.ts src/catalog/release.integration.test.ts src/catalog/effective-catalog.integration.test.ts
```

Expected: staged user apps remain in Cloud/admin projections but are not launchable after Dashboard projection.

- [ ] **Step 5: Commit the classifications**

```bash
git add apps/Nexus-Cloud/src/catalog/seed.ts apps/Nexus-Cloud/src/catalog/seed.test.ts apps/Nexus-Cloud/src/catalog/release-fixture.ts apps/Nexus-Cloud/src/catalog/release-fixture.test.ts apps/Nexus-Cloud/src/catalog/release.integration.test.ts
git commit -m "feat(cloud): classify catalog surfaces and releases"
```

### Task 3: Enforce the launcher projection in the Dashboard server

**Files:**
- Modify: `apps/Nexus-Dashboard/src/apps.ts`
- Modify: `apps/Nexus-Dashboard/src/server.ts`
- Modify: `apps/Nexus-Dashboard/src/catalog-release.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/catalog.ts`

**Interfaces:**
- `AppEntry` requires `surface` and `releaseState`.
- Add `CatalogReleaseState` to Dashboard API types.
- Add `isReleasedUserApp(entry: AppEntry): boolean` in `apps.ts`.
- `toAppEntries()` drops records without valid explicit surface/release metadata.
- `fetchApps()` returns only released, stable, healthy user-app entries plus validated native entries.

- [ ] **Step 1: Write failing projection tests**

Add to `apps/Nexus-Dashboard/src/catalog-release.test.ts`:

Define this test helper in the same file and use it for each fixture below:

```ts
const releasedHealthyUserApp = (id: string) => ({
  id,
  name: id,
  description: "released user app",
  publicUrl: `https://${id}.tnhc.dev`,
  path: `/${id.replace(/^nexus-/, "")}`,
  delivery: "framed" as const,
  health: "healthy" as const,
  surface: "user-app" as const,
  releaseState: "released" as const,
  maturity: "stable" as const,
  visibility: "default" as const,
  reviewStatus: "approved" as const,
});
```

```ts
it("keeps only released stable healthy user apps", () => {
  const entries = toAppEntries({ apps: [
    releasedHealthyUserApp("nexus-calendar"),
    { ...releasedHealthyUserApp("nexus-cloud"), surface: "admin-console" },
    { ...releasedHealthyUserApp("nexus-auth"), surface: "platform-service" },
    { ...releasedHealthyUserApp("nexus-video"), releaseState: "staged" },
    { ...releasedHealthyUserApp("nexus-draw"), health: "offline" },
  ] }, "auth.tnhc.dev");

  expect(entries.map((entry) => entry.id)).toEqual(["nexus-calendar"]);
});

it("fails closed for legacy records without release metadata", () => {
  expect(toAppEntries({
    apps: [{ id: "legacy", name: "Legacy", publicUrl: "https://legacy.tnhc.dev", health: "healthy" }],
  }, "auth.tnhc.dev")).toEqual([]);
});
```

Add native-entry assertions proving Mail is released user-app metadata and Terminal is released developer-tool metadata. Update the existing Cloud fixture expectation so Cloud is not a launcher entry.

- [ ] **Step 2: Run focused Dashboard tests and confirm they fail**

```bash
cd apps/Nexus-Dashboard
bun test src/catalog-release.test.ts src/apps.test.ts
```

- [ ] **Step 3: Implement the server projection**

Make metadata required in `AppEntry`, validate it in `toAppEntries()`, and filter through `isReleasedUserApp()`. Preserve `mergeApps()` de-duplication and the Cloud-down fallback. Native Mail and role-gated Terminal must have explicit metadata.

- [ ] **Step 4: Run the server suite**

```bash
bun test src/catalog-release.test.ts src/apps.test.ts src/server.test.ts
```

Expected: platform services do not appear in the returned launcher list.

- [ ] **Step 5: Commit the server projection**

```bash
git add apps/Nexus-Dashboard/src/apps.ts apps/Nexus-Dashboard/src/server.ts apps/Nexus-Dashboard/src/catalog-release.test.ts apps/Nexus-Dashboard/frontend/src/api.ts apps/Nexus-Dashboard/frontend/src/shell/catalog.ts
git commit -m "feat(dashboard): project only released user apps"
```

### Task 4: Apply the policy to shelves, search, pins, and role surfaces

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/catalog.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/filterApps.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/AppsDrawer.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Grid.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/widgets/Pinned.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/catalog.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/Launcher.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Grid.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/Shell.test.tsx`

**Interfaces:**
- `isLauncherApp()` is shared by `appsForShelf`, `searchCatalog`, and `orderedLauncherApps`.
- Developer/admin navigation uses explicit role checks and routes; it never relies on a hidden app tile.

- [ ] **Step 1: Write failing frontend tests**

Extend `frontend/src/shell/catalog.test.ts`:

Define this frontend fixture helper in the same file:

```ts
const releasedUserApp = (id: string) => ({
  id,
  name: id,
  description: "released user app",
  path: `/${id}`,
  delivery: "shell-native" as const,
  health: "healthy" as const,
  surface: "user-app" as const,
  releaseState: "released" as const,
  maturity: "stable" as const,
  visibility: "default" as const,
  reviewStatus: "approved" as const,
  primaryShelf: "everyday" as const,
  secondaryShelves: [],
  audiences: ["general" as const],
  capabilities: [],
});
```

```ts
it("excludes platform, developer, staged, and offline entries from user shelves", () => {
  const grouped = groupAppsByShelf([
    releasedUserApp("mail"),
    { ...releasedUserApp("cloud"), surface: "admin-console" },
    { ...releasedUserApp("terminal"), surface: "developer-tool" },
    { ...releasedUserApp("video"), releaseState: "staged" },
    { ...releasedUserApp("draw"), health: "offline" },
  ]);
  expect(grouped.everyday.map((app) => app.id)).toEqual(["mail"]);
});
```

Update drawer/home tests so app counts use only the user-app projection. Add a role-gated navigation assertion for Terminal and Cloud entry points in the authorized shell surface.

- [ ] **Step 2: Run frontend tests and confirm they fail**

```bash
cd apps/Nexus-Dashboard
bun test frontend/src/shell/catalog.test.ts frontend/src/shell/Launcher.test.tsx frontend/src/pages/Grid.test.tsx frontend/src/pages/Home.test.tsx frontend/src/shell/Shell.test.tsx
```

- [ ] **Step 3: Implement the frontend projection**

Filter shelf/search/pin inputs through the same predicate. Remove offline/unavailable entries from the normal drawer input because they no longer enter it. Keep health wording in Admin/Cloud pages. Add explicit authorized links for existing `/terminal` and `/cloud` control surfaces without presenting them as user apps.

- [ ] **Step 4: Run the frontend catalog suite**

```bash
bun test frontend/src/shell/catalog.test.ts frontend/src/shell/Launcher.test.tsx frontend/src/pages/Grid.test.tsx frontend/src/pages/Home.test.tsx frontend/src/shell/Shell.test.tsx
```

Expected: user shelves, search, pins, signed-out summaries, and counts contain only released stable healthy user apps; authorized control links remain available.

- [ ] **Step 5: Commit the frontend projection**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell/catalog.ts apps/Nexus-Dashboard/frontend/src/shell/filterApps.ts apps/Nexus-Dashboard/frontend/src/shell/AppsDrawer.tsx apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx apps/Nexus-Dashboard/frontend/src/pages/Home.tsx apps/Nexus-Dashboard/frontend/src/pages/Grid.tsx apps/Nexus-Dashboard/frontend/src/pages/widgets/Pinned.tsx apps/Nexus-Dashboard/frontend/src/shell/catalog.test.ts apps/Nexus-Dashboard/frontend/src/shell/Launcher.test.tsx apps/Nexus-Dashboard/frontend/src/pages/Grid.test.tsx apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx apps/Nexus-Dashboard/frontend/src/shell/Shell.test.tsx
git commit -m "feat(dashboard): separate app and control surfaces"
```

### Task 5: Run the quality matrix and deploy verification

**Files:**
- Modify only files required by failing quality gates.
- Do not alter unrelated dirty paths or submodule changes.

**Interfaces:**
- Cloud and Dashboard submodule commits are advanced in the parent repository after their own commits are pushed.
- The deployed launcher exposes only released, stable, healthy user apps.

- [ ] **Step 1: Run focused Cloud and Dashboard suites**

```bash
cd apps/Nexus-Cloud
bun test --timeout=15000 --max-concurrency=1 src
cd ../Nexus-Dashboard
bun test src/catalog-release.test.ts src/apps.test.ts src/server.test.ts frontend/src/shell/catalog.test.ts frontend/src/shell/Launcher.test.tsx frontend/src/pages/Grid.test.tsx frontend/src/pages/Home.test.tsx frontend/src/shell/Shell.test.tsx
```

- [ ] **Step 2: Run the established app matrix**

Run the existing quality commands for Design, Graphic, Photos, Video, Draw, Chat, Auth, Calendar, Project, and Dashboard, recording tests, TypeScript checks, and production builds. Treat the existing Draw jsdom canvas warning and expected Cloud registration 401 warnings as known non-failing output only when all assertions pass.

- [ ] **Step 3: Run production and runtime verification**

```bash
bash deploy/production/deploy.sh status
```

Confirm managed services and HTTP health probes are green. Query the deployed authenticated `/ipa/apps` response and verify every returned entry has `surface=user-app`, `releaseState=released`, `maturity=stable`, and `health=healthy`.

- [ ] **Step 4: Advance parent pointers and verify the working tree**

```bash
git diff --check
git status --short
git submodule status
```

Only intended Cloud/Dashboard submodule pointers and catalog policy changes may be staged. Leave unrelated modifications untouched.

- [ ] **Step 5: Push the final parent commit to main**

```bash
git add apps/Nexus-Cloud apps/Nexus-Dashboard
git commit -m "feat: enforce released app catalog policy"
git push origin main
```

- [ ] **Step 6: Final verification before reporting completion**

```bash
git log -3 --oneline --decorate
git status --short
```

Report exact commits, test totals, build results, runtime health, and any remaining expected warnings.

