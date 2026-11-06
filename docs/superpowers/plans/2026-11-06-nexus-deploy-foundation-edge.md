# Nexus Deploy Foundation — Plan 2 of 3: The Edge

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make an app that Deploy has built reachable at `https://<slug>.<domain>` and at `https://deploy.<domain>/<slug>`, isolated from every visitor's session, with the name checked against everything else on the node. Nothing is restarted or deployed to production by this plan.

**Architecture:** Nexus Cloud's registry stays the single routing authority. Cloud gains a *user-apps* surface behind its own credential: one `PUT` publishes or moves an app's route and marks it as a member's app, which the ecosystem proxy reads from `/api/v1/routes` and treats like a Hosting site (public, no cookies or credentials forwarded, no visitor tag). The proxy also routes `deploy.<domain>` to Deploy and turns `deploy.<domain>/<slug>` into a redirect. Deploy gets a Cloud client that implements the `RoutePublisher` interface from Plan 1, re-announces live apps as the heartbeat, and checks names against Cloud and Hosting.

**Tech Stack:** Bun + TypeScript (Cloud, proxy); Node ESM + TypeScript with `node:test` (Deploy). No new dependencies.

**Spec:** `docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md` (§5 units 4–6, §6, §8.1, §8.2, §14). **Plan 1:** `docs/superpowers/plans/2026-11-06-nexus-deploy-foundation-control-plane.md`, already built; Deploy is at v0.6.0.

## Where this plan sits

| Plan | Delivers | State of the world when it is done |
|---|---|---|
| 1 — Control plane | Everything inside `apps/Nexus-Deploy`, with a route-publisher *interface* | Built and pushed. Not deployed. |
| **2 — Edge** (this plan) | Cloud user-apps surface, proxy isolation, `deploy.<domain>` host and short link, Deploy's Cloud client and name checks, an end-to-end check | Tested in isolation and end to end against a throwaway Cloud. **Still not live**: no running service is restarted, and no credential is created. |
| 3 — Sandbox, conformance, dashboard | `sudo` sandbox setup and capped storage, builder, `doctor`/`verify` with fixture apps, dashboard build and theme, `deploy.sh` bring-up (this is where the credential is created and services restart), the disposable second-node proof | The spec's success criterion. |

## Spec coverage

