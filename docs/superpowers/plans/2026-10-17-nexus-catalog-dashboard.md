# Nexus Dashboard Catalog and Curated Shelves Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to execute this plan task by task with verification after each task.

**Goal:** Replace the Dashboard’s flat app-directory behavior with curated shelves, bounded launcher results, full-catalog search, and user-visible maturity, health, access, and review states.

**Architecture:** Normalize catalog metadata once at the Dashboard API boundary, then keep shelf grouping and ordering in pure frontend helpers. The Home route shows a compact set of Essentials, Everyday, and one recommended shelf; the drawer is a fast searchable launcher; `/apps` is the complete catalog. Pins and recents affect presentation only and never authorize an app.

**Tech Stack:** React 19, React Router, TypeScript, Vite, Tailwind CSS, Vitest, Testing Library, Bun server tests.

**Spec:** `docs/superpowers/specs/2026-10-17-nexus-app-catalog-and-shelves-design.md`

## Global Constraints

- Use the shelf enum `everyday | work | create | develop | operate | personal | explore` exactly.
- Keep the empty-query launcher bounded at `EMPTY_QUERY_CAP = 12`; search with text must reach all searchable apps, including offline apps.
- Keep existing `/ipa/apps`, `/apps`, flat app paths, shell-native routes, delivery modes, health behavior, and SSO behavior compatible.
- Hidden or restricted entries must not appear in the ordinary user catalog without the corresponding access state; an unavailable app must never render as an activatable link.
- Use the existing accent and accessibility patterns; every new shelf/filter control needs an accessible name and `:focus-visible` state.
- Preserve local-storage compatibility while account-backed preference APIs are introduced in the scoped-preferences workstream.

---

### Task 1: Extend Dashboard app types and normalize catalog metadata

**Files:**
- Modify: `apps/Nexus-Dashboard/src/apps.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Create: `apps/Nexus-Dashboard/frontend/src/shell/catalog.ts`
- Test: `apps/Nexus-Dashboard/tests/apps.test.ts`
- Test: `apps/Nexus-Dashboard/frontend/src/shell/catalog.test.ts`

**Interfaces:**
- `CatalogShelf`, `CatalogAudience`, `CatalogMaturity`, `CatalogVisibility`, and `CatalogReviewStatus` mirror the Cloud contract.
- `AppEntry` gains `primaryShelf`, `secondaryShelves`, `audiences`, `capabilities`, `maturity`, `visibility`, and `reviewStatus`.
- `normalizeCatalogEntry(raw: unknown): DashboardCatalogEntry` supplies Explore/pending defaults for missing or malformed metadata.
- `appsForShelf(apps, shelf)`, `groupAppsByShelf(apps)`, `searchCatalog(apps, query, filters)`, and `orderedLauncherApps(apps, pinnedIds, recentIds)` are pure functions with stable ordering.

- [ ] **Step 1: Write failing normalization and shelf-helper tests.** Cover old payloads, approved records, secondary shelves, pending records, hidden records, capability filters, audience filters, text search over name/id/keywords, and one app appearing in its primary and secondary shelves without duplicate cards inside one shelf.

```ts
test("old payloads resolve to Explore with pending review", () => {
  expect(normalizeCatalogEntry({ id: "nexus-legacy", name: "Legacy" })).toMatchObject({
    primaryShelf: "explore",
    secondaryShelves: [],
    reviewStatus: "pending",
    maturity: "experimental",
  });
});
```

- [ ] **Step 2: Run the focused tests to verify failure.**

Run: `cd apps/Nexus-Dashboard && bun test tests/apps.test.ts frontend/src/shell/catalog.test.ts`

Expected: FAIL because catalog fields and helpers do not exist.

- [ ] **Step 3: Extend server and client wire types and implement normalization.** Keep `toAppEntries()` responsible for delivery/path/health normalization and call `normalizeCatalogEntry()` for catalog fields. Do not let components inspect raw Cloud responses.

- [ ] **Step 4: Implement pure shelf grouping and filtering.** Filter by `visibility`, query name/id/keywords, then sort healthy before offline and stable name within each shelf. Keep Explore as the fallback for pending/long-tail entries.

- [ ] **Step 5: Run focused tests and the Dashboard type-check.**

Run: `cd apps/Nexus-Dashboard && bun test tests/apps.test.ts frontend/src/shell/catalog.test.ts && cd frontend && bunx tsc --noEmit`

Expected: all helper tests pass and frontend TypeScript remains clean.

- [ ] **Step 6: Commit the catalog data boundary.**

```bash
git add apps/Nexus-Dashboard/src/apps.ts apps/Nexus-Dashboard/frontend/src/api.ts apps/Nexus-Dashboard/frontend/src/shell/catalog.ts apps/Nexus-Dashboard/tests/apps.test.ts apps/Nexus-Dashboard/frontend/src/shell/catalog.test.ts
git commit -m "feat(dashboard): normalize curated catalog metadata"
```

### Task 2: Add recents and preference-compatible launcher ordering

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/shell/useRecentApps.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/usePinnedApps.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/App.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/useRecentApps.test.ts`
- Test: `apps/Nexus-Dashboard/frontend/src/shell/Shell.test.tsx`

