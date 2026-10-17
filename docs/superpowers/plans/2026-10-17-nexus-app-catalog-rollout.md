# Nexus App Catalog and Curated Shelves Rollout Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans` to execute the selected workstream task by task with verification after each task.

**Goal:** Implement the approved Nexus app catalog architecture so a large ecosystem is organized into curated shelves, governed by Cloud approval, resolved across hosted tenants and self-hosted nodes, and personalized per user.

**Architecture:** The rollout is intentionally split into four independently testable workstreams. Cloud governance establishes the catalog contract and approval projection; Dashboard consumes that projection and renders shelves; scoped resolution adds tenant/node/user layers; seed and release verification proves the complete flow. Runtime delivery and authorization remain separate from catalog placement throughout.

**Tech Stack:** Bun, strict TypeScript, React 19, React Router, Tailwind CSS, Vitest, Testing Library, JSON atomic stores, Nexus-Auth RBAC, Nexus-Cloud Systems API.

**Spec:** `docs/superpowers/specs/2026-10-17-nexus-app-catalog-and-shelves-design.md`

## Global Constraints

- Preserve the single ecosystem login and all existing app paths, delivery modes, health probes, and route behavior.
- Use only the controlled shelves `everyday | work | create | develop | operate | personal | explore`.
- Use only the controlled audiences `general | business | creator | developer | operations | power-user`.
- Shelf placement never grants access; access remains enforced by platform and application authorization.
- Hosted `tnhc.dev` has one global approved catalog even when several platform administrators or catalog stewards exist.
- Tenant owners/admins and self-hosted node owners/admins can change only their own availability and local overlays.
- User pins, recents, and preferred shelf change presentation only.
- Keep the current local-storage preferences as a fallback until account-backed preferences are available.
- Treat pending, deferred, rejected, offline, restricted, and hidden states explicitly in API, UI, and tests.
- Preserve unrelated working-tree changes and stage only files named by the active task.

## Workstream sequence

The workstreams are separate plans because each has its own data boundary and test cycle. Execute them in this order:

1. [Cloud catalog governance](2026-10-17-nexus-catalog-cloud-governance.md) — contract, persistence, proposals, approval, audit, Dashboard projection.
2. [Dashboard catalog UX](2026-10-17-nexus-catalog-dashboard.md) — normalization, recents, drawer shelves, Home, and All Apps.
3. [Scoped resolution and preferences](2026-10-17-nexus-catalog-scoped-resolution.md) — tenant entitlements, node overlays, federated snapshots, and account preferences.
4. [Seed and release verification](2026-10-17-nexus-catalog-seed-and-release.md) — core metadata, disposable integration fixtures, and the full verification matrix.

## Cross-workstream interfaces

Cloud produces one normalized catalog entry per tool id:

```ts
type DashboardCatalogEntry = {
  id: string;
  name: string;
  description: string;
  path?: string;
  publicUrl?: string;
  delivery?: "shell-native" | "proxied-app" | "framed" | "external";
  health: "healthy" | "offline";
  primaryShelf: "everyday" | "work" | "create" | "develop" | "operate" | "personal" | "explore";
  secondaryShelves: string[];
  audiences: string[];
  capabilities: string[];
  maturity: "stable" | "preview" | "experimental";
  visibility: "default" | "discoverable" | "restricted" | "hidden";
  reviewStatus: "approved" | "pending" | "deferred" | "rejected";
};
```

The Dashboard preserves this identity while adding presentation state:

```ts
type CatalogPreferences = {
  pinnedIds: string[];
  recentIds: string[];
  preferredShelf: "everyday" | "work" | "create" | "develop" | "operate" | "personal" | "explore";
};
```

The effective-resolution boundary is:

```text
global approved catalog
→ tenant entitlements
→ node-local policy and local apps
→ runtime health and access state
→ user preferences
```

## Verification gates

After each workstream:

- Run its focused tests before the implementation step is considered complete.
- Run its package type-check and relevant production build.
- Run `git diff --check`.
- Inspect the staged file list and preserve unrelated working-tree changes.
- Commit the workstream’s small, reviewable changes before moving to the next workstream.

At the end:

- Run Cloud, Auth, Dashboard server, and Dashboard frontend gates.
- Re-run the existing Design, Graphic, Photos, Video, Draw, Chat, Auth, Calendar, and Project regression matrix.
- Verify `HEAD` equals `origin/main` after the authorized push.
- Record expected warnings separately from failures in the verification report.