| Spec section | Where it is built |
|---|---|
| §5 unit 4 Router (user-app isolation) | Task 3 |
| §5 unit 5 Short link | Task 4 |
| §5 unit 6 Name registry (real Cloud and Hosting check) | Tasks 2, 5, 6 |
| §6.1 Cloud as routing authority; heartbeat; user-app flag | Tasks 1, 2, 5, 6 |
| §6.2 Precedence (fixed names → Cloud routes → Hosting) | Task 3 (a member's app is a Cloud route, so it already sits between the fixed names and Hosting); proven in Task 7 |
| §6.3 Name registry: reserved, Cloud, Hosting, fail closed | Tasks 2, 5 |
| §6.4 Short link | Task 4 |
| §8.1 Sessions and cookies: `Cookie`/`Authorization` stripped, `Domain` cookies dropped | Task 3 |
| §8.2 No visitor address or client tag reaches an app | Task 3 |
| §10 item 4 (proxy and Cloud: fixed `deploy` route, user-app flag, short link) | Tasks 1–4 |
| §14 task 1 (`HttpOnly` session cookie) | Resolved while writing this plan: see "Verification results" |
| §14 task 4 (how Cloud models the flag; Hosting name check) | Resolved while writing this plan: see "Decisions" |
| Everything else (sandbox, builder, `doctor`/`verify`, dashboard, bring-up, second-node proof; §14 tasks 2, 3, 5–7) | Plan 3 |

## Decisions made while writing this plan

These refine the spec where it said "a planning task". Each was settled by reading the code, and the code in this plan is what was built and tested.

1. **A separate credential for member apps.** Cloud has one node API key, held by every ecosystem service, and its own source calls it "explicitly not an operator identity". Letting that key publish a member's app would let any service mint or clear the isolation flag. So `NEXUS_CLOUD_USER_APPS_KEY` guards a dedicated `/api/v1/user-apps/:slug` surface, the node key is refused there, and the generic registration refuses the `deploy-` id prefix so it cannot create or take over a member's app. The flag (`userDeployed`) is carried over from the previous record and never read from a payload, exactly like `requiresAuth`.
2. **Cloud limits where a member's app may point.** A route can target only `http://127.0.0.1:<port>` with the port inside `NEXUS_CLOUD_USER_APPS_PORTS` (default `20000-29999`). A leaked credential therefore cannot aim a public hostname at Auth, Cloud or a database.
3. **Hosting is asked through its site-proxy, with no change to Hosting.** Hosting has no name-availability endpoint. Its site-proxy answers an unknown host with a 404 page that says "No site found", and any site (including a private or password-gated one) with something else. Deploy asks it by `Host` header. An unreachable Hosting or a 5xx makes the check fail, and a failed check refuses the name. **Known limitation, accepted:** the site-proxy returns the same 404 when its own database errors, so a Hosting database outage can make a taken name look free. Because Cloud routes win over Hosting (§6.2), the failure mode is a member's app shadowing a Hosting site, never leaking its content. An explicit Hosting endpoint is the real fix and is left as a follow-up.
4. **`deploy.<domain>` is an application host that authenticates itself.** It joins the proxy's public-host allowlist next to Hosting and Auth, because Deploy's API checks the session with Auth, its pages send browsers to sign in, and GitHub's push webhook has no session at all. It stays `kind: "app"`, so the proxy's same-site mutation block (O-1) still protects it from a member's app making credentialed requests to it.
5. **A member's app is a public site routed by Cloud.** `buildRouteMap` turns `userDeployed: true` into `kind: "site"` with no gate; it is never gated whatever `requiresAuth` says. A mistyped flag (anything but boolean `true`) leaves the route an ordinary, gated application.
6. **The re-announcement is the heartbeat.** `PUT` is idempotent and also records Cloud's heartbeat. Deploy repeats it for every live app on a timer and removes a route whose app is no longer live.

## Verification results

- **§14 task 1 — the Auth session cookie is `HttpOnly`.** Confirmed in `apps/Nexus-Auth/src/server.ts` (the cookie is built with `HttpOnly` at line 192 and cleared with it at 207). The residual risk stays as the spec states it: browser-side JavaScript in a member's app cannot *read* the session, but can plant a cookie for the parent domain; the proxy drops any such `Set-Cookie` coming from an app's response, and invite-only access bounds who can try.

## Global Constraints

Copied from the spec and the project rules. Every task's requirements include them.

- Free; no billing, plans or paywalls; the only recurring cost allowed is the domain. No dependency is added.
- No visitor address is read or stored. For a member's app the proxy forwards no visitor tag either, so the app learns nothing about the visitor.
- A member's app receives no `Cookie` and no `Authorization` header, and cannot set a cookie for the parent domain.
- Fail closed: an unreachable Cloud or Hosting refuses a name; a Cloud that is not configured with a user-apps key refuses everything (503), never accepts anything.
- The node API key can never publish, move or remove a member's app.
- No source file in `apps/Nexus-Deploy/src` names a domain (`tests/no-hardcoded-domain.test.ts` enforces it).
- Cloud, the proxy and Deploy each commit to `main` of their own repository (solo developer: no branches, no pull requests), in `type(scope): description` form, with the `Co-Authored-By` line shown in each commit step.
- **Do not restart or reconfigure any running service, create any production credential, or edit a `.env` in this plan.** That belongs to Plan 3's bring-up, which the operator runs.

## Running things

Three codebases, three ways to run tests. All paths are from the monorepo root.

**Cloud** (`apps/Nexus-Cloud`, Bun). Some Cloud tests import `server.ts`, which binds a port; the running production Cloud holds 8787, so always set a spare port:

```bash
cd apps/Nexus-Cloud
PORT=18787 bun test src/systems-api src/test src/api          # the suites this plan touches
node node_modules/typescript/bin/tsc --noEmit -p tsconfig.json
```

Baseline before Task 1: **116 tests pass, 0 fail** (46 in `src/systems-api` + `src/test`, 70 in `src/api`). Run `tsc` once before Task 1 and keep its output; this plan adds no new type errors to whatever it prints. Do not run `bun test` with no arguments: other suites in the repository expect sibling apps and the live port.

**Proxy** (`deploy/production`, Bun):

```bash
cd deploy/production
bun test tests/proxy.test.ts tests/gate.test.ts tests/client-tag.test.ts
```

Baseline before Task 3: **105 tests pass, 0 fail**.

**Deploy** (`apps/Nexus-Deploy`, Node). The repository sits on a mount that cannot execute binaries, so esbuild and Biome run from copies:

```bash
cd apps/Nexus-Deploy
mkdir -p /tmp/nexus-deploy-tools
cp node_modules/@esbuild/linux-x64/bin/esbuild /tmp/nexus-deploy-tools/esbuild
cp node_modules/@biomejs/cli-linux-x64/biome /tmp/nexus-deploy-tools/biome
chmod +x /tmp/nexus-deploy-tools/esbuild /tmp/nexus-deploy-tools/biome
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Baseline before Task 5: **202 tests pass**, both `tsc` runs clean.

**Order matters.** Tasks 1–2 (Cloud) and 3–4 (proxy) are independent of each other; Tasks 5–6 (Deploy) do not need them to run their own tests; Task 7 needs all of them.

---

## Tasks

### Task 1: Cloud: the member-app flag and the publish, move and remove operations

Cloud's registry already emits the routes the proxy reads. This adds one more fact to a route, `userDeployed`, and the operations that set it. The flag follows the same rule as `requiresAuth`: it is carried over from the previous record and never read from a registration payload, so an app cannot choose it and a re-registration cannot clear it. `publishUserApp` registers the tool as `deploy-<slug>`, flags it *before* exposing it (so no route is ever emitted without isolation), and is idempotent, which is what makes it both the blue/green switch and the heartbeat. The generic registration refuses the `deploy-` prefix.

**Repository:** `apps/Nexus-Cloud` (its own git repository). Paths below are relative to it.

**Files:**

- Modify: `src/systems-api/registry.ts`
- Modify: `src/systems-api/service.ts`
- Modify: `src/systems-api/store.ts`
- Modify: `src/systems-api/types.ts`
- Create: `src/test/user-apps.test.ts`

**Interfaces:**

- Consumes: Existing `registerSystemsApiTool`, `requestExposure`, `recordToolHeartbeat`, `revokeSystemsApiExposure`, `deregisterTool` in `src/systems-api/registry.ts`.
- Produces: `SystemsApiTool.userDeployed?`, `SystemsApiRoute.userDeployed`, `USER_APP_ID_PREFIX`, `userAppToolId(slug)`, `isHostInUse(host, exceptToolId?)`, `publishUserApp({ slug, upstreamUrl, host }): { ok: true, tool } | { ok: false, reason: 'conflict' | 'denied' }`, `unpublishUserApp(slug)`, all re-exported on `systemsApiService`.

- [ ] **Step 1: Write the tests**

**Create `src/test/user-apps.test.ts`:**

````typescript
import { describe, expect, test } from "bun:test";
import { createSystemsApiTestHarness, emptySystemsApiRegistry } from "./systems-api-harness";

/**
 * A member's deployed app gets the proxy's user-site isolation through one flag
 * on its route. These tests pin how that flag is set and, as importantly, how it
 * cannot be: it must not come from a payload, and it must survive a re-register.
 */
describe("user-deployed apps", () => {
  async function harness() {
    return createSystemsApiTestHarness(emptySystemsApiRegistry);
  }
  const routeFor = (h: Awaited<ReturnType<typeof harness>>, host: string) =>
    h.systemsApiService.listSystemsApiRoutes().find((r) => r.domain === host);

  test("publishing makes a flagged route to the given port", async () => {
    const h = await harness();
    try {
      const result = h.systemsApiService.publishUserApp({
        slug: "my-app",
        upstreamUrl: "http://127.0.0.1:20001",
        host: "my-app.nexus.local",
      });
      expect(result.ok).toBe(true);
      const route = routeFor(h, "my-app.nexus.local");
      expect(route?.upstream).toBe("http://127.0.0.1:20001");
      expect(route?.userDeployed).toBe(true);
      expect(route?.requiresAuth).toBe(false);
    } finally {
      h.cleanup();
    }
  });

  test("publishing again moves the route to the new port (the blue/green switch)", async () => {
    const h = await harness();
    try {
      const publish = (port: number) =>
        h.systemsApiService.publishUserApp({
          slug: "my-app",
          upstreamUrl: `http://127.0.0.1:${port}`,
          host: "my-app.nexus.local",
        });
      publish(20001);
      publish(20002);
      const routes = h.systemsApiService
        .listSystemsApiRoutes()
        .filter((r) => r.domain === "my-app.nexus.local");
      expect(routes).toHaveLength(1);
      expect(routes[0]?.upstream).toBe("http://127.0.0.1:20002");
    } finally {
      h.cleanup();
    }
  });

  test("an ordinary route is not flagged", async () => {
    const h = await harness();
    try {
      h.systemsApiService.registerSystemsApiTool({
        id: "plain",
        name: "Plain",
        description: "An ecosystem tool",
        upstreamUrl: "http://127.0.0.1:4200",
        exposed: false,
        health: "healthy",
        capabilities: [],
      });
      h.systemsApiService.requestSystemsApiExposure({
        toolId: "plain",
        desiredHost: "plain.nexus.local",
      });
      expect(routeFor(h, "plain.nexus.local")?.userDeployed).toBe(false);
    } finally {
      h.cleanup();
    }
  });

  test("the generic registration cannot create or take over a deployed app", async () => {
    const h = await harness();
    try {
      expect(() =>
        h.systemsApiService.registerSystemsApiTool({
          id: "deploy-sneaky",
          name: "Sneaky",
          description: "Claims the deployed-app namespace",
          upstreamUrl: "http://127.0.0.1:1",
          exposed: false,
          health: "healthy",
          capabilities: [],
        }),
      ).toThrow(/reserved/);

      h.systemsApiService.publishUserApp({
        slug: "mine",
        upstreamUrl: "http://127.0.0.1:20003",
        host: "mine.nexus.local",
      });
      expect(() =>
        h.systemsApiService.registerSystemsApiTool({
          id: "deploy-mine",
          name: "Overwrite",
          description: "Tries to re-register a deployed app",
          upstreamUrl: "http://127.0.0.1:2",
          exposed: false,
          health: "healthy",
          capabilities: [],
        }),
      ).toThrow(/reserved/);
      expect(routeFor(h, "mine.nexus.local")?.upstream).toBe("http://127.0.0.1:20003");
    } finally {
      h.cleanup();
    }
  });

  test("a payload field cannot set the flag on an ordinary tool", async () => {
    const h = await harness();
    try {
      h.systemsApiService.registerSystemsApiTool({
        id: "plain2",
        name: "Plain 2",
        description: "Sends a userDeployed field",
        upstreamUrl: "http://127.0.0.1:4201",
        exposed: false,
        health: "healthy",
        capabilities: [],
        userDeployed: true,
      } as never);
      expect(h.systemsApiService.getSystemsApiTool("plain2")?.userDeployed).toBeUndefined();
    } finally {
      h.cleanup();
    }
  });

  test("a host held by another tool is a conflict, and nothing is changed", async () => {
    const h = await harness();
    try {
      h.systemsApiService.registerSystemsApiTool({
        id: "draw",
        name: "Draw",
        description: "Ecosystem app",
        upstreamUrl: "http://127.0.0.1:8091",
        exposed: false,
        health: "healthy",
        capabilities: [],
      });
      h.systemsApiService.requestSystemsApiExposure({ toolId: "draw", desiredHost: "draw.nexus.local" });

      const result = h.systemsApiService.publishUserApp({
        slug: "draw",
        upstreamUrl: "http://127.0.0.1:20004",
        host: "draw.nexus.local",
      });
      expect(result).toEqual({ ok: false, reason: "conflict" });
      expect(routeFor(h, "draw.nexus.local")?.upstream).toBe("http://127.0.0.1:8091");
      expect(h.systemsApiService.getSystemsApiTool("deploy-draw")).toBeNull();
      expect(h.systemsApiService.isHostInUse("draw.nexus.local")).toBe(true);
      expect(h.systemsApiService.isHostInUse("free.nexus.local")).toBe(false);
    } finally {
      h.cleanup();
    }
  });

  test("unpublishing removes the route and refuses anything that is not a deployed app", async () => {
    const h = await harness();
    try {
      h.systemsApiService.registerSystemsApiTool({
        id: "plain3",
        name: "Plain 3",
        description: "Ecosystem tool",
        upstreamUrl: "http://127.0.0.1:4202",
        exposed: false,
        health: "healthy",
        capabilities: [],
      });
      h.systemsApiService.publishUserApp({
        slug: "gone",
        upstreamUrl: "http://127.0.0.1:20005",
        host: "gone.nexus.local",
      });
      expect(h.systemsApiService.unpublishUserApp("gone")).not.toBeNull();
      expect(routeFor(h, "gone.nexus.local")).toBeUndefined();
      expect(h.systemsApiService.isHostInUse("gone.nexus.local")).toBe(false);
      expect(h.systemsApiService.unpublishUserApp("gone")).toBeNull();
      expect(h.systemsApiService.unpublishUserApp("plain3")).toBeNull();
    } finally {
      h.cleanup();
    }
  });
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Cloud
PORT=18787 bun test src/test/user-apps.test.ts
```

Expected: FAIL — the tests fail with `publishUserApp is not a function` (and the route has no `userDeployed`).

- [ ] **Step 3: Implement**

**Modify `src/systems-api/registry.ts`:**

````diff
--- a/src/systems-api/registry.ts
+++ b/src/systems-api/registry.ts
@@ -486,6 +486,8 @@ function buildTool(
     // — the same way an omitted publicUrl once clobbered a live route. Only
     // setToolRequiresAuth changes it.
     ...(previous?.requiresAuth !== undefined ? { requiresAuth: previous.requiresAuth } : {}),
+    // Same rule: carried, never read from `input`. Only publishUserApp sets it.
+    ...(previous?.userDeployed !== undefined ? { userDeployed: previous.userDeployed } : {}),
     ...(lastHeartbeatAt !== undefined ? { lastHeartbeatAt } : {}),
     heartbeatCount: previous?.heartbeatCount ?? 0,
     registeredAt: previous?.registeredAt ?? now(),
@@ -584,7 +586,26 @@ export function listToolHistory(toolId: string): readonly SystemsApiToolHistoryE
   return registry.history.filter((entry) => entry.toolId === toolId);
 }
 
+/** Tool ids that belong to apps members deployed through Nexus Deploy. */
+export const USER_APP_ID_PREFIX = "deploy-";
+
+export function userAppToolId(slug: string): string {
+  return `${USER_APP_ID_PREFIX}${slug}`;
+}
+
+/**
+ * The node API key is shared by every ecosystem service, so it must not be able
+ * to mint, or take over, a member's app: those carry the user-site isolation the
+ * proxy applies, and are published only through publishUserApp.
+ */
 export function registerSystemsApiTool(input: SystemsApiToolRegistrationInput): SystemsApiTool {
+  if (input.id.startsWith(USER_APP_ID_PREFIX) || getTool(input.id)?.userDeployed === true) {
+    throw new Error(`tool ids starting with "${USER_APP_ID_PREFIX}" are reserved for deployed apps`);
+  }
+  return registerToolRecord(input);
+}
+
+function registerToolRecord(input: SystemsApiToolRegistrationInput): SystemsApiTool {
   if (input.catalogProposal !== undefined && !validateCatalogProposal(input.catalogProposal)) {
     throw new Error("Invalid catalog proposal");
   }
@@ -1282,6 +1303,7 @@ export function listActiveRoutes(): readonly SystemsApiRoute[] {
       securityTag: phantomSecurityTag(tool.phantomSecurityProfile),
       phantomProtectionLevel: phantomProtectionLevel(tool.phantomSecurityProfile),
       requiresAuth: tool.requiresAuth === true,
+      userDeployed: tool.userDeployed === true,
       status: "active",
     });
   }
@@ -1306,6 +1328,7 @@ export function listActiveRoutes(): readonly SystemsApiRoute[] {
       securityTag: phantomSecurityTag(tool.phantomSecurityProfile),
       phantomProtectionLevel: phantomProtectionLevel(tool.phantomSecurityProfile),
       requiresAuth: tool.requiresAuth === true,
+      userDeployed: tool.userDeployed === true,
       status: "active",
     });
   }
@@ -1329,6 +1352,7 @@ export function listActiveRoutes(): readonly SystemsApiRoute[] {
       securityTag: phantomSecurityTag(tool.phantomSecurityProfile),
       phantomProtectionLevel: phantomProtectionLevel(tool.phantomSecurityProfile),
       requiresAuth: tool.requiresAuth === true,
+      userDeployed: tool.userDeployed === true,
       status: "active",
     });
   }
@@ -1352,3 +1376,98 @@ export function listActiveRoutes(): readonly SystemsApiRoute[] {
     };
   });
 }
+
+
+// ── Apps deployed by members ─────────────────────────────────────────────────
+
+export type UserAppPublishInput = {
+  slug: string;
+  /** Where the app listens, e.g. http://127.0.0.1:20017. */
+  upstreamUrl: string;
+  /** The public hostname, e.g. my-app.tnhc.dev. */
+  host: string;
+};
+
+export type UserAppPublishResult =
+  | { ok: true; tool: SystemsApiTool }
+  | { ok: false; reason: "conflict" | "denied" };
+
+function hostOf(url: string): string {
+  return url.replace(/^https?:\/\//, "").replace(/\/.*$/, "").toLowerCase();
+}
+
+/**
+ * Whether anything other than `exceptToolId` already answers for `host`: a
+ * tool's public URL, an exposure, an address or a bound domain. Revoked
+ * records do not count.
+ */
+export function isHostInUse(host: string, exceptToolId?: string): boolean {
+  const wanted = host.toLowerCase();
+  const other = (toolId: string) => toolId !== exceptToolId;
+  if (registry.tools.some((t) => other(t.id) && t.publicUrl && hostOf(t.publicUrl) === wanted)) {
+    return true;
+  }
+  if (
+    registry.exposures.some(
+      (e) => other(e.toolId) && e.status !== "revoked" && hostOf(e.publicUrl) === wanted,
+    )
+  ) {
+    return true;
+  }
+  if (
+    registry.addresses.some(
+      (a) => other(a.toolId) && a.status !== "revoked" && hostOf(a.publicAddress) === wanted,
+    )
+  ) {
+    return true;
+  }
+  return registry.domains.some(
+    (d) => other(d.toolId) && d.status !== "revoked" && d.domain.toLowerCase() === wanted,
+  );
+}
+
+/**
+ * Register (or move) a member's app and make its host route to `upstreamUrl`.
+ * Idempotent: calling it again with a new port is how a blue/green switch
+ * reaches the proxy, and calling it on a timer is the heartbeat.
+ */
+export function publishUserApp(input: UserAppPublishInput): UserAppPublishResult {
+  const toolId = userAppToolId(input.slug);
+  const existing = getTool(toolId);
+  if (existing && existing.userDeployed !== true) return { ok: false, reason: "conflict" };
+  if (isHostInUse(input.host, toolId)) return { ok: false, reason: "conflict" };
+
+  registerToolRecord({
+    id: toolId,
+    name: input.slug,
+    description: "An app deployed with Nexus Deploy",
+    exposed: false,
+    health: "healthy",
+    delivery: "proxied-app",
+    upstreamUrl: input.upstreamUrl,
+    capabilities: [],
+  });
+  // Flagged before it is exposed, so no route is ever emitted without isolation.
+  updateToolRecord(toolId, (current) => ({ ...current, userDeployed: true }));
+
+  const active = registry.exposures.find((e) => e.toolId === toolId && e.status === "active");
+  if (!active || hostOf(active.publicUrl) !== input.host.toLowerCase()) {
+    const exposure = requestExposure({ toolId, desiredHost: input.host });
+    if (!exposure || exposure.status !== "active") {
+      deregisterTool(toolId);
+      return { ok: false, reason: "denied" };
+    }
+  }
+  const tool = recordToolHeartbeat(toolId, { upstreamUrl: input.upstreamUrl, health: "healthy" });
+  if (!tool) return { ok: false, reason: "denied" };
+  return { ok: true, tool };
+}
+
+/** Remove a member's app. Refuses (null) anything that is not one. */
+export function unpublishUserApp(slug: string): SystemsApiTool | null {
+  const toolId = userAppToolId(slug);
+  const tool = getTool(toolId);
+  if (!tool || tool.userDeployed !== true) return null;
+  revokeSystemsApiExposure(toolId);
+  return deregisterTool(toolId);
+}
````

**Modify `src/systems-api/service.ts`:**

````diff
--- a/src/systems-api/service.ts
+++ b/src/systems-api/service.ts
@@ -4,6 +4,9 @@ import { systemsApiDeployIntegration as deployIntegration } from "./deploy";
 import {
   canSubmitCatalogProposal,
   deregisterTool,
+  isHostInUse,
+  publishUserApp,
+  unpublishUserApp,
   describeStatus,
   disableSystemsApiTool as disableTool,
   enableSystemsApiTool as enableTool,
@@ -588,4 +591,7 @@ export const systemsApiService = {
   applyHeartbeatExpiry,
   probeExposedToolsHealth,
   getCloudDomain,
+  isHostInUse,
+  publishUserApp,
+  unpublishUserApp,
 };
````

**Modify `src/systems-api/store.ts`:**

````diff
--- a/src/systems-api/store.ts
+++ b/src/systems-api/store.ts
@@ -218,6 +218,9 @@ function sanitizeTool(value: unknown): SystemsApiTool | null {
   // anywhere reported a change. A security switch that resets itself and says
   // nothing is worse than not having one.
   const requiresAuth = typeof value.requiresAuth === "boolean" ? value.requiresAuth : undefined;
+  // Read for the same reason as requiresAuth: dropping it on load would turn a
+  // member's app into an ordinary, un-isolated route at the next restart.
+  const userDeployed = typeof value.userDeployed === "boolean" ? value.userDeployed : undefined;
   const toolResult = {
     id,
     name,
@@ -244,6 +247,7 @@ function sanitizeTool(value: unknown): SystemsApiTool | null {
     ...(upstreamUrl !== undefined ? { upstreamUrl } : {}),
     ...(lastHeartbeatAt !== undefined ? { lastHeartbeatAt } : {}),
     ...(requiresAuth !== undefined ? { requiresAuth } : {}),
+    ...(userDeployed !== undefined ? { userDeployed } : {}),
   };
 }
````

**Modify `src/systems-api/types.ts`:**

````diff
--- a/src/systems-api/types.ts
+++ b/src/systems-api/types.ts
@@ -76,6 +76,15 @@ export type SystemsApiTool = {
    * reachable exactly as it is today.
    */
   requiresAuth?: boolean;
+  /**
+   * True for an app a member deployed through Nexus Deploy. The proxy gives such
+   * a route user-site isolation (no cookies, no credentials, no visitor tag).
+   *
+   * Set only by publishUserApp, which is reachable only with the user-apps key.
+   * Never read from a registration payload or a patch, so an app cannot choose
+   * its own treatment and a registrant cannot clear it.
+   */
+  userDeployed?: boolean;
   lastHeartbeatAt?: string;
   heartbeatCount: number;
   registeredAt: string;
@@ -343,6 +352,8 @@ export type SystemsApiRoute = {
   phantomProtectionLevel: SystemsApiPhantomProtectionLevel;
   /** Mirrors the tool's flag; the proxy reads this to decide whether to gate. */
   requiresAuth: boolean;
+  /** Mirrors the tool's flag; the proxy isolates the route as a member's app. */
+  userDeployed: boolean;
   status: "active";
   /** Present when an operator has this part in maintenance; the proxy serves
    *  the maintenance page instead of proxying. */
````

- [ ] **Step 4: Run the checks**

```bash
cd apps/Nexus-Cloud
node node_modules/typescript/bin/tsc --noEmit -p tsconfig.json
PORT=18787 bun test src/systems-api src/test src/api
```

Expected: `tsc` prints nothing new; **123 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Cloud
git add src/systems-api/registry.ts src/systems-api/service.ts src/systems-api/store.ts src/systems-api/types.ts src/test/user-apps.test.ts
git commit \
  -m "feat(user-apps): flag member apps on their routes and publish, move and remove them" \
  -m "A route carries userDeployed so the proxy can isolate it. The flag is set only by
publishUserApp and carried, never read from a payload. The generic registration
refuses the deploy- id prefix." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 2: Cloud: the user-apps endpoints, behind their own credential

Three endpoints under `/api/v1/user-apps/:slug`: `PUT` publishes or moves, `DELETE` removes, `GET .../availability` says whether a name is already used. They are guarded by `NEXUS_CLOUD_USER_APPS_KEY`, compared in constant time; the node API key is refused, and with no key configured the surface answers 503 rather than accepting anything. The upstream must be `http://127.0.0.1:<port>` inside `NEXUS_CLOUD_USER_APPS_PORTS` (default `20000-29999`). The dispatch runs before the node-API-key gate because that gate must not see these requests.

**Repository:** `apps/Nexus-Cloud` (its own git repository). Paths below are relative to it.

**Files:**

- Modify: `src/api/cloud-control-contract.ts`
- Modify: `src/api/handlers.ts`
- Modify: `src/api/routes.manifest.test.ts`
- Modify: `src/api/routes.ts`
- Create: `src/api/user-apps.ts`
- Modify: `src/config.ts`
- Create: `src/test/user-apps-http.test.ts`

**Interfaces:**

- Consumes: `systemsApiService.publishUserApp`, `unpublishUserApp`, `isHostInUse` (Task 1).
- Produces: `handleUserAppsRequest(request, pathname): Promise<Response | null>`, `isAllowedUserAppUpstream(value)`, `cloudConfig.userAppsKey`, `cloudConfig.userAppsPortRange`, and the three manifest entries with access class `user-apps-key`.