**Interfaces:**
- `readRecentIds(): string[]`, `writeRecentIds(ids: string[]): void`, and `recordRecentId(id: string): void` use a bounded local-storage list with the newest id first.
- `useRecentApps()` returns `[recentIds, recordRecent]` and ignores malformed storage.
- `Shell` records the app matching the current location after a successful route change; direct deep links are recorded as well as drawer launches.

- [ ] **Step 1: Write failing recents tests.** Verify newest-first ordering, duplicate promotion, maximum length of eight, malformed storage fallback, and no write when the current route is a shell-owned page.

- [ ] **Step 2: Run the focused test to verify failure.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test src/shell/__tests__/useRecentApps.test.ts`

Expected: FAIL because the hook and storage helpers do not exist.

- [ ] **Step 3: Implement bounded recent storage and route recording.** Use the same storage error handling as `usePinnedApps`; never throw because local storage is unavailable or corrupt.

- [ ] **Step 4: Pass recent ids into `AppsDrawer` and add a pure ordering assertion.** Pinned ids remain first, then recent ids, then healthy alphabetical fallback, capped at 12.

- [ ] **Step 5: Run shell tests and the Dashboard frontend check.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test src/shell/__tests__/useRecentApps.test.ts src/shell/__tests__/AppsDrawer.test.tsx src/shell/Shell.test.tsx && bunx tsc --noEmit`

Expected: existing pin behavior and new recent behavior pass together.

- [ ] **Step 6: Commit recent launcher state.**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell/useRecentApps.ts apps/Nexus-Dashboard/frontend/src/shell/usePinnedApps.ts apps/Nexus-Dashboard/frontend/src/shell/Shell.tsx apps/Nexus-Dashboard/frontend/src/App.tsx apps/Nexus-Dashboard/frontend/src/shell/__tests__/useRecentApps.test.ts apps/Nexus-Dashboard/frontend/src/shell/Shell.test.tsx
git commit -m "feat(dashboard): add bounded recent app ordering"
```

### Task 3: Build reusable shelf and app-card presentation components

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/src/components/AppCard.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/components/AppShelf.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/components/CatalogFilters.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/components/catalogLabels.ts`
- Test: `apps/Nexus-Dashboard/frontend/src/components/AppShelf.test.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/components/CatalogFilters.test.tsx`

**Interfaces:**
- `AppCard` accepts `{ app, compact?: boolean, showShelf?: boolean }` and renders a link only when `app.health === "healthy"` and access is available.
- `AppShelf` accepts `{ title, shelf, apps, limit?, emptyMessage? }` and renders a labelled list with optional “See all” navigation.
- `CatalogFilters` accepts controlled `query`, `shelf`, `audience`, and `capability` values and emits change callbacks; it does not fetch data or mutate preferences.
- `catalogLabels.ts` maps controlled ids to user-facing labels without changing ids sent over the API.