- [ ] **Step 1: Write the tests**

**Modify `src/api/routes.manifest.test.ts`:**

````diff
--- a/src/api/routes.manifest.test.ts
+++ b/src/api/routes.manifest.test.ts
@@ -327,6 +327,21 @@ describe("API route manifest", () => {
         path: "/api/v1/auth/me",
         description: "Validate a portal session token and return the authenticated user",
       },
+      {
+        method: "PUT",
+        path: "/api/v1/user-apps/:slug",
+        description: "Publish or move an app a member deployed (user-apps key only)",
+      },
+      {
+        method: "DELETE",
+        path: "/api/v1/user-apps/:slug",
+        description: "Remove an app a member deployed (user-apps key only)",
+      },
+      {
+        method: "GET",
+        path: "/api/v1/user-apps/:slug/availability",
+        description: "Whether a name is already used on this node (user-apps key only)",
+      },
     ];
     const actual = apiRouteManifest.map((route) => ({ ...route })) as any[];
     expect(actual).toEqual(expected as any[]);
````

**Create `src/test/user-apps-http.test.ts`:**

````typescript
import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { createSystemsApiTestHarness, emptySystemsApiRegistry } from "./systems-api-harness";

const KEY = "user-apps-test-key";
const NODE_KEY = "node-key";

describe("/api/v1/user-apps", () => {
  let h: Awaited<ReturnType<typeof createSystemsApiTestHarness>>;
  beforeEach(async () => {
    h = await createSystemsApiTestHarness(emptySystemsApiRegistry);
    process.env.NEXUS_CLOUD_USER_APPS_KEY = KEY;
    process.env.NEXUS_CLOUD_API_KEY = NODE_KEY;
    delete process.env.NEXUS_CLOUD_USER_APPS_PORTS;
  });
  afterEach(() => {
    delete process.env.NEXUS_CLOUD_USER_APPS_KEY;
    process.env.NEXUS_CLOUD_API_KEY = "";
    h.cleanup();
  });

  const call = (method: string, path: string, opts: { key?: string; body?: unknown } = {}) =>
    h.handleRequest(
      new Request(`http://127.0.0.1:8787${path}`, {
        method,
        headers: {
          ...(opts.key ? { authorization: `Bearer ${opts.key}` } : {}),
          ...(opts.body !== undefined ? { "content-type": "application/json" } : {}),
        },
        ...(opts.body !== undefined ? { body: JSON.stringify(opts.body) } : {}),
      }),
    );

  test("without the user-apps key every method is refused, including the node key", async () => {
    for (const key of [undefined, "wrong", NODE_KEY]) {
      const res = await call("PUT", "/api/v1/user-apps/my-app", {
        ...(key ? { key } : {}),
        body: { upstreamUrl: "http://127.0.0.1:20001" },
      });
      expect(res.status).toBe(401);
    }
    expect((await call("DELETE", "/api/v1/user-apps/my-app", { key: NODE_KEY })).status).toBe(401);
    expect((await call("GET", "/api/v1/user-apps/my-app/availability")).status).toBe(401);
  });

  test("with no key configured the feature is off, not open", async () => {
    delete process.env.NEXUS_CLOUD_USER_APPS_KEY;
    const res = await call("PUT", "/api/v1/user-apps/my-app", {
      key: "anything",
      body: { upstreamUrl: "http://127.0.0.1:20001" },
    });
    expect(res.status).toBe(503);
  });

  test("publish, move and remove an app", async () => {
    const put = (port: number) =>
      call("PUT", "/api/v1/user-apps/my-app", {
        key: KEY,
        body: { upstreamUrl: `http://127.0.0.1:${port}` },
      });
    expect((await put(20001)).status).toBe(200);
    expect((await put(20002)).status).toBe(200);
    const routes = h.systemsApiService
      .listSystemsApiRoutes()
      .filter((r) => r.domain === "my-app.nexus.local");
    expect(routes).toHaveLength(1);
    expect(routes[0]?.upstream).toBe("http://127.0.0.1:20002");
    expect(routes[0]?.userDeployed).toBe(true);

    expect((await call("DELETE", "/api/v1/user-apps/my-app", { key: KEY })).status).toBe(204);
    expect(
      h.systemsApiService.listSystemsApiRoutes().some((r) => r.domain === "my-app.nexus.local"),
    ).toBe(false);
    expect((await call("DELETE", "/api/v1/user-apps/my-app", { key: KEY })).status).toBe(404);
  });

  test("an upstream outside loopback or the port range is refused", async () => {
    for (const upstreamUrl of [
      "http://127.0.0.1:4310",
      "http://127.0.0.1:19999",
      "http://127.0.0.1:30000",
      "http://10.0.0.5:20001",
      "https://127.0.0.1:20001",
      "http://127.0.0.1:20001/path",
      "http://localhost:20001",
      42,
    ]) {
      const res = await call("PUT", "/api/v1/user-apps/my-app", { key: KEY, body: { upstreamUrl } });
      expect(res.status, JSON.stringify(upstreamUrl)).toBe(400);
    }
  });

  test("the port range is configurable", async () => {
    process.env.NEXUS_CLOUD_USER_APPS_PORTS = "40000-40010";
    const ok = await call("PUT", "/api/v1/user-apps/my-app", {
      key: KEY,
      body: { upstreamUrl: "http://127.0.0.1:40005" },
    });
    expect(ok.status).toBe(200);
    const bad = await call("PUT", "/api/v1/user-apps/other", {
      key: KEY,
      body: { upstreamUrl: "http://127.0.0.1:20001" },
    });
    expect(bad.status).toBe(400);
  });

  test("a malformed name is refused", async () => {
    for (const slug of ["A", "ab", "-x-", "a--b", "x".repeat(41), "bad_name"]) {
      const res = await call("PUT", `/api/v1/user-apps/${slug}`, {
        key: KEY,
        body: { upstreamUrl: "http://127.0.0.1:20001" },
      });
      expect(res.status, slug).toBe(400);
    }
  });

  test("a name held by an ecosystem tool is a 409 and the tool's route is untouched", async () => {
    h.systemsApiService.registerSystemsApiTool({
      id: "draw",
      name: "Draw",
      description: "Ecosystem app",
      upstreamUrl: "http://127.0.0.1:8091",
      exposed: false,
      health: "healthy",
      capabilities: [],
    });
    h.systemsApiService.requestSystemsApiExposure({ toolId: "draw", desiredHost: "draw.nexus.local" });

    const res = await call("PUT", "/api/v1/user-apps/draw", {
      key: KEY,
      body: { upstreamUrl: "http://127.0.0.1:20001" },
    });
    expect(res.status).toBe(409);
    const route = h.systemsApiService.listSystemsApiRoutes().find((r) => r.domain === "draw.nexus.local");
    expect(route?.upstream).toBe("http://127.0.0.1:8091");
    expect(route?.userDeployed).toBe(false);
  });

  test("availability reports whether a name is already used", async () => {
    const free = await call("GET", "/api/v1/user-apps/fresh-name/availability", { key: KEY });
    expect(await free.json()).toEqual({ slug: "fresh-name", host: "fresh-name.nexus.local", taken: false });

    h.systemsApiService.registerSystemsApiTool({
      id: "calendar",
      name: "Calendar",
      description: "Ecosystem app",
      upstreamUrl: "http://127.0.0.1:8092",
      exposed: false,
      health: "healthy",
      capabilities: [],
    });
    h.systemsApiService.requestSystemsApiExposure({ toolId: "calendar", desiredHost: "calendar.nexus.local" });
    const taken = await call("GET", "/api/v1/user-apps/calendar/availability", { key: KEY });
    expect(((await taken.json()) as { taken: boolean }).taken).toBe(true);

    // The app's own earlier registration does not count against it.
    await call("PUT", "/api/v1/user-apps/mine", { key: KEY, body: { upstreamUrl: "http://127.0.0.1:20009" } });
    const own = await call("GET", "/api/v1/user-apps/mine/availability", { key: KEY });
    expect(((await own.json()) as { taken: boolean }).taken).toBe(false);
  });

  test("DELETE refuses to remove an ecosystem tool", async () => {
    h.systemsApiService.registerSystemsApiTool({
      id: "plain",
      name: "Plain",
      description: "Ecosystem tool",
      upstreamUrl: "http://127.0.0.1:4200",
      exposed: false,
      health: "healthy",
      capabilities: [],
    });
    expect((await call("DELETE", "/api/v1/user-apps/plain", { key: KEY })).status).toBe(404);
    expect(h.systemsApiService.getSystemsApiTool("plain")).not.toBeNull();
  });
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Cloud
PORT=18787 bun test src/test/user-apps-http.test.ts src/api/routes.manifest.test.ts
```

Expected: FAIL — `src/test/user-apps-http.test.ts` fails because the paths answer 404; `src/api/routes.manifest.test.ts` is updated in the same step and passes only once the routes exist.

- [ ] **Step 3: Implement**

**Modify `src/api/cloud-control-contract.ts`:**

````diff
--- a/src/api/cloud-control-contract.ts
+++ b/src/api/cloud-control-contract.ts
@@ -6,7 +6,8 @@ export type CloudContractAccess =
   | "cloud-api-key"
   | "platform-permission"
   | "session-or-bearer"
-  | "deploy-token";
+  | "deploy-token"
+  | "user-apps-key";
 
 export type CloudContractRoute = ApiRoute & {
   access: CloudContractAccess;
@@ -50,6 +51,7 @@ const PUBLIC_GET_PATHS = new Set([
 
 function accessForRoute(route: ApiRoute): CloudContractAccess {
   if (route.path === "/api/v1/contract") return "public";
+  if (route.path.startsWith("/api/v1/user-apps/")) return "user-apps-key";
   if (route.path.includes("/catalog/:toolId/") && route.method === "POST") {
     return "platform-permission";
   }
````

**Modify `src/api/handlers.ts`:**

````diff
--- a/src/api/handlers.ts
+++ b/src/api/handlers.ts
@@ -11,6 +11,7 @@ import {
   tunnelTarget,
 } from "../cloudflare-dns";
 import { cloudConfig, isValidApiKey, requiresApiKey } from "../config";
+import { handleUserAppsRequest } from "./user-apps";
 import { buildCloudControlContract } from "./cloud-control-contract";
 import { exploreFallbackCatalog, validateCatalogProposal, validateApprovedCatalog } from "../systems-api/catalog";
 import { controlPlane, controlPlaneService } from "../control-plane";
@@ -1905,6 +1906,11 @@ export async function handleApiRequest(request: Request): Promise<Response> {
     return handleStoragePoolDelete(pathname);
   }
 
+  // Apps members deploy through Nexus Deploy. Guarded by their own credential,
+  // so it must run before the node-API-key gate further down.
+  const userApps = await handleUserAppsRequest(request, pathname);
+  if (userApps) return userApps;
+
   // Subdomain proxy: *.cloudDomain requests are routed to the registered upstream.
   // This runs before CORS and auth so the upstream handles its own CORS/auth headers.
   const host = (request.headers.get("host") ?? "").toLowerCase().split(":")[0] ?? "";
````

**Modify `src/api/routes.ts`:**

````diff
--- a/src/api/routes.ts
+++ b/src/api/routes.ts
@@ -299,6 +299,21 @@ export const apiRouteManifest = [
     path: "/api/v1/auth/me",
     description: "Validate a portal session token and return the authenticated user",
   },
+  {
+    method: "PUT",
+    path: "/api/v1/user-apps/:slug",
+    description: "Publish or move an app a member deployed (user-apps key only)",
+  },
+  {
+    method: "DELETE",
+    path: "/api/v1/user-apps/:slug",
+    description: "Remove an app a member deployed (user-apps key only)",
+  },
+  {
+    method: "GET",
+    path: "/api/v1/user-apps/:slug/availability",
+    description: "Whether a name is already used on this node (user-apps key only)",
+  },
 ] as const satisfies readonly ApiRoute[];
 
 export function getApiRoute(path: string): ApiRoute | null {
````

**Create `src/api/user-apps.ts`:**

````typescript
import { timingSafeEqual } from "node:crypto";
import { cloudConfig } from "../config";
import { systemsApiService } from "../systems-api";

/**
 * Publishing and removing the apps members deploy through Nexus Deploy.
 *
 * These routes are guarded by their own credential, not the node API key. The
 * route a publish creates carries the proxy's user-site isolation, and only the
 * holder of this credential may create one, move one, or remove one.
 */

/** Same shape Deploy enforces; checked again here because this is the trust boundary. */
const SLUG_RE = /^(?!.*--)[a-z][a-z0-9-]{1,38}[a-z0-9]$/;

function json(data: unknown, status = 200): Response {
  return new Response(JSON.stringify(data), {
    status,
    headers: { "content-type": "application/json" },
  });
}

function sameSecret(given: string, expected: string): boolean {
  const a = Buffer.from(given);
  const b = Buffer.from(expected);
  return a.length === b.length && timingSafeEqual(a, b);
}

function portRange(): { min: number; max: number } {
  const match = /^(\d{1,5})-(\d{1,5})$/.exec(cloudConfig.userAppsPortRange);
  if (!match) return { min: 20000, max: 29999 };
  return { min: Number(match[1]), max: Number(match[2]) };
}

/** Loopback only, plain http, inside the configured port range. */
export function isAllowedUserAppUpstream(value: unknown): value is string {
  if (typeof value !== "string") return false;
  const match = /^http:\/\/127\.0\.0\.1:(\d{1,5})$/.exec(value);
  if (!match) return false;
  const port = Number(match[1]);
  const { min, max } = portRange();
  return port >= min && port <= max;
}

function authorize(request: Request): Response | null {
  const expected = cloudConfig.userAppsKey;
  // Not configured means the feature is off, never open.
  if (!expected) return json({ error: "User apps are not configured" }, 503);
  const header = request.headers.get("authorization");
  const token = header?.startsWith("Bearer ") ? header.slice(7).trim() : "";
  if (!token || !sameSecret(token, expected)) return json({ error: "Unauthorized" }, 401);
  return null;
}

/** Returns a Response for /api/v1/user-apps/*, or null for any other path. */
export async function handleUserAppsRequest(
  request: Request,
  pathname: string,
): Promise<Response | null> {
  const match = /^\/api\/v1\/user-apps\/([^/]+?)(\/availability)?$/.exec(pathname);
  if (!match) return null;

  const denied = authorize(request);
  if (denied) return denied;

  const slug = decodeURIComponent(match[1] ?? "");
  if (!SLUG_RE.test(slug)) return json({ error: "Invalid app name" }, 400);
  const host = `${slug}.${cloudConfig.cloudDomain}`;

  if (match[2]) {
    if (request.method !== "GET") return json({ error: "Method not allowed" }, 405);
    const taken = systemsApiService.isHostInUse(host, `deploy-${slug}`);
    return json({ slug, host, taken });
  }

  if (request.method === "PUT") {
    let body: unknown = null;
    try {
      body = await request.json();
    } catch {
      return json({ error: "Invalid JSON body" }, 400);
    }
    const upstreamUrl = (body as { upstreamUrl?: unknown } | null)?.upstreamUrl;
    if (!isAllowedUserAppUpstream(upstreamUrl)) {
      return json({ error: "upstreamUrl must be http://127.0.0.1:<port> inside the allowed range" }, 400);
    }
    const result = systemsApiService.publishUserApp({ slug, upstreamUrl, host });
    if (!result.ok) {
      return result.reason === "conflict"
        ? json({ error: "That name is already in use" }, 409)
        : json({ error: "That name was refused" }, 403);
    }
    return json({ slug, host, upstreamUrl, health: result.tool.health });
  }

  if (request.method === "DELETE") {
    return systemsApiService.unpublishUserApp(slug)
      ? new Response(null, { status: 204 })
      : json({ error: "Not found" }, 404);
  }

  return json({ error: "Method not allowed" }, 405);
}
````

**Modify `src/config.ts`:**

````diff
--- a/src/config.ts
+++ b/src/config.ts
@@ -8,6 +8,17 @@ export const cloudConfig = {
   get apiKey(): string {
     return process.env.NEXUS_CLOUD_API_KEY ?? "";
   },
+  // Credential for the apps-deployed-by-members endpoints. Deliberately not the
+  // node API key: that one is held by every ecosystem service and must not be
+  // able to publish a route that carries user-site isolation, or to clear it.
+  get userAppsKey(): string {
+    return process.env.NEXUS_CLOUD_USER_APPS_KEY?.trim() ?? "";
+  },
+  // Loopback ports a deployed app may be routed to. A leaked credential must not
+  // be able to aim a public hostname at Auth, Cloud or a database.
+  get userAppsPortRange(): string {
+    return process.env.NEXUS_CLOUD_USER_APPS_PORTS?.trim() || "20000-29999";
+  },
   get corsOrigin(): string {
     return process.env.CORS_ORIGIN ?? "*";
   },
````

- [ ] **Step 4: Run the checks**

```bash
cd apps/Nexus-Cloud
node node_modules/typescript/bin/tsc --noEmit -p tsconfig.json
PORT=18787 bun test src/systems-api src/test src/api
```

Expected: `tsc` prints nothing new; **132 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Cloud
git add src/api/cloud-control-contract.ts src/api/handlers.ts src/api/routes.manifest.test.ts src/api/routes.ts src/api/user-apps.ts src/config.ts src/test/user-apps-http.test.ts
git commit \
  -m "feat(user-apps): publish, move and remove member apps behind their own credential" \
  -m "PUT/DELETE/GET availability under /api/v1/user-apps/:slug, guarded by
NEXUS_CLOUD_USER_APPS_KEY. The node API key is refused, an unconfigured key means
503, and the upstream must be loopback inside a configured port range." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 3: Proxy: a member's app is public, isolated, and carries no visitor tag

`buildRouteMap` reads the new route flag with strict equality. A route with `userDeployed === true` becomes `kind: "site"` (public, like Hosting sites, whatever `requiresAuth` says) and gets the same request and response isolation Hosting sites already get: no `Cookie` or `Authorization` forwarded, `Domain` cookies dropped. Beyond that, a member's app gets no `x-nexus-client-tag` either, over HTTP and over WebSocket, so the owner's code learns nothing about the visitor. Ordinary routes are unchanged, and a test pins that.

**Repository:** `deploy/production` (the monorepo). Paths below are relative to it.

**Files:**

- Modify: `proxy.ts`
- Create: `tests/user-apps.test.ts`

**Interfaces:**

- Consumes: `userDeployed` on Cloud's `/api/v1/routes` rows (Tasks 1–2).
- Produces: `NexusCloudRoute.userDeployed?`, `RouteTarget.userDeployed?`, `WsProxyData.omitClientTag?`.

- [ ] **Step 1: Write the tests**

**Create `tests/user-apps.test.ts`:**

````typescript
import { afterEach, beforeAll, describe, expect, it } from "bun:test";

beforeAll(async () => {
  // These tests exercise forwarding and isolation, not the login gate.
  process.env.GATE_SKIP_AUTH = "true";
  const { __setMaintenanceForTest } = await import("../proxy");
  __setMaintenanceForTest([]);
});

afterEach(async () => {
  const { __setRoutesForTest } = await import("../proxy");
  __setRoutesForTest({});
});

type Seen = Record<string, string | null>;

function listen(onRequest: (req: Request) => Response | void) {
  return Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch(req) {
      return onRequest(req) ?? new Response("ok");
    },
  });
}

describe("routes Cloud marks as a member's app", () => {
  it("become public, isolated routes, and a mistyped flag does not", async () => {
    const { buildRouteMap } = await import("../proxy");
    const map = buildRouteMap([
      { domain: "mine.tnhc.dev", upstream: "http://127.0.0.1:20001", requiresAuth: true, userDeployed: true },
      { domain: "typo.tnhc.dev", upstream: "http://127.0.0.1:20002", userDeployed: "true" as never },
      { domain: "draw.tnhc.dev", upstream: "http://127.0.0.1:8091", requiresAuth: true },
    ]);
    expect(map["mine.tnhc.dev"]).toMatchObject({ kind: "site", userDeployed: true, requiresAuth: false });
    expect(map["typo.tnhc.dev"]).toMatchObject({ kind: "app" });
    expect(map["typo.tnhc.dev"]?.userDeployed).toBeUndefined();
    expect(map["draw.tnhc.dev"]).toMatchObject({ kind: "app", requiresAuth: true });
  });

  it("never see the visitor's cookie, credentials, address or tag, and cannot plant a parent-domain cookie", async () => {
    let seen: Seen = {};
    const app = listen((req) => {
      seen = {
        cookie: req.headers.get("cookie"),
        authorization: req.headers.get("authorization"),
        tag: req.headers.get("x-nexus-client-tag"),
        cfIp: req.headers.get("cf-connecting-ip"),
        xff: req.headers.get("x-forwarded-for"),
        identity: req.headers.get("x-nexus-identity"),
        subject: req.headers.get("x-nexus-subject"),
        host: req.headers.get("x-forwarded-host"),
      };
      const headers = new Headers({ "content-type": "text/plain" });
      headers.append("set-cookie", "nexus_session=planted; Domain=.tnhc.dev; Path=/");
      headers.append("set-cookie", "theme=dark; Path=/");
      return new Response("hello", { headers });
    });
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([{ domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true }]),
      );
      const res = await handleRequest(
        new Request("http://mine.tnhc.dev/page", {
          headers: {
            cookie: "nexus_session=victim",
            authorization: "Bearer victim",
            "cf-connecting-ip": "203.0.113.77",
            "x-forwarded-for": "203.0.113.77",
            "x-nexus-identity": "forged",
            "x-nexus-subject": "forged",
          },
        }),
      );
      expect(res.status).toBe(200);
      expect(seen.cookie).toBeNull();
      expect(seen.authorization).toBeNull();
      expect(seen.tag).toBeNull();
      expect(seen.cfIp).toBeNull();
      expect(seen.xff).toBeNull();
      expect(seen.identity).toBeNull();
      expect(seen.subject).toBeNull();
      expect(seen.host).toBe("mine.tnhc.dev");
      expect(res.headers.getSetCookie()).toEqual(["theme=dark; Path=/"]);
    } finally {
      app.stop(true);
    }
  });

  it("are reachable without a session even when the gate is on", async () => {
    const app = listen(() => new Response("public app"));
    delete process.env.GATE_SKIP_AUTH;
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([{ domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true }]),
      );
      const res = await handleRequest(new Request("http://mine.tnhc.dev/"));
      expect(res.status).toBe(200);
      expect(await res.text()).toBe("public app");
    } finally {
      process.env.GATE_SKIP_AUTH = "true";
      app.stop(true);
    }
  });

  it("may be called with a same-site mutation, which only ecosystem apps refuse", async () => {
    const app = listen(() => new Response("created", { status: 201 }));
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([{ domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true }]),
      );
      const res = await handleRequest(
        new Request("http://mine.tnhc.dev/api", { method: "POST", headers: { "sec-fetch-site": "same-site" }, body: "{}" }),
      );
      expect(res.status).toBe(201);
    } finally {
      app.stop(true);
    }
  });

  it("do not change what an ordinary registered app receives", async () => {
    let seen: Seen = {};
    const app = listen((req) => {
      seen = { cookie: req.headers.get("cookie"), tag: req.headers.get("x-nexus-client-tag") };
    });
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(buildRouteMap([{ domain: "draw.tnhc.dev", upstream: `http://127.0.0.1:${app.port}` }]));
      await handleRequest(new Request("http://draw.tnhc.dev/", { headers: { cookie: "nexus_session=me", "cf-connecting-ip": "203.0.113.5" } }));
      expect(seen.cookie).toBe("nexus_session=me");
      expect(seen.tag).toMatch(/^[A-Za-z0-9_-]{22}$/);
    } finally {
      app.stop(true);
    }
  });

  it("get a WebSocket upgrade that carries no visitor tag", async () => {
    const app = listen(() => new Response("ok"));
    try {
      const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
      __setRoutesForTest(
        buildRouteMap([
          { domain: "mine.tnhc.dev", upstream: `http://127.0.0.1:${app.port}`, userDeployed: true },
          { domain: "draw.tnhc.dev", upstream: `http://127.0.0.1:${app.port}` },
        ]),
      );
      const upgrades: Array<{ omitClientTag?: boolean }> = [];
      const server = {
        upgrade: (_req: Request, opts: { data: { omitClientTag?: boolean } }) => {
          upgrades.push(opts.data);
          return true;
        },
      };
      const ws = (host: string) =>
        new Request(`http://${host}/socket`, { headers: { upgrade: "websocket", connection: "Upgrade" } });
      await handleRequest(ws("mine.tnhc.dev"), server as never);
      await handleRequest(ws("draw.tnhc.dev"), server as never);
      expect(upgrades[0]?.omitClientTag).toBe(true);
      expect(upgrades[1]?.omitClientTag).toBeUndefined();
    } finally {
      app.stop(true);
    }
  });
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd deploy/production
bun test tests/user-apps.test.ts
```

Expected: FAIL — `buildRouteMap` returns `kind: "app"` for a flagged route, so the isolation tests fail.

- [ ] **Step 3: Implement**

**Modify `proxy.ts`:**

````diff
--- a/proxy.ts
+++ b/proxy.ts
@@ -29,6 +29,12 @@ interface NexusCloudRoute {
    * missing or misspelled field can never lock an app's users out.
    */
   requiresAuth?: boolean;
+  /**
+   * True for an app a member deployed through Nexus Deploy. Cloud sets it only
+   * for registrations that arrive with the user-apps credential, so an app can
+   * neither choose nor clear it. Read with strict equality, like requiresAuth.
+   */
+  userDeployed?: boolean;
   /** Present when an operator has this part in maintenance; the proxy serves
    *  the maintenance page instead of proxying. */
   maintenance?: { part: string; reason?: string; startedAt: string; expectedEndAt?: string };
@@ -58,6 +64,12 @@ export interface RouteTarget {
    * addressing.
    */
   kind: "app" | "site";
+  /**
+   * A member's app: public like a site, and isolated like one (no cookies or
+   * credentials forwarded, parent-domain cookies dropped), and additionally
+   * given no visitor tag, so the owner's code learns nothing about who called.
+   */
+  userDeployed?: boolean;
   /** Operator-set maintenance window for this part, if any. When present the
    *  proxy serves the maintenance page in place of the upstream. */
   maintenance?: { part: string; reason?: string; startedAt: string; expectedEndAt?: string };
@@ -154,14 +166,18 @@ export function buildRouteMap(routes: NexusCloudRoute[]): Record<string, RouteTa
       // unconditionally yields "http://http://host:port", whose URL hostname
       // parses as "http".
       const domain = route.domain.toLowerCase().replace(/^www\./, "");
+      // Strict equality, as for requiresAuth: only a real boolean true isolates.
+      const userDeployed = route.userDeployed === true;
       map[domain] = {
         upstream: normalizeUpstream(route.upstream),
         // Strict equality, not truthiness: only a real boolean true gates a
         // host, so a stray string or a typo cannot silently lock people out.
-        requiresAuth: route.requiresAuth === true,
-        // Anything Cloud has a route row for is a registered application. Sites
-        // never appear here — they are reached through the wildcard fallback.
-        kind: "app",
+        // A member's app is never gated: like a site, it is public by design.
+        requiresAuth: !userDeployed && route.requiresAuth === true,
+        // Anything Cloud has a route row for is a registered application, except
+        // a member's app, which is a public site that happens to be routed by Cloud.
+        kind: userDeployed ? "site" : "app",
+        ...(userDeployed ? { userDeployed: true } : {}),
         ...(route.maintenance ? { maintenance: route.maintenance } : {}),
       };
     }
@@ -284,6 +300,8 @@ export interface WsProxyData {
   upstreamPathname?: string;
   /** Forgetful per-visitor tag (never an address) for the upstream. */
   clientTag: string;
+  /** A member's app gets no tag at all: its owner's code learns nothing about the visitor. */
+  omitClientTag?: boolean;
   /**
    * Browser credentials for one tightly-scoped hop only. Presence means the
    * request was the exact app.<DOMAIN> terminal attach route and upstreamUrl
@@ -685,6 +703,7 @@ async function handleRequestInner(
     // rather than assumed, so the route table stays the thing that decides and
     // the value cannot quietly become decorative.
     let matchedKind: RouteTarget["kind"] | null = null;
+    let matchedUserApp = false;
 
     // Cloud keys its routing table by full hostname, and buildRouteMap
     // lowercases and strips a leading "www." from both sides, so a single
@@ -700,6 +719,7 @@ async function handleRequestInner(
         upstreamUrl = matched.upstream;
         requiresAuth = matched.requiresAuth;
         matchedKind = matched.kind;
+        matchedUserApp = matched.userDeployed === true;
       }
     }
 
@@ -787,7 +807,8 @@ async function handleRequestInner(
     let kind: RouteTarget["kind"] = fixedKind ?? matchedKind ?? "app";
     // Content deployed by a user, as opposed to storage or Supabase, which are
     // also ungated but authenticate requests themselves. See isolateUserSite*.
-    let isUserSite = false;
+    // A member's app gets the same isolation as a Hosting site.
+    let isUserSite = matchedUserApp;
     if (!upstreamUrl) {
       upstreamUrl = hostingSiteUpstream();
       kind = "site";
@@ -852,6 +873,7 @@ async function handleRequestInner(
         requestUrl: req.url,
         upstreamPathname: translatedPath,
         clientTag: tagFromRequestHeaders(req.headers),
+        ...(matchedUserApp ? { omitClientTag: true } : {}),
       };
       if (dashboardTerminalRequest) {
         const dashboardTerminalHeaders: NonNullable<WsProxyData["dashboardTerminalHeaders"]> = {};
@@ -892,7 +914,7 @@ async function handleRequestInner(
         tag = tagFromRequestHeaders(req.headers);
         stripAddressHeaders(forwardHeaders);
         forwardHeaders.delete(CLIENT_TAG_HEADER);
-        forwardHeaders.set(CLIENT_TAG_HEADER, tag);
+        if (!matchedUserApp) forwardHeaders.set(CLIENT_TAG_HEADER, tag);
       } catch {
         return new Response("Request refused", { status: 500 });
       }
@@ -1077,7 +1099,7 @@ export function startProxy() {
 
         const headers: Record<string, string> = {};
         if (identityToken) headers["x-nexus-identity"] = identityToken;
-        headers[CLIENT_TAG_HEADER] = ws.data.clientTag;
+        if (!ws.data.omitClientTag) headers[CLIENT_TAG_HEADER] = ws.data.clientTag;
         if (dashboardTerminalHeaders?.cookie !== undefined) {
           headers.cookie = dashboardTerminalHeaders.cookie;
         }
````

- [ ] **Step 4: Run the checks**

```bash
cd deploy/production
bun test tests/user-apps.test.ts tests/proxy.test.ts tests/gate.test.ts tests/client-tag.test.ts
```

Expected: **111 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd deploy/production
git add proxy.ts tests/user-apps.test.ts
git commit \
  -m "feat(proxy): isolate routes Cloud marks as member apps" \
  -m "A userDeployed route is public like a site, loses Cookie, Authorization and the
visitor tag, and cannot set a parent-domain cookie. Ordinary routes are unchanged." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 4: Proxy: deploy.<domain> reaches Deploy, and deploy.<domain>/<name> redirects to the app

`deploy.<domain>` is Deploy's control plane. It authenticates itself (its API checks the session with Auth, its pages send browsers to sign in, and GitHub's push webhook has no session and proves itself with a signed body), so it joins the public-host allowlist next to Hosting and Auth. It stays `kind: "app"`, so the same-site mutation block still protects it from a member's app. The short link answers only for a name Cloud currently routes as a member's app; every other path, including every dashboard path, goes to the control plane. The upstream is read at request time so tests can point it elsewhere. A mutation check confirms the allowlist test really guards: removing the host from the set makes it fail.

**Repository:** `deploy/production` (the monorepo). Paths below are relative to it.

**Files:**

- Modify: `gate.ts`
- Modify: `proxy.ts`
- Modify: `tests/user-apps.test.ts`

**Interfaces:**

- Consumes: Task 3's `userDeployed` route targets.
- Produces: `DEPLOY_HOST`, `deployUpstream()` (env `DEPLOY_UPSTREAM`, default `http://127.0.0.1:8096`; host override `NEXUS_DEPLOY_HOST`), the short-link redirect, and `deploy.<domain>` in the gate's public hosts.