- [ ] **Step 1: Write component tests.** Cover healthy/offline/pending/restricted card states, accessible names, no dead links, shelf heading/list semantics, and filter control values.

- [ ] **Step 2: Run component tests to verify failure.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test src/components/AppShelf.test.tsx src/components/CatalogFilters.test.tsx`

Expected: FAIL because the components do not exist.

- [ ] **Step 3: Implement `AppCard` with status pills and explicit unavailable reasons.** Keep description text, delivery-independent paths, and visible maturity/review state; use `aria-disabled` on non-links only where it communicates state.

- [ ] **Step 4: Implement `AppShelf` and controlled filters using existing design-system primitives.** Keep visual density compact enough for Home and reusable for `/apps`.

- [ ] **Step 5: Run component tests, frontend type-check, and production build.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test src/components/AppShelf.test.tsx src/components/CatalogFilters.test.tsx && bunx tsc --noEmit && bun run build`

Expected: all tests, type-check, and build pass.

- [ ] **Step 6: Commit the reusable catalog UI.**

```bash
git add apps/Nexus-Dashboard/frontend/src/components/AppCard.tsx apps/Nexus-Dashboard/frontend/src/components/AppShelf.tsx apps/Nexus-Dashboard/frontend/src/components/CatalogFilters.tsx apps/Nexus-Dashboard/frontend/src/components/catalogLabels.ts apps/Nexus-Dashboard/frontend/src/components/AppShelf.test.tsx apps/Nexus-Dashboard/frontend/src/components/CatalogFilters.test.tsx
git commit -m "feat(dashboard): add reusable catalog shelf components"
```

### Task 4: Turn the Apps drawer into the fast shelf-aware launcher

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/AppsDrawer.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/filterApps.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/AppsDrawer.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/overlay.test.tsx` (only if overlay focus assertions need the new controls)

**Interfaces:**
- Drawer state includes `selectedShelf: CatalogShelf | "all"`, optional audience/capability filters, and query.
- Empty query uses pins → recents → healthy apps within the selected shelf, capped at 12.
- Non-empty query searches every visible shelf and includes offline entries so the user can see why an app is unavailable.

- [ ] **Step 1: Add failing drawer tests.** Verify shelf tabs, audience/capability filters, recents order, Explore visibility for pending apps, offline search, bounded default results, keyboard tab/arrow/Enter behavior, and the `/apps` link.

- [ ] **Step 2: Run the drawer suite to verify failure.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test src/shell/__tests__/AppsDrawer.test.tsx`

Expected: FAIL on the new controls and shelf-aware result policy.

- [ ] **Step 3: Implement drawer state and pure result selection.** Keep the existing listbox keyboard behavior; update `aria-controls`, `aria-selected`, and labels for the shelf/filter controls.

- [ ] **Step 4: Render cards with shelf, maturity, review, health, and access status.** Pending and deferred entries remain searchable under Explore; hidden entries are omitted from ordinary drawer results.

- [ ] **Step 5: Run all drawer, shell, and filter tests.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test src/shell/__tests__/AppsDrawer.test.tsx src/shell/__tests__/overlay.test.tsx src/shell/__tests__/useDensity.test.tsx`

Expected: existing launcher tests and new shelf/filter tests pass.

- [ ] **Step 6: Commit the shelf-aware drawer.**

```bash
git add apps/Nexus-Dashboard/frontend/src/shell/AppsDrawer.tsx apps/Nexus-Dashboard/frontend/src/shell/filterApps.ts apps/Nexus-Dashboard/frontend/src/shell/__tests__/AppsDrawer.test.tsx apps/Nexus-Dashboard/frontend/src/shell/__tests__/overlay.test.tsx
git commit -m "feat(dashboard): add shelf-aware apps drawer"
```

### Task 5: Rebuild Home and All Apps around curated shelves

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Grid.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Grid.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx`
- Create: `apps/Nexus-Dashboard/frontend/src/pages/catalog/catalogView.ts`
- Test: `apps/Nexus-Dashboard/frontend/src/pages/catalog/catalogView.test.ts`