- [ ] **Step 1: Write the tests**

**Modify `tests/user-apps.test.ts`:**

````diff
--- a/tests/user-apps.test.ts
+++ b/tests/user-apps.test.ts
@@ -165,3 +165,103 @@ describe("routes Cloud marks as a member's app", () => {
     }
   });
 });
+
+describe("deploy.<domain>", () => {
+  const originalGate = process.env.GATE_SKIP_AUTH;
+
+  it("redirects /<name> to a member's app, and only to one", async () => {
+    const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
+    const control = listen(() => new Response("control plane"));
+    process.env.DEPLOY_UPSTREAM = `http://127.0.0.1:${control.port}`;
+    try {
+      __setRoutesForTest(
+        buildRouteMap([
+          { domain: "mine.tnhc.dev", upstream: "http://127.0.0.1:20001", userDeployed: true },
+          { domain: "draw.tnhc.dev", upstream: "http://127.0.0.1:8091" },
+        ]),
+      );
+      const hit = await handleRequest(new Request("http://deploy.tnhc.dev/mine"));
+      expect(hit.status).toBe(302);
+      expect(hit.headers.get("location")).toBe("https://mine.tnhc.dev/");
+      const slash = await handleRequest(new Request("http://deploy.tnhc.dev/mine/"));
+      expect(slash.headers.get("location")).toBe("https://mine.tnhc.dev/");
+
+      // An ecosystem app is not a member's app: no redirect, the control plane answers.
+      const eco = await handleRequest(new Request("http://deploy.tnhc.dev/draw"));
+      expect(eco.status).toBe(200);
+      expect(await eco.text()).toBe("control plane");
+      // A name nothing routes, a deeper path, and a POST all reach the control plane.
+      for (const [path, method] of [["/unknown", "GET"], ["/mine/extra", "GET"], ["/mine", "POST"]] as const) {
+        const res = await handleRequest(new Request(`http://deploy.tnhc.dev${path}`, { method, ...(method === "POST" ? { body: "x" } : {}) }));
+        expect(res.status, `${method} ${path}`).toBe(200);
+      }
+    } finally {
+      delete process.env.DEPLOY_UPSTREAM;
+      control.stop(true);
+    }
+  });
+
+  it("does not redirect on any other host", async () => {
+    const { handleRequest, buildRouteMap, __setRoutesForTest } = await import("../proxy");
+    const site = listen(() => new Response("a site"));
+    process.env.HOSTING_SITE_UPSTREAM = `http://127.0.0.1:${site.port}`;
+    try {
+      __setRoutesForTest(buildRouteMap([{ domain: "mine.tnhc.dev", upstream: "http://127.0.0.1:20001", userDeployed: true }]));
+      const res = await handleRequest(new Request("http://elsewhere.tnhc.dev/mine"));
+      expect(res.status).toBe(200);
+    } finally {
+      delete process.env.HOSTING_SITE_UPSTREAM;
+      site.stop(true);
+    }
+  });
+
+  it("reaches the control plane without an ecosystem session, and with the visitor's cookie", async () => {
+    let seen: Seen = {};
+    const control = listen((req) => {
+      seen = { cookie: req.headers.get("cookie"), webhook: req.headers.get("x-hub-signature-256") };
+    });
+    process.env.DEPLOY_UPSTREAM = `http://127.0.0.1:${control.port}`;
+    delete process.env.GATE_SKIP_AUTH;
+    try {
+      const { handleRequest, __setRoutesForTest } = await import("../proxy");
+      __setRoutesForTest({});
+      const res = await handleRequest(
+        new Request("http://deploy.tnhc.dev/api/webhooks/github/abc", {
+          method: "POST",
+          headers: { "x-hub-signature-256": "sha256=deadbeef", cookie: "nexus_session=me" },
+          body: "{}",
+        }),
+      );
+      expect(res.status).toBe(200);
+      // Deploy authenticates the session itself, so it needs the cookie; GitHub's
+      // signature must arrive untouched.
+      expect(seen.cookie).toBe("nexus_session=me");
+      expect(seen.webhook).toBe("sha256=deadbeef");
+    } finally {
+      if (originalGate !== undefined) process.env.GATE_SKIP_AUTH = originalGate;
+      else process.env.GATE_SKIP_AUTH = "true";
+      delete process.env.DEPLOY_UPSTREAM;
+      control.stop(true);
+    }
+  });
+
+  it("is an application host: a same-site mutation from a member's app is refused", async () => {
+    const control = listen(() => new Response("should not be reached"));
+    process.env.DEPLOY_UPSTREAM = `http://127.0.0.1:${control.port}`;
+    try {
+      const { handleRequest, __setRoutesForTest } = await import("../proxy");
+      __setRoutesForTest({});
+      const res = await handleRequest(
+        new Request("http://deploy.tnhc.dev/api/projects", {
+          method: "POST",
+          headers: { "sec-fetch-site": "same-site", "content-type": "application/json" },
+          body: "{}",
+        }),
+      );
+      expect(res.status).toBe(403);
+    } finally {
+      delete process.env.DEPLOY_UPSTREAM;
+      control.stop(true);
+    }
+  });
+});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd deploy/production
bun test tests/user-apps.test.ts
```

Expected: FAIL — the redirect test gets the control plane's body instead of a 302, and the gate test is redirected to sign in.

- [ ] **Step 3: Implement**

**Modify `gate.ts`:**

````diff
--- a/gate.ts
+++ b/gate.ts
@@ -270,7 +270,12 @@ export async function resolveIdentity(cookie: string | null, audience: string):
 // site directory (/api/sites), and health probes must reach it unauthenticated.
 // The hosting app enforces auth on routes that need it.
 const HOSTING_HOST = process.env.NEXUS_HOSTING_HOST || `hosting.${DOMAIN}`;
-const PUBLIC_HOSTS = new Set([AUTH_HOST, API_HOST, HOSTING_HOST]);
+// deploy.<domain> likewise authenticates itself: its API checks the session with
+// Auth, its pages send a browser to sign in, and GitHub's push webhook has no
+// session at all and proves itself with a signed body. The proxy still treats it
+// as an application host (kind "app"), so the same-site mutation block applies.
+const DEPLOY_HOST = process.env.NEXUS_DEPLOY_HOST || `deploy.${DOMAIN}`;
+const PUBLIC_HOSTS = new Set([AUTH_HOST, API_HOST, HOSTING_HOST, DEPLOY_HOST]);
 
 /** Paths exempt from the auth gate on ANY host (deploy.sh probes these). */
 const PUBLIC_PATHS = new Set(["/health", "/health/live", "/health/ready"]);
````

**Modify `proxy.ts`:**

````diff
--- a/proxy.ts
+++ b/proxy.ts
@@ -88,6 +88,14 @@ const FALLBACK_HOSTING_UPSTREAM = process.env.HOSTING_UPSTREAM || "http://127.0.
 const FALLBACK_DRAW_UPSTREAM = process.env.DRAW_UPSTREAM || "http://127.0.0.1:8091";
 const FALLBACK_CALENDAR_UPSTREAM = process.env.CALENDAR_UPSTREAM || "http://127.0.0.1:8092";
 const FALLBACK_PROJECT_UPSTREAM = process.env.PROJECT_UPSTREAM || "http://127.0.0.1:8093";
+// deploy.<DOMAIN> is Nexus Deploy's control plane: its dashboard, its API and the
+// push-to-deploy webhook. The apps it runs are NOT here; they are reached on
+// their own hostnames through Cloud's routes.
+export const DEPLOY_HOST = process.env.NEXUS_DEPLOY_HOST || `deploy.${DOMAIN}`;
+// Read at request time so tests can point it elsewhere.
+function deployUpstream(): string { return process.env.DEPLOY_UPSTREAM || "http://127.0.0.1:8096"; }
+// A path that could be an app name: one lowercase DNS-style label.
+const SHORT_LINK_PATH = /^\/([a-z][a-z0-9-]{1,38}[a-z0-9])\/?$/;
 // auth.<DOMAIN> is the sign-in host. Every app redirects an unauthenticated
 // browser to https://auth.<DOMAIN>/login?redirect=..., so this is the one that
 // has to resolve or single sign-on has nowhere to happen.
@@ -691,7 +699,22 @@ async function handleRequestInner(
     }
 
     const routes = dashboardTerminalRequest || apiRequest ? {} : await getRoutes();
-    
+
+    // Short link: deploy.<DOMAIN>/<name> sends the visitor to a member's app.
+    // Only a name Cloud currently routes as a member's app redirects; any other
+    // path, including every dashboard path, goes on to the control plane. Names
+    // that collide with a dashboard path are reserved at creation, so an app can
+    // never shadow one.
+    if (host === DEPLOY_HOST && (req.method === "GET" || req.method === "HEAD")) {
+      const name = SHORT_LINK_PATH.exec(url.pathname)?.[1];
+      if (name && routes[`${name}.${DOMAIN}`]?.userDeployed === true) {
+        return new Response(null, {
+          status: 302,
+          headers: { location: `https://${name}.${DOMAIN}/`, "cache-control": "no-store" },
+        });
+      }
+    }
+
     // Find matching route
     let upstreamUrl: string | null = null;
     // Only a route Cloud explicitly marked can gate. Every fallback below
@@ -772,6 +795,9 @@ async function handleRequestInner(
       } else if (host === `project.${DOMAIN}` || host === `www.project.${DOMAIN}`) {
         upstreamUrl = FALLBACK_PROJECT_UPSTREAM;
         fixedKind = "app";
+      } else if (host === DEPLOY_HOST) {
+        upstreamUrl = deployUpstream();
+        fixedKind = "app";
       } else if (host === `auth.${DOMAIN}` || host === `www.auth.${DOMAIN}`) {
         upstreamUrl = FALLBACK_AUTH_UPSTREAM;
       } else if (host === DOMAIN || host === `www.${DOMAIN}`) {
````

- [ ] **Step 4: Run the checks**

```bash
cd deploy/production
bun test tests/user-apps.test.ts tests/proxy.test.ts tests/gate.test.ts tests/client-tag.test.ts
```

Expected: **115 tests, all passing**.

Then confirm the gate test really guards, by temporarily weakening the allowlist and watching it fail:

```bash
cd deploy/production
cp gate.ts /tmp/gate.ts.bak
sed -i 's/API_HOST, HOSTING_HOST, DEPLOY_HOST\]/API_HOST, HOSTING_HOST]/' gate.ts
bun test tests/user-apps.test.ts   # expect exactly 1 failure: 'reaches the control plane without an ecosystem session'
cp /tmp/gate.ts.bak gate.ts && rm /tmp/gate.ts.bak
bun test tests/user-apps.test.ts   # expect 0 failures again
```

- [ ] **Step 5: Commit**

```bash
cd deploy/production
git add gate.ts proxy.ts tests/user-apps.test.ts
git commit \
  -m "feat(proxy): route deploy.<domain> to Deploy and redirect /<name> to member apps" \
  -m "deploy.<domain> authenticates itself, so it joins the public-host allowlist, but it
stays an application host for the same-site mutation block. The short link answers
only for a name Cloud routes as a member's app." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 5: Deploy: the Cloud client, the route publisher and the name checks

`CloudPublisher` implements Plan 1's `RoutePublisher`. `publish` PUTs the loopback port with the user-apps credential; a 409 (name in use), any refusal, an unreachable Cloud or a slow one makes the release fail, which Plan 1's release already handles by keeping the previous release serving. `unpublish` treats an already-gone route as success. `refresh` re-announces live routes as the heartbeat and removes the ones whose app stopped. The name checks ask Cloud and Hosting and fail closed: anything that cannot be asked is not 'free'.

**Repository:** `apps/Nexus-Deploy` (its own git repository). Paths below are relative to it.

**Files:**

- Create: `src/lib/cloud.ts`
- Modify: `src/lib/config.ts`
- Create: `tests/cloud.test.ts`

**Interfaces:**

- Consumes: `RoutePublisher`, `PublishedRoute`, `NullPublisher` (Plan 1, `src/lib/publisher.ts`); the Cloud endpoints from Task 2.
- Produces: `CloudSettings`, `CloudError`, `CloudPublisher` (`publish`, `unpublish`, `restore`, `refresh`, `start`, `stop`), `cloudNameTaken`, `hostingNameTaken`, `makeTakenElsewhere`, and `config.cloudUrl`, `cloudUserAppsKey`, `cloudTimeoutMs`, `cloudHeartbeatMs`, `hostingSiteUrl`.

- [ ] **Step 1: Write the tests**

**Create `tests/cloud.test.ts`:**

````typescript
import assert from "node:assert/strict";
import http from "node:http";
import type { AddressInfo } from "node:net";
import { test } from "node:test";
import {
  CloudError,
  CloudPublisher,
  type CloudSettings,
  cloudNameTaken,
  hostingNameTaken,
  makeTakenElsewhere,
} from "../src/lib/cloud.js";

type Seen = { method: string; url: string; auth: string | undefined; body: string; host: string };

/** A stand-in server: records requests, answers with whatever `reply` says. */
async function serve(
  reply: (seen: Seen) => { status: number; body?: string } | "hang",
): Promise<{ url: string; seen: Seen[]; close: () => Promise<void> }> {
  const seen: Seen[] = [];
  const server = http.createServer((req, res) => {
    const chunks: Buffer[] = [];
    req.on("data", (c: Buffer) => chunks.push(c));
    req.on("end", () => {
      const entry: Seen = {
        method: req.method ?? "",
        url: req.url ?? "",
        auth: req.headers.authorization,
        body: Buffer.concat(chunks).toString("utf8"),
        host: req.headers.host ?? "",
      };
      seen.push(entry);
      const answer = reply(entry);
      if (answer === "hang") return;
      res.writeHead(answer.status, { "content-type": "application/json" });
      res.end(answer.body ?? "");
    });
  });
  await new Promise<void>((r) => server.listen(0, "127.0.0.1", r));
  const { port } = server.address() as AddressInfo;
  return {
    url: `http://127.0.0.1:${port}`,
    seen,
    close: () =>
      new Promise<void>((r) => {
        server.closeAllConnections();
        server.close(() => r());
      }),
  };
}

const settings = (url: string): CloudSettings => ({ url, key: "k-test", timeoutMs: 400 });
const route = (slug: string, hostPort: number) => ({
  projectId: `p-${slug}`,
  slug,
  host: `${slug}.example.test`,
  hostPort,
});

test("publishing PUTs the loopback port with the user-apps credential", async () => {
  const cloud = await serve(() => ({ status: 200, body: "{}" }));
  try {
    await new CloudPublisher(settings(cloud.url)).publish(route("my-app", 20007));
    assert.equal(cloud.seen.length, 1);
    assert.equal(cloud.seen[0]?.method, "PUT");
    assert.equal(cloud.seen[0]?.url, "/api/v1/user-apps/my-app");
    assert.equal(cloud.seen[0]?.auth, "Bearer k-test");
    assert.deepEqual(JSON.parse(cloud.seen[0]?.body ?? ""), {
      upstreamUrl: "http://127.0.0.1:20007",
    });
  } finally {
    await cloud.close();
  }
});

test("a name Cloud says is taken, any refusal, a dead Cloud and a slow Cloud all fail the publish", async () => {
  for (const [status, pattern] of [
    [409, /already in use/],
    [403, /HTTP 403/],
    [401, /HTTP 401/],
    [503, /HTTP 503/],
  ] as const) {
    const cloud = await serve(() => ({ status }));
    try {
      await assert.rejects(
        new CloudPublisher(settings(cloud.url)).publish(route("a-b", 20001)),
        (e) => {
          assert.ok(e instanceof CloudError);
          assert.match(e.message, pattern);
          assert.equal(e.status, status);
          return true;
        },
      );
    } finally {
      await cloud.close();
    }
  }
  await assert.rejects(
    new CloudPublisher(settings("http://127.0.0.1:1")).publish(route("a-b", 20001)),
    /could not be reached/,
  );
  const slow = await serve(() => "hang");
  try {
    await assert.rejects(
      new CloudPublisher(settings(slow.url)).publish(route("a-b", 20001)),
      /could not be reached/,
    );
  } finally {
    await slow.close();
  }
});

test("a publisher cannot be built without a URL or a key", () => {
  assert.throws(() => new CloudPublisher({ url: "", key: "k", timeoutMs: 1 }), /CLOUD_URL/);
  assert.throws(
    () => new CloudPublisher({ url: "http://x", key: "", timeoutMs: 1 }),
    /CLOUD_USER_APPS_KEY/,
  );
});

test("unpublishing DELETEs, and an already-gone route is fine", async () => {
  for (const status of [204, 404]) {
    const cloud = await serve(() => ({ status }));
    try {
      await new CloudPublisher(settings(cloud.url)).unpublish("my-app");
      assert.equal(cloud.seen[0]?.method, "DELETE");
      assert.equal(cloud.seen[0]?.url, "/api/v1/user-apps/my-app");
    } finally {
      await cloud.close();
    }
  }
  const broken = await serve(() => ({ status: 500 }));
  try {
    await assert.rejects(new CloudPublisher(settings(broken.url)).unpublish("my-app"), /HTTP 500/);
  } finally {
    await broken.close();
  }
});

test("refresh re-announces live routes and removes the ones that stopped", async () => {
  const cloud = await serve((s) => ({ status: s.method === "DELETE" ? 204 : 200 }));
  try {
    const publisher = new CloudPublisher(settings(cloud.url));
    await publisher.publish(route("alive", 20001));
    await publisher.publish(route("stopped", 20002));
    cloud.seen.length = 0;

    const failed = await publisher.refresh((slug) => slug === "alive");
    assert.equal(failed, 0);
    assert.deepEqual(cloud.seen.map((s) => `${s.method} ${s.url}`).sort(), [
      "DELETE /api/v1/user-apps/stopped",
      "PUT /api/v1/user-apps/alive",
    ]);

    // The stopped route is forgotten, so it is not touched again.
    cloud.seen.length = 0;
    await publisher.refresh(() => true);
    assert.deepEqual(
      cloud.seen.map((s) => s.url),
      ["/api/v1/user-apps/alive"],
    );
  } finally {
    await cloud.close();
  }
});

test("refresh counts failures instead of throwing, so one bad route does not stop the rest", async () => {
  let n = 0;
  const cloud = await serve(() => ({ status: n++ === 2 ? 500 : 200 }));
  try {
    const publisher = new CloudPublisher(settings(cloud.url));
    await publisher.publish(route("one", 20001));
    await publisher.publish(route("two", 20002));
    const failed = await publisher.refresh(() => true);
    assert.equal(failed, 1);
    assert.equal(cloud.seen.length, 4, "the second route was still announced");
  } finally {
    await cloud.close();
  }
});

test("restore announces earlier routes and reports how many failed", async () => {
  const cloud = await serve((s) => ({ status: s.url.endsWith("/bad") ? 409 : 200 }));
  try {
    const publisher = new CloudPublisher(settings(cloud.url));
    const failed = await publisher.restore([route("good", 20001), route("bad", 20002)]);
    assert.equal(failed, 1);
    assert.equal(cloud.seen.length, 2);
  } finally {
    await cloud.close();
  }
});

test("Cloud's availability answer is passed on, and anything unreadable is an error", async () => {
  for (const taken of [true, false]) {
    const cloud = await serve(() => ({ status: 200, body: JSON.stringify({ taken }) }));
    try {
      assert.equal(await cloudNameTaken(settings(cloud.url), "my-app"), taken);
      assert.equal(cloud.seen[0]?.url, "/api/v1/user-apps/my-app/availability");
    } finally {
      await cloud.close();
    }
  }
  for (const reply of [
    { status: 200, body: "not json" },
    { status: 200, body: JSON.stringify({ taken: "no" }) },
    { status: 500 },
  ]) {
    const cloud = await serve(() => reply);
    try {
      await assert.rejects(cloudNameTaken(settings(cloud.url), "my-app"), CloudError);
    } finally {
      await cloud.close();
    }
  }
});

const NO_SITE = "<h1>404</h1><p>No site found for <code>x</code></p>";

test("Hosting: its own 'No site found' 404 means the name is free; any site means taken", async () => {
  const free = await serve(() => ({ status: 404, body: NO_SITE }));
  const site = await serve(() => ({ status: 200, body: "<p>a site</p>" }));
  const privateSite = await serve(() => ({ status: 403, body: "private" }));
  const siteWithOwn404 = await serve(() => ({
    status: 404,
    body: "<p>this site has no page here</p>",
  }));
  try {
    assert.equal(await hostingNameTaken(free.url, "my-app.example.test", 400), false);
    assert.equal(await hostingNameTaken(site.url, "my-app.example.test", 400), true);
    assert.equal(await hostingNameTaken(privateSite.url, "my-app.example.test", 400), true);
    assert.equal(await hostingNameTaken(siteWithOwn404.url, "my-app.example.test", 400), true);
    assert.equal(free.seen[0]?.host, "my-app.example.test", "asks by the app's host");
  } finally {
    await Promise.all([free.close(), site.close(), privateSite.close(), siteWithOwn404.close()]);
  }
});

test("Hosting: a 5xx, a refused connection and a hang are 'could not check', not 'free'", async () => {
  const broken = await serve(() => ({ status: 502, body: "bad gateway" }));
  const slow = await serve(() => "hang");
  try {
    await assert.rejects(hostingNameTaken(broken.url, "a.example.test", 400), CloudError);
    await assert.rejects(hostingNameTaken("http://127.0.0.1:1", "a.example.test", 400));
    await assert.rejects(hostingNameTaken(slow.url, "a.example.test", 150), /in time/);
  } finally {
    await Promise.all([broken.close(), slow.close()]);
  }
});

test("the combined check is taken if any neighbour says so, and fails if any cannot be asked", async () => {
  const cloud = await serve(() => ({ status: 200, body: JSON.stringify({ taken: false }) }));
  const hosting = await serve(() => ({ status: 404, body: NO_SITE }));
  const takenHosting = await serve(() => ({ status: 200, body: "site" }));
  const hostOf = (slug: string) => `${slug}.example.test`;
  try {
    const free = makeTakenElsewhere({
      cloud: settings(cloud.url),
      hostingSiteUrl: hosting.url,
      hostOf,
      timeoutMs: 400,
    });
    assert.equal(await free("my-app"), false);

    const hostingTaken = makeTakenElsewhere({
      cloud: settings(cloud.url),
      hostingSiteUrl: takenHosting.url,
      hostOf,
      timeoutMs: 400,
    });
    assert.equal(await hostingTaken("my-app"), true);

    const deadHosting = makeTakenElsewhere({
      cloud: settings(cloud.url),
      hostingSiteUrl: "http://127.0.0.1:1",
      hostOf,
      timeoutMs: 400,
    });
    await assert.rejects(deadHosting("my-app"));

    const deadCloud = makeTakenElsewhere({
      cloud: settings("http://127.0.0.1:1"),
      hostOf,
      timeoutMs: 400,
    });
    await assert.rejects(deadCloud("my-app"));

    // Nothing configured: there is no neighbour to ask.
    assert.equal(await makeTakenElsewhere({ hostOf, timeoutMs: 400 })("my-app"), false);
  } finally {
    await Promise.all([cloud.close(), hosting.close(), takenHosting.close()]);
  }
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node --import tsx --test tests/cloud.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/cloud.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/cloud.ts`:**

````typescript
/**
 * Telling the ingress about live apps, and asking whether a name is free.
 *
 * On the reference node the ingress reads Nexus Cloud's registry, so announcing
 * an app means registering it there. Cloud, not this server, decides how the
 * route is treated: it marks it as a member's app, which the proxy isolates.
 */
import http from "node:http";
import type { PublishedRoute, RoutePublisher } from "./publisher.js";

export type CloudSettings = {
  /** Cloud's internal address, e.g. http://127.0.0.1:8787. */
  url: string;
  /** The user-apps credential. */
  key: string;
  timeoutMs: number;
};

export class CloudError extends Error {
  constructor(
    message: string,
    readonly status?: number,
  ) {
    super(message);
    this.name = "CloudError";
  }
}

function endpoint(settings: CloudSettings, slug: string, suffix = ""): string {
  return `${settings.url.replace(/\/+$/, "")}/api/v1/user-apps/${encodeURIComponent(slug)}${suffix}`;
}

async function request(
  settings: CloudSettings,
  method: string,
  url: string,
  body?: unknown,
): Promise<Response> {
  try {
    return await fetch(url, {
      method,
      headers: {
        authorization: `Bearer ${settings.key}`,
        accept: "application/json",
        ...(body !== undefined ? { "content-type": "application/json" } : {}),
      },
      ...(body !== undefined ? { body: JSON.stringify(body) } : {}),
      signal: AbortSignal.timeout(settings.timeoutMs),
    });
  } catch (error) {
    throw new CloudError(`Cloud could not be reached: ${(error as Error).message}`);
  }
}

/** Registers each live app with Cloud and keeps the registration fresh. */
export class CloudPublisher implements RoutePublisher {
  private readonly live = new Map<string, PublishedRoute>();
  private timer: ReturnType<typeof setInterval> | undefined;

  constructor(private readonly settings: CloudSettings) {
    if (!settings.url) throw new Error("CLOUD_URL is required to publish routes");
    if (!settings.key) throw new Error("CLOUD_USER_APPS_KEY is required when CLOUD_URL is set");
  }

  async publish(route: PublishedRoute): Promise<void> {
    const res = await request(this.settings, "PUT", endpoint(this.settings, route.slug), {
      upstreamUrl: `http://127.0.0.1:${route.hostPort}`,
    });
    if (res.status === 409) {
      throw new CloudError(`"${route.slug}" is already in use on this node`, 409);
    }
    if (!res.ok) throw new CloudError(`Cloud refused the route (HTTP ${res.status})`, res.status);
    this.live.set(route.slug, route);
  }

  async unpublish(slug: string): Promise<void> {
    this.live.delete(slug);
    const res = await request(this.settings, "DELETE", endpoint(this.settings, slug));
    // Already gone is the state we wanted.
    if (res.status !== 204 && res.status !== 404) {
      throw new CloudError(`Cloud could not remove the route (HTTP ${res.status})`, res.status);
    }
  }

  /** Announce routes that were live before this process started. */
  async restore(routes: readonly PublishedRoute[]): Promise<number> {
    let failed = 0;
    for (const route of routes) {
      try {
        await this.publish(route);
      } catch (error) {
        failed++;
        console.warn(`[cloud] could not restore ${route.slug}: ${(error as Error).message}`);
      }
    }
    return failed;
  }

  /**
   * Re-announce every route that is still live, and remove the ones that are
   * not, so a stopped app stops being routed instead of lingering until Cloud
   * times it out. Returns how many announcements failed.
   */
  async refresh(isLive: (slug: string) => boolean): Promise<number> {
    let failed = 0;
    for (const route of [...this.live.values()]) {
      try {
        if (isLive(route.slug)) await this.publish(route);
        else await this.unpublish(route.slug);
      } catch (error) {
        failed++;
        console.warn(`[cloud] ${route.slug}: ${(error as Error).message}`);
      }
    }
    return failed;
  }

  start(intervalMs: number, isLive: (slug: string) => boolean): void {
    this.stop();
    this.timer = setInterval(() => void this.refresh(isLive), intervalMs);
    this.timer.unref();
  }

  stop(): void {
    if (this.timer) clearInterval(this.timer);
    this.timer = undefined;
  }
}

/** Does Cloud already route this name? Throws when it cannot be answered. */
export async function cloudNameTaken(settings: CloudSettings, slug: string): Promise<boolean> {
  const res = await request(settings, "GET", endpoint(settings, slug, "/availability"));
  if (!res.ok)
    throw new CloudError(`Cloud could not check the name (HTTP ${res.status})`, res.status);
  const body = (await res.json().catch(() => null)) as { taken?: unknown } | null;
  if (typeof body?.taken !== "boolean") throw new CloudError("Cloud gave an unreadable answer");
  return body.taken;
}