**Interfaces:**
- Home renders `Essentials`, a compact `Everyday` shelf, one preferred/recommended shelf, and a link to `/apps`/Explore.
- `/apps` renders searchable, filterable groups by primary shelf and places pending/long-tail apps in Explore.
- `catalogView.ts` exposes `recommendedShelf(user, apps)` with default Everyday and deterministic role-aware ordering only when an explicit preference exists; it never changes permissions.

- [ ] **Step 1: Write failing Home/Grid tests.** Assert Home no longer renders an unbounded tile wall, Essentials includes pins/recents, Everyday is visible, Explore is reachable, `/apps` groups by shelf, and offline/pending/restricted states remain legible.

- [ ] **Step 2: Run Home/Grid tests to verify failure.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test src/pages/Home.test.tsx src/pages/Grid.test.tsx src/pages/catalog/catalogView.test.ts`

Expected: FAIL because Home and Grid still render the pre-catalog views.

- [ ] **Step 3: Implement `catalogView.ts` and Home shelves.** Keep the existing health strip and widgets; insert the curated catalog sections without removing account, activity, or health information.

- [ ] **Step 4: Implement full catalog grouping, search, filters, and empty/error states in Grid.** Preserve the existing `/apps` route and same-origin `/ipa/apps` fetch.

- [ ] **Step 5: Run the full Dashboard frontend test and build gates.**

Run: `cd apps/Nexus-Dashboard/frontend && bun test && bunx tsc --noEmit && bun run build`

Expected: all existing and new frontend tests pass and production build succeeds.

- [ ] **Step 6: Commit the curated Home and All Apps views.**

```bash
git add apps/Nexus-Dashboard/frontend/src/pages/Home.tsx apps/Nexus-Dashboard/frontend/src/pages/Grid.tsx apps/Nexus-Dashboard/frontend/src/pages/Grid.test.tsx apps/Nexus-Dashboard/frontend/src/pages/Home.test.tsx apps/Nexus-Dashboard/frontend/src/pages/catalog/catalogView.ts apps/Nexus-Dashboard/frontend/src/pages/catalog/catalogView.test.ts
git commit -m "feat(dashboard): organize Home and All Apps by shelves"
```

### Task 6: Run Dashboard integration and accessibility gates

**Files:**
- Modify: `apps/Nexus-Dashboard/tests/apps.test.ts`
- Modify: `apps/Nexus-Dashboard/tests/server.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/shell/__tests__/shell.integration.test.tsx`
- Modify: `apps/Nexus-Dashboard/README.md`

- [ ] **Step 1: Add server integration cases for Cloud catalog success, old Cloud fallback, Cloud outage, hidden filtering, and shell-native app merging.**

- [ ] **Step 2: Add browser-level assertions for skip link, shelf controls, listbox semantics, visible focus rings, keyboard navigation, and the bounded empty-query state.**

- [ ] **Step 3: Run all Dashboard gates.**

Run: `cd apps/Nexus-Dashboard && bun run check && bun test tests/ && cd frontend && bun test && bunx tsc --noEmit && bun run build`

Expected: server tests, frontend tests, type-check, and production build all pass.

- [ ] **Step 4: Run `git diff --check`, inspect route/delivery compatibility, and commit the integration documentation.**

```bash
git add apps/Nexus-Dashboard/tests/apps.test.ts apps/Nexus-Dashboard/tests/server.test.ts apps/Nexus-Dashboard/frontend/src/shell/__tests__/shell.integration.test.tsx apps/Nexus-Dashboard/README.md
git commit -m "test(dashboard): verify curated catalog integration"
```