/**
 * Does a Hosting site already answer for this host?
 *
 * Hosting's site-proxy dispatches on the Host header and answers an unknown name
 * with a 404 page that says "No site found". Anything else, including a 404 that
 * does not say it, means a site is there. Errors and 5xx throw: if we cannot tell,
 * the name is not given out.
 */
export function hostingNameTaken(
  siteUrl: string,
  host: string,
  timeoutMs: number,
): Promise<boolean> {
  return new Promise((resolve, reject) => {
    const target = new URL(siteUrl);
    const req = http.request(
      {
        hostname: target.hostname,
        port: target.port,
        path: "/",
        method: "GET",
        headers: { host },
        timeout: timeoutMs,
      },
      (res) => {
        const chunks: Buffer[] = [];
        res.on("data", (c: Buffer) => chunks.length < 64 && chunks.push(c));
        res.on("end", () => {
          const status = res.statusCode ?? 0;
          if (status >= 500)
            return reject(new CloudError(`Hosting answered HTTP ${status}`, status));
          const body = Buffer.concat(chunks).toString("utf8");
          resolve(!(status === 404 && body.includes("No site found")));
        });
        res.on("error", reject);
      },
    );
    req.on("timeout", () => req.destroy(new Error("Hosting did not answer in time")));
    req.on("error", reject);
    req.end();
  });
}

export type NameCheckSettings = {
  cloud?: CloudSettings;
  hostingSiteUrl?: string;
  /** slug → public host, from APP_HOST_PATTERN. */
  hostOf: (slug: string) => string;
  timeoutMs: number;
};

/**
 * "Is this name used outside Deploy?" — the answer for checkSlug's takenElsewhere.
 * Asks every configured neighbour. If any cannot be asked, this throws.
 */
export function makeTakenElsewhere(
  settings: NameCheckSettings,
): (slug: string) => Promise<boolean> {
  return async (slug) => {
    const checks: Promise<boolean>[] = [];
    if (settings.cloud) checks.push(cloudNameTaken(settings.cloud, slug));
    if (settings.hostingSiteUrl) {
      checks.push(
        hostingNameTaken(settings.hostingSiteUrl, settings.hostOf(slug), settings.timeoutMs),
      );
    }
    const answers = await Promise.all(checks);
    return answers.some(Boolean);
  };
}
````

**Modify `src/lib/config.ts`:**

````diff
--- a/src/lib/config.ts
+++ b/src/lib/config.ts
@@ -36,6 +36,18 @@ export const config = {
   // Mount point of the sandbox's size-capped storage. Builds are refused when it,
   // or the data directory's volume, is nearly full.
   sandboxStoragePath: process.env.SANDBOX_STORAGE_PATH ?? "",
+  // Nexus Cloud's registry is how the ingress learns where a live app listens.
+  // Empty CLOUD_URL means this node has no ingress to tell: nothing is announced.
+  cloudUrl: process.env.CLOUD_URL ?? "",
+  // Cloud's credential for apps members deploy. Not the node API key: that one is
+  // shared by every ecosystem service and cannot publish an isolated route.
+  cloudUserAppsKey: process.env.CLOUD_USER_APPS_KEY ?? "",
+  cloudTimeoutMs: Number(process.env.CLOUD_TIMEOUT_MS ?? 5000),
+  // How often live routes are re-announced, which is also Cloud's heartbeat.
+  cloudHeartbeatMs: Number(process.env.CLOUD_HEARTBEAT_MS ?? 30_000),
+  // Hosting's site-proxy, asked whether a name already belongs to a site so a new
+  // app never shadows one. Empty means this node has no Hosting to ask.
+  hostingSiteUrl: process.env.HOSTING_SITE_URL ?? "",
   // Safeguards (CAP_* variables). The same for every member of the node.
   limits: limitsFromEnv(process.env),
   dataDir: root,
````

- [ ] **Step 4: Run the checks**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
/tmp/nexus-deploy-tools/biome check src/lib/cloud.ts tests/cloud.test.ts
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: Biome reports no errors for the new files; no type errors; **213 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/cloud.ts src/lib/config.ts tests/cloud.test.ts
git commit \
  -m "feat(cloud): publish routes to Cloud and check names against Cloud and Hosting" \
  -m "CloudPublisher implements RoutePublisher with the user-apps credential, and its
refresh is the heartbeat. Name checks ask Cloud and Hosting's site-proxy and fail
closed when either cannot be asked." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 6: Deploy: assemble the edge from configuration and wire it in

`createEdge` builds the publisher and the name check from settings. With no `CLOUD_URL` the node announces nothing and only checks names against its own projects; with a `CLOUD_URL` but no credential it refuses to be built, so a half-configured node cannot silently run apps nobody can reach. `start()` announces the apps that were live before this process started, then keeps announcing them. `index.ts` installs the publisher for the release pipeline and starts the edge once listening.

**Repository:** `apps/Nexus-Deploy` (its own git repository). Paths below are relative to it.

**Files:**

- Modify: `.env.example`
- Modify: `src/index.ts`
- Create: `src/lib/edge.ts`
- Modify: `src/lib/wiring.ts`
- Create: `tests/edge.test.ts`

**Interfaces:**

- Consumes: `CloudPublisher`, `makeTakenElsewhere`, `CloudSettings` (Task 5); `appHost` (Plan 1); `loadDb`.
- Produces: `Edge`, `EdgeSettings`, `createEdge(settings, loadProjects)`, `liveRoutes(projects, settings)`, `realEdge()`, and `realDeps(edge)` (signature changed from `realDeps()`).

- [ ] **Step 1: Write the tests**

**Create `tests/edge.test.ts`:**

````typescript
import assert from "node:assert/strict";
import http from "node:http";
import type { AddressInfo } from "node:net";
import { test } from "node:test";
import { type EdgeSettings, createEdge, liveRoutes } from "../src/lib/edge.js";
import { NullPublisher } from "../src/lib/publisher.js";
import type { Project } from "../src/types.js";

const base: EdgeSettings = {
  cloudUrl: "",
  cloudUserAppsKey: "",
  cloudTimeoutMs: 400,
  cloudHeartbeatMs: 30,
  hostingSiteUrl: "",
  baseDomain: "example.test",
  appHostPattern: "{slug}.{BASE_DOMAIN}",
};

const project = (name: string, status: Project["status"], hostPort?: number): Project =>
  ({ id: `id-${name}`, name, status, ...(hostPort !== undefined ? { hostPort } : {}) }) as Project;

async function fakeCloud(): Promise<{
  url: string;
  calls: string[];
  close: () => Promise<void>;
}> {
  const calls: string[] = [];
  const server = http.createServer((req, res) => {
    calls.push(`${req.method} ${req.url}`);
    if (req.method === "DELETE") return void res.writeHead(204).end();
    res.writeHead(200, { "content-type": "application/json" });
    res.end(JSON.stringify({ taken: false }));
  });
  await new Promise<void>((r) => server.listen(0, "127.0.0.1", r));
  return {
    url: `http://127.0.0.1:${(server.address() as AddressInfo).port}`,
    calls,
    close: () =>
      new Promise<void>((r) => {
        server.closeAllConnections();
        server.close(() => r());
      }),
  };
}

test("only a project that is serving, on a known port, becomes a route", () => {
  const routes = liveRoutes(
    [
      project("serving", "live", 20001),
      project("stopped", "stopped", 20002),
      project("failed", "failed"),
      project("live-without-port", "live"),
    ],
    base,
  );
  assert.deepEqual(routes, [
    { projectId: "id-serving", slug: "serving", host: "serving.example.test", hostPort: 20001 },
  ]);
});

test("the host follows the node's pattern", () => {
  const routes = liveRoutes([project("a", "live", 20001)], {
    ...base,
    appHostPattern: "{slug}.apps.{BASE_DOMAIN}",
  });
  assert.equal(routes[0]?.host, "a.apps.example.test");
});

test("a node with no Cloud announces nothing and finds every name free", async () => {
  const edge = createEdge(base, () => []);
  assert.ok(edge.publisher instanceof NullPublisher);
  assert.equal(await edge.takenElsewhere("my-app"), false);
  await edge.start();
  edge.stop();
});

test("a node with Cloud but no credential refuses to be built", () => {
  assert.throws(
    () => createEdge({ ...base, cloudUrl: "http://127.0.0.1:1", cloudUserAppsKey: "" }, () => []),
    /CLOUD_USER_APPS_KEY/,
  );
});

test("starting announces the apps that were live, keeps announcing them, and removes one that stopped", async () => {
  const cloud = await fakeCloud();
  let projects: Project[] = [
    project("serving", "live", 20001),
    project("stopped", "stopped", 20002),
  ];
  const edge = createEdge({ ...base, cloudUrl: cloud.url, cloudUserAppsKey: "k" }, () => projects);
  try {
    await edge.start();
    assert.deepEqual(cloud.calls, ["PUT /api/v1/user-apps/serving"]);

    await new Promise((r) => setTimeout(r, 120));
    const puts = cloud.calls.filter((c) => c === "PUT /api/v1/user-apps/serving").length;
    assert.ok(puts >= 3, `expected repeated announcements, saw ${puts}`);

    projects = [project("serving", "stopped", 20001)];
    await new Promise((r) => setTimeout(r, 120));
    assert.ok(cloud.calls.includes("DELETE /api/v1/user-apps/serving"));
    const after = cloud.calls.length;
    await new Promise((r) => setTimeout(r, 100));
    assert.equal(cloud.calls.length, after, "a stopped app is no longer announced");
  } finally {
    edge.stop();
    await cloud.close();
  }
});

test("starting with Cloud down does not throw, and the node still comes up", async () => {
  const edge = createEdge(
    { ...base, cloudUrl: "http://127.0.0.1:1", cloudUserAppsKey: "k" },
    () => [project("serving", "live", 20001)],
  );
  try {
    await edge.start();
  } finally {
    edge.stop();
  }
});

test("a name check goes to Cloud, and a Cloud that cannot answer fails it", async () => {
  const cloud = await fakeCloud();
  try {
    const up = createEdge({ ...base, cloudUrl: cloud.url, cloudUserAppsKey: "k" }, () => []);
    assert.equal(await up.takenElsewhere("my-app"), false);
    assert.ok(cloud.calls.includes("GET /api/v1/user-apps/my-app/availability"));
  } finally {
    await cloud.close();
  }
  const down = createEdge(
    { ...base, cloudUrl: "http://127.0.0.1:1", cloudUserAppsKey: "k" },
    () => [],
  );
  await assert.rejects(down.takenElsewhere("my-app"));
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
node --import tsx --test tests/edge.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/edge.js'`.

- [ ] **Step 3: Implement**

**Modify `.env.example`:**

````diff
--- a/.env.example
+++ b/.env.example
@@ -46,6 +46,19 @@ SANDBOX_DOCKER_HOST=unix:///run/user/1001/docker.sock
 # Mount point of the sandbox's size-capped storage (builds pause when it fills)
 SANDBOX_STORAGE_PATH=/var/lib/nexus-deploy-sandbox
 
+# === Ingress (how live apps are announced) ===
+# Nexus Cloud's internal address. Empty means this node has no ingress to tell.
+# CLOUD_URL=http://127.0.0.1:8787
+# Cloud's credential for member apps (NEXUS_CLOUD_USER_APPS_KEY on the Cloud side).
+# This is NOT the node API key. Required when CLOUD_URL is set.
+# CLOUD_USER_APPS_KEY=
+# CLOUD_TIMEOUT_MS=5000
+# How often live apps are re-announced (Cloud's heartbeat)
+# CLOUD_HEARTBEAT_MS=30000
+# Hosting's site-proxy, asked whether a name already belongs to a site.
+# Empty means this node has no Hosting to ask.
+# HOSTING_SITE_URL=http://127.0.0.1:8090
+
 # === Safeguards (the same for everyone; all optional) ===
 # CAP_MEMORY_MB=512
 # CAP_CPUS=0.5
````

**Modify `src/index.ts`:**

````diff
--- a/src/index.ts
+++ b/src/index.ts
@@ -1,18 +1,23 @@
 import { createServer } from "node:http";
 import { createApp } from "./app.js";
+import { setRoutePublisher } from "./lib/build.js";
 import { config, ensureDataDir } from "./lib/config.js";
 import { startStatusSync } from "./lib/status-sync.js";
-import { realDeps } from "./lib/wiring.js";
+import { realDeps, realEdge } from "./lib/wiring.js";
 import { attachSockets } from "./lib/ws.js";
 
 ensureDataDir();
-const deps = realDeps();
+const edge = realEdge();
+// The release pipeline announces new releases through this.
+setRoutePublisher(edge.publisher);
+const deps = realDeps(edge);
 const server = createServer(createApp(deps));
 attachSockets(server, deps);
 
 startStatusSync(30_000);
 
 server.listen(config.port, () => {
+  void edge.start();
   console.log(`
 ╔════════════════════════════════════════╗
 ║        NEXUS DEPLOY  v${deps.version.padEnd(16)}║
````

**Create `src/lib/edge.ts`:**

````typescript
import type { Project } from "../types.js";
import { CloudPublisher, type CloudSettings, makeTakenElsewhere } from "./cloud.js";
/**
 * How this node's ingress is told about apps, assembled from configuration.
 *
 * Cloud and Hosting are both optional: a node with neither announces nothing and
 * only checks names against its own projects. A node with Cloud must have its
 * credential, so a half-configured node refuses to start instead of silently
 * running apps nobody can reach.
 */
import { appHost } from "./hosts.js";
import { NullPublisher, type PublishedRoute, type RoutePublisher } from "./publisher.js";

export type EdgeSettings = {
  cloudUrl: string;
  cloudUserAppsKey: string;
  cloudTimeoutMs: number;
  cloudHeartbeatMs: number;
  hostingSiteUrl: string;
  baseDomain: string;
  appHostPattern: string;
};

export type Edge = {
  publisher: RoutePublisher;
  /** For checkSlug: is the name used outside Deploy? Throws when it cannot be checked. */
  takenElsewhere: (slug: string) => Promise<boolean>;
  /** Announce the apps that were live before this process started, then keep them announced. */
  start: () => Promise<void>;
  stop: () => void;
};

/** The routes for every project that is serving now. */
export function liveRoutes(projects: readonly Project[], settings: EdgeSettings): PublishedRoute[] {
  return projects.flatMap((project) =>
    project.status === "live" && project.hostPort !== undefined
      ? [
          {
            projectId: project.id,
            slug: project.name,
            host: appHost(project.name, settings.appHostPattern, settings.baseDomain),
            hostPort: project.hostPort,
          },
        ]
      : [],
  );
}

export function createEdge(settings: EdgeSettings, loadProjects: () => readonly Project[]): Edge {
  const hostOf = (slug: string) => appHost(slug, settings.appHostPattern, settings.baseDomain);

  let cloud: CloudSettings | undefined;
  let publisher: RoutePublisher = new NullPublisher();
  let cloudPublisher: CloudPublisher | undefined;
  if (settings.cloudUrl) {
    cloud = {
      url: settings.cloudUrl,
      key: settings.cloudUserAppsKey,
      timeoutMs: settings.cloudTimeoutMs,
    };
    cloudPublisher = new CloudPublisher(cloud);
    publisher = cloudPublisher;
  }

  return {
    publisher,
    takenElsewhere: makeTakenElsewhere({
      ...(cloud ? { cloud } : {}),
      ...(settings.hostingSiteUrl ? { hostingSiteUrl: settings.hostingSiteUrl } : {}),
      hostOf,
      timeoutMs: settings.cloudTimeoutMs,
    }),
    async start() {
      if (!cloudPublisher) return;
      const failed = await cloudPublisher.restore(liveRoutes(loadProjects(), settings));
      if (failed > 0) console.warn(`[edge] ${failed} live app(s) could not be announced to Cloud`);
      cloudPublisher.start(settings.cloudHeartbeatMs, (slug) =>
        loadProjects().some((p) => p.name === slug && p.status === "live"),
      );
    },
    stop() {
      cloudPublisher?.stop();
    },
  };
}
````

**Modify `src/lib/wiring.ts`:**

````diff
--- a/src/lib/wiring.ts
+++ b/src/lib/wiring.ts
@@ -9,7 +9,6 @@ import type { AppDeps, ProjectOps } from "../routes/types.js";
 import {
   cancelBuild,
   createDeployment,
-  getRoutePublisher,
   guardBuild,
   isBuilding,
   runDeploy,
@@ -27,6 +26,8 @@ import {
   dockerStatus,
   dockerStop,
 } from "./docker.js";
+import { type Edge, createEdge } from "./edge.js";
+import { loadDb } from "./store.js";
 
 export const dockerOps: ProjectOps = {
   stop: dockerStop,
@@ -49,7 +50,12 @@ export function packageVersion(): string {
   }
 }
 
-export function realDeps(): AppDeps {
+/** The ingress this node is configured with. */
+export function realEdge(): Edge {
+  return createEdge(config, () => loadDb().projects);
+}
+
+export function realDeps(edge: Edge): AppDeps {
   return {
     authenticate: authenticateWithNexusAuth,
     pipeline: {
@@ -62,9 +68,8 @@ export function realDeps(): AppDeps {
       guardBuild: (project) => guardBuild(project),
     },
     ops: dockerOps,
-    publisher: getRoutePublisher(),
-    // Until the edge is wired in, only Deploy's own projects are checked for a taken name.
-    takenElsewhere: async () => false,
+    publisher: edge.publisher,
+    takenElsewhere: edge.takenElsewhere,
     limits: config.limits,
     allowLocalRepos: config.allowLocalRepos,
     now: Date.now,
````

- [ ] **Step 4: Run the checks**

```bash
cd apps/Nexus-Deploy
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
/tmp/nexus-deploy-tools/biome check src/lib/edge.ts src/lib/wiring.ts src/index.ts tests/edge.test.ts
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: Biome reports no errors for the new files; no type errors; **220 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add .env.example src/index.ts src/lib/edge.ts src/lib/wiring.ts tests/edge.test.ts
git commit \
  -m "feat(edge): announce live apps to Cloud and check names against Cloud and Hosting" \
  -m "A node with no Cloud announces nothing; one with a Cloud but no credential refuses
to start. Live apps are re-announced on a timer, which is Cloud's heartbeat." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 7: Prove the whole edge end to end, record the decisions in the spec, and push

Each task above passed alone. This one runs a **real Cloud** (a child process on a spare port with an empty data directory, so it never touches the running one), Deploy's real `CloudPublisher`, and the real proxy together, and asserts what the spec promises: the app answers; it sees no cookie, no visitor tag and no address; the short link redirects; publishing again moves the route; the node API key cannot publish; and after removal the host falls through to Hosting and the short link stops redirecting. The script is not named `*.test.ts` because it starts a process and binds ports, so `bun test` never picks it up.

**Repository:** the monorepo for the script and the spec; each of `apps/Nexus-Cloud` and `apps/Nexus-Deploy` is pushed from its own repository.

**Files:**

- Create: `deploy/production/tests/deploy-edge.e2e.ts`
- Modify: `docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md`

**Interfaces:**

- Consumes: everything from Tasks 1–6.
- Produces: a repeatable check that Plan 3's `verify` builds on, and a spec that matches the code.

- [ ] **Step 1: Add the end-to-end script**

**Create `deploy/production/tests/deploy-edge.e2e.ts`:**

````typescript
/**
 * End to end: a real Nexus Cloud, Deploy's real publisher, and this proxy.
 *
 * Run by hand (it starts a Cloud of its own on a spare port, with its own empty
 * data directory, so it never touches the running one):
 *
 *   bun run deploy/production/tests/deploy-edge.e2e.ts
 *
 * It is not named *.test.ts on purpose: it starts a child process and binds
 * ports, so `bun test` should not pick it up.
 */
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve } from "node:path";
import { CloudPublisher, cloudNameTaken } from "../../../apps/Nexus-Deploy/src/lib/cloud.ts";

const CLOUD_PORT = 18788;
const APP_A = 20555;
const APP_B = 20556;

process.env.DOMAIN = "nexus.local";
process.env.CLOUD_URL = `http://127.0.0.1:${CLOUD_PORT}`;
process.env.NEXUS_CLOUD_API_KEY = "e2e-node-key";
process.env.CACHE_TTL_MS = "1";
process.env.GATE_SKIP_AUTH = "true";

const dataDir = mkdtempSync(join(tmpdir(), "deploy-edge-e2e-"));
const cloudProc = Bun.spawn(
  ["bun", "run", resolve(import.meta.dir, "../../../apps/Nexus-Cloud/src/server.ts")],
  {
    cwd: dataDir,
    env: {
      ...process.env,
      PORT: String(CLOUD_PORT),
      NEXUS_CLOUD_DOMAIN: "nexus.local",
      NEXUS_CLOUD_USER_APPS_KEY: "e2e-user-apps-key",
      NEXUS_CLOUD_API_KEY: "e2e-node-key",
    },
    stdout: "ignore",
    stderr: "ignore",
  },
);

const servers: Array<{ stop(force?: boolean): void }> = [];
const results: string[] = [];
const check = (name: string, ok: boolean, detail = "") => {
  results.push(`${ok ? "PASS" : "FAIL"} ${name}${detail ? ` — ${detail}` : ""}`);
};

async function waitForCloud(): Promise<void> {
  for (let i = 0; i < 50; i++) {
    try {
      if ((await fetch(`http://127.0.0.1:${CLOUD_PORT}/health`)).ok) return;
    } catch {
      // not up yet
    }
    await Bun.sleep(100);
  }
  throw new Error("the scratch Cloud did not start");
}

try {
  await waitForCloud();

  let seen: Record<string, string | null> = {};
  servers.push(
    Bun.serve({
      port: APP_A,
      hostname: "127.0.0.1",
      fetch(req) {
        seen = {
          cookie: req.headers.get("cookie"),
          tag: req.headers.get("x-nexus-client-tag"),
          ip: req.headers.get("cf-connecting-ip"),
        };
        return new Response("hello from my-app");
      },
    }),
    Bun.serve({ port: APP_B, hostname: "127.0.0.1", fetch: () => new Response("v2") }),
  );
  const hosting = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch: () => new Response("No site found for x", { status: 404 }),
  });
  const control = Bun.serve({
    port: 0,
    hostname: "127.0.0.1",
    fetch: () => new Response("deploy control plane"),
  });
  servers.push(hosting, control);
  process.env.HOSTING_SITE_UPSTREAM = `http://127.0.0.1:${hosting.port}`;
  process.env.DEPLOY_UPSTREAM = `http://127.0.0.1:${control.port}`;

  const { handleRequest, __setMaintenanceForTest } = await import("../proxy.ts");
  __setMaintenanceForTest([]);
  const cloud = { url: process.env.CLOUD_URL, key: "e2e-user-apps-key", timeoutMs: 3000 };
  const route = (port: number) => ({
    projectId: "p1",
    slug: "my-app",
    host: "my-app.nexus.local",
    hostPort: port,
  });

  check("the name is free before anything is published", (await cloudNameTaken(cloud, "my-app")) === false);

  const publisher = new CloudPublisher(cloud);
  await publisher.publish(route(APP_A));
  await Bun.sleep(20);

  const res = await handleRequest(
    new Request("http://my-app.nexus.local/", {
      headers: { cookie: "nexus_session=victim", "cf-connecting-ip": "203.0.113.9" },
    }),
  );
  check(
    "the app answers through the proxy",
    res.status === 200 && (await res.text()) === "hello from my-app",
    `status ${res.status}`,
  );
  check(
    "the app saw no cookie, no visitor tag and no address",
    seen.cookie === null && seen.tag === null && seen.ip === null,
    JSON.stringify(seen),
  );

  const link = await handleRequest(new Request("http://deploy.nexus.local/my-app"));
  check(
    "deploy.<domain>/<name> redirects to the app",
    link.status === 302 && link.headers.get("location") === "https://my-app.nexus.local/",
    `${link.status} ${link.headers.get("location")}`,
  );

  await publisher.publish(route(APP_B));
  await Bun.sleep(20);
  const moved = await handleRequest(new Request("http://my-app.nexus.local/"));
  check("publishing again moves the route to the new port", (await moved.text()) === "v2");

  const stolen = await fetch(`${cloud.url}/api/v1/user-apps/my-app`, {
    method: "PUT",
    headers: { authorization: "Bearer e2e-node-key", "content-type": "application/json" },
    body: JSON.stringify({ upstreamUrl: `http://127.0.0.1:${APP_A}` }),
  });
  check("the node API key cannot publish a member's app", stolen.status === 401, `${stolen.status}`);

  await publisher.unpublish("my-app");
  await Bun.sleep(20);
  const gone = await handleRequest(new Request("http://my-app.nexus.local/"));
  check("after unpublishing, the host falls through to Hosting", gone.status === 404, `${gone.status}`);
  const goneLink = await handleRequest(new Request("http://deploy.nexus.local/my-app"));
  check(
    "after unpublishing, the short link no longer redirects",
    goneLink.status === 200 && (await goneLink.text()) === "deploy control plane",
    `${goneLink.status}`,
  );
} finally {
  for (const server of servers) server.stop(true);
  cloudProc.kill();
  rmSync(dataDir, { recursive: true, force: true });
}

console.log(results.join("\n"));
process.exit(results.length === 0 || results.some((r) => r.startsWith("FAIL")) ? 1 : 0);
````

- [ ] **Step 2: Run it**

```bash
bun run deploy/production/tests/deploy-edge.e2e.ts
```

Expected: eight lines, each beginning `PASS`, and exit status 0:

```
PASS the name is free before anything is published
PASS the app answers through the proxy — status 200
PASS the app saw no cookie, no visitor tag and no address — {"cookie":null,"tag":null,"ip":null}
PASS deploy.<domain>/<name> redirects to the app — 302 https://my-app.nexus.local/
PASS publishing again moves the route to the new port
PASS the node API key cannot publish a member's app — 401
PASS after unpublishing, the host falls through to Hosting — 404
PASS after unpublishing, the short link no longer redirects — 200
```

If it fails to start Cloud, check that port 18788 is free. It leaves nothing behind: the data directory is removed and the child process is killed on exit.

- [ ] **Step 3: Make the spec match the code**

The spec left three things to planning. Replace the sentences below, each of which occurs exactly once in `docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md`.

In §6.1, replace `The flag is set by Cloud for registrations that arrive through Deploy's credential; an app cannot choose or clear it, mirroring how `requiresAuth` is operator-set only. The exact Cloud change is a planning task.` with:

````markdown
The flag (`userDeployed`) is set by Cloud only through its `/api/v1/user-apps/:slug` surface, which is guarded by its own credential (`NEXUS_CLOUD_USER_APPS_KEY`), not by the node API key that every ecosystem service holds. The flag is carried over from the previous record and never read from a payload, exactly like `requiresAuth`, and the generic registration refuses the `deploy-` id prefix, so an app can neither choose nor clear it. Cloud accepts only `http://127.0.0.1:<port>` upstreams inside `NEXUS_CLOUD_USER_APPS_PORTS` (default `20000-29999`), so a leaked credential cannot aim a public hostname at Auth, Cloud or a database.
````

In §6.3, replace `The Hosting check calls Hosting's internal API and **fails closed**.` with:

````markdown
The Hosting check asks Hosting's site-proxy by `Host` header (Hosting has no availability endpoint): its own "No site found" 404 means the name is free, anything else means a site is there, and an unreachable Hosting or a 5xx **fails closed**. Known limitation: the site-proxy returns the same 404 when its own database errors, so a Hosting database outage can make a taken name look free; because Cloud routes take precedence over Hosting (§6.2), the failure mode is a member's app shadowing a Hosting site, never exposing its content. An explicit Hosting endpoint is the follow-up that removes it.
````

In §8.1, replace ` **To verify:** that the Auth session cookie is `HttpOnly`.` with:

````markdown
 The Auth session cookie is `HttpOnly` (verified in `apps/Nexus-Auth/src/server.ts`), so an app's JavaScript cannot read the session.
````

In §14, replace the two lines `1. Auth session cookie is `HttpOnly`.` and `4. How Cloud should model the user-app flag, and whether Hosting exposes (or can expose) an internal name-check endpoint.` with:

````markdown
1. ~~Auth session cookie is `HttpOnly`.~~ Verified (Plan 2).
4. ~~How Cloud should model the user-app flag, and whether Hosting exposes an internal name-check endpoint.~~ Resolved (Plan 2): a dedicated credential and surface for the flag; Hosting is asked through its site-proxy.
````

- [ ] **Step 4: Run every suite once more**

```bash
cd apps/Nexus-Cloud && PORT=18787 bun test src/systems-api src/test src/api && cd ../..
cd deploy/production && bun test tests/user-apps.test.ts tests/proxy.test.ts tests/gate.test.ts tests/client-tag.test.ts && cd ../..
cd apps/Nexus-Deploy && ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild node --import tsx --test tests/*.test.ts && cd ../..
```

Expected: Cloud **132** passing, proxy **115** passing, Deploy **220** passing.

- [ ] **Step 5: Commit and push each repository, then the monorepo**

The three repositories are independent; push Cloud and Deploy first, then record their new commits in the monorepo together with the script and the spec.

```bash
cd apps/Nexus-Cloud && git push origin main && cd ../..
cd apps/Nexus-Deploy && git push origin main && cd ../..
git add deploy/production/proxy.ts deploy/production/gate.ts deploy/production/tests/user-apps.test.ts deploy/production/tests/deploy-edge.e2e.ts docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md apps/Nexus-Cloud apps/Nexus-Deploy
git commit \
  -m "feat(edge): the proxy and Cloud side of Nexus Deploy, with an end-to-end check" \
  -m "Member apps are public, isolated routes; deploy.<domain> reaches Deploy and
deploy.<domain>/<name> redirects to the app. Spec updated to match what was built." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
git push origin main
```

Nothing is running differently yet: the proxy and Cloud keep serving their old code until the operator restarts them in Plan 3.
