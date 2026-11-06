# Nexus Deploy Foundation — Plan 1 of 3: The Control Plane

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn the existing `apps/Nexus-Deploy` server into a control plane that a node can safely share between invited members: ownership, two kinds of source, private repositories, blue/green releases, safeguards, a sandbox-only Docker boundary, and a route-publishing interface. Nothing is deployed to production by this plan.

**Architecture:** `src/app.ts` builds the HTTP app from injected dependencies (identity, build pipeline, container operations, route publisher), and `src/index.ts` only wires the real ones. Policy lives in small pure modules (names, safeguards, members, sources, release, prune) that are tested without Docker, Git, a disk or a clock. Every Docker call is forced to a rootless sandbox daemon and refuses to run without one.

**Tech Stack:** Node.js (ESM, `NodeNext`), TypeScript strict, Express 5, `ws`, `node:test` run through `tsx`, Biome.

**Spec:** `docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md`. Executors read it with this plan.

## Where this plan sits

The spec describes one slice, delivered as three plans, each ending in working, tested software:

| Plan | Delivers | State of the world when it is done |
|---|---|---|
| **1 — Control plane** (this plan) | Everything inside `apps/Nexus-Deploy`: members, ownership, sources, safeguards, sandbox-only Docker, blue/green releases, the publisher interface, fixes to defects in the original | Fully tested in isolation. Not deployed; Deploy still is not reachable. |
| 2 — Edge | Cloud registers deployed apps and marks them user-owned; the proxy isolates them, serves `<app>.tnhc.dev`, redirects `deploy.tnhc.dev/<app>`, and routes `deploy.tnhc.dev` to Deploy; a real name check against Cloud and Hosting | Deployed apps become reachable. |
| 3 — Sandbox, conformance, dashboard | The `sudo` setup script for the rootless sandbox and its size-capped storage; `doctor` and `verify` with fixture apps; the dashboard built and moved onto the shared theme; node bring-up; the disposable second-node proof | The success criterion in the spec: `verify` green on this node and on a disposable one. |

Plans 2 and 3 are written after this one is built, against the real code.

## Spec coverage

Every section of the spec is owned by exactly one plan, so nothing is dropped between them.

| Spec section | Where it is built |
|---|---|
| §1 Success criterion (`verify` on this node and a disposable one) | Plan 3 |
| §2 Charter constraints: privacy and capped logs | Tasks 7, 10, 13, 14; the proxy half in Plan 2 |
| §2 Licence (AGPL) | Task 16 |
| §2 Best-effort availability stated to members | Task 12 (`/api/limits`) |
| §2 Cloud parity: release lifecycle | Tasks 8, 9 |
| §2 Build volume is production; abuse control | Tasks 3, 7, 9, 14; the fixed-size storage image in Plan 3 |
| §3 Decisions and cost audit | Global Constraints; no paid dependency is introduced |
| §5 Control plane | Tasks 4–13 |
| §5 Builder | Task 9 (cloning and building); installing a Nixpacks-class builder in Plan 3 |
| §5 Sandbox runtime | Task 7 (the boundary in code); the `sudo` setup script and storage in Plan 3 |
| §5 Router, short link | Plan 2 |
| §5 Name registry | Tasks 2, 12; the real Cloud and Hosting check in Plan 2 |
| §5 Conformance check | Plan 3 |
| §6.1–6.4 Cloud routes, user-app isolation flag, precedence, short link | Plan 2 (the publisher interface is Task 8) |
| §7.1 Members, §7.2 ownership and storage | Tasks 4, 10, 12 |
| §7.3 Sources, private repositories, image pinning | Tasks 5, 9, 11 |
| §7.4 Blue/green and stop-then-start | Tasks 8, 9 |
| §8.1 Sessions and cookies | Plan 2 (the proxy) |
| §8.1 Secrets, fail closed, sandbox boundary | Tasks 2, 5, 7 |
| §8.2 Logs capped, audit, export and deletion | Tasks 7, 10, 13 |
| §8.2 Privacy guard (`scripts/check-privacy.sh`) | Task 16 |
| §9 Safeguards and the disk guard | Tasks 3, 9, 14 |
| §10 Bring-up, `deploy.sh`, Cloud registration, theme | Plan 3 (documentation and licence in Task 16) |
| §11 Portability, no hardcoded domain | Task 1; the disposable-node proof in Plan 3 |
| §13 Testing | Throughout; fixtures and `verify` in Plan 3 |
| §14 Verification tasks | 1 (`HttpOnly` cookie) and 4 (Cloud flag, Hosting check) in Plan 2; 2, 3, 5–8, 10 in Plan 3; 9 is tested in Tasks 5 and 13 |

## Global Constraints

These are copied from the spec. Every task's requirements include them.

- Free; no billing, plans or paywalls. The only recurring cost allowed is the domain (about 12.20 USD per year). Every dependency must work at zero further cost.
- No visitor address is read or stored anywhere. The audit log records account, action and time only, and is kept 30 days.
- Applications are licensed AGPL-3.0.
- Node-specific values are configuration: `BASE_DOMAIN`, `APP_HOST_PATTERN` (default `{slug}.{BASE_DOMAIN}`), `CONTROL_HOST`, `SANDBOX_DOCKER_HOST`, `SANDBOX_STORAGE_PATH`, `APP_PORT_RANGE` (default `20000-29999`), `CAP_*`. No source file names a domain.
- A slug matches `^[a-z][a-z0-9-]{1,38}[a-z0-9]$` (and contains no `--`), is not reserved, is not already used, and if the registry cannot be asked the name is refused.
- Safeguard defaults, equal for everyone: container 512 MB memory with no swap, 0.5 CPU, 256 processes; member 5 projects, 1 concurrent build, 6 builds per hour, 10-minute build timeout, 10-minute image pull; builds refused when a measured host volume has less than 15% free or the sandbox storage is more than 85% used.
- App containers drop all capabilities, set `no-new-privileges`, are published on `127.0.0.1` only, and use the `local` log driver capped at 10 MB × 2 files with compression off.
- `docker` and `nixpacks` run only against `SANDBOX_DOCKER_HOST` and never fall back to the host's daemon.
- Releases are blue/green; a project with a persistent volume releases stop-then-start.
- An image source records its resolved digest, so a rollback restores the exact bytes.
- A private-repository token never appears in a URL, a command line, a log or an API response.
- Another member's project answers `404`, exactly like a missing one.
- New code passes Biome; `tsc` is clean for both source and tests. Commits go to `main` of the `Nexus-Deploy` repository (solo developer: no branches, no pull requests), in `type(scope): description` form.

## Running things on this node

All commands run from `apps/Nexus-Deploy`. The repository sits on a mount that cannot execute binaries, so esbuild (used by `tsx`) and Biome are run from copies:

```bash
cd apps/Nexus-Deploy
mkdir -p /tmp/nexus-deploy-tools
cp node_modules/@esbuild/linux-x64/bin/esbuild /tmp/nexus-deploy-tools/esbuild
chmod +x /tmp/nexus-deploy-tools/esbuild
export ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild
```

(On a machine where `npm test` already works, skip this.) Three commands recur:

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit        # type-check the source
node node_modules/typescript/bin/tsc -p tsconfig.test.json            # also type-check the tests (from Task 8)
node --import tsx --test tests/*.test.ts                              # every test
```

**Baseline before Task 1:** 17 tests pass; `tsc` is clean; Biome reports 41 existing errors that this plan does not touch.

## Defects found in the existing code

Found while preparing this plan, by probing the untouched original server. Each is fixed by the task named.

| Defect | Evidence | Fixed in |
|---|---|---|
| A correctly signed GitHub push webhook returns **HTTP 500** (`TypeError`). The global JSON parser consumes the body first, so the signature is computed over an object, not bytes. Push-to-deploy by webhook has not worked. | probe: signed JSON `POST` → `500` | Task 13 |
| A webhook with **no signature** gets past the check (the original only verified when a header was present). | probe: unsigned `POST` → reached payload parsing (`400 Bad payload`) | Task 13 |
| GitHub's `ping` event would start a build (the payload has no `ref`, so the branch filter was skipped). | reading the code | Task 13 |
| **Both live-log WebSocket endpoints are broken.** Two `WebSocketServer`s attached to one HTTP server each answer every upgrade, so the non-matching one rejects what the other accepted. | probe: `/api/log-stream` → `Invalid WebSocket frame: RSV1 must be clear`; `/api/container-stream` → `HTTP 400` | Task 13 |
| Every signed-in account could list **every project, deployment, activity entry and status event**, and act on any project. | reading the code | Tasks 4, 12, 13 |
| A project's notification URL could point at loopback or the LAN, making the server send requests to internal services. | reading the code | Task 11 |
| `volumePath` was passed unchecked into `-v name:path`, so a value like `/a:/b` could add mount options. | reading the code | Tasks 7, 11 |
| Containers used Docker's default log driver, which keeps logs forever and unbounded; environment variables were passed on the command line, visible to `ps`. | reading the code | Task 7 |
| `README.md` says MIT; `LICENSE` is AGPL-3.0. | reading both | Task 16 |

## Where this plan refines the spec

Learned while building the code the plan contains.

- **Images are pruned by reference, not by label.** The spec (§7.3) said images carry a `nexus.managed` label for the prune. A pulled third-party image cannot be labelled in place, and the sandbox daemon is dedicated to Deploy, so every image in it is Deploy's. The sweep removes any image no project, rollback target or recent deployment references (Task 14). Containers keep the label.
- **The real name check arrives in Plan 2.** `realDeps().takenElsewhere` returns `false` until Plan 2 connects Cloud's registry and Hosting. The fail-closed behaviour (an unreachable registry refuses the name) is implemented and tested now.
- **`--cap-drop ALL` is strict on purpose.** Images that must switch user at start-up (the official `nginx` image does) will not run. No per-project opt-in is added; Plan 3's fixtures avoid such images, and a relaxation needs evidence first.
- **Environment goes in a mode-0600 `--env-file`.** Docker's env files have no quoting, so a value may not contain a line break (Task 5).
- **Custom domains are removed from the project update.** The field did nothing under the new routing; custom domains are a later sub-project.
- **The first founder to arrive becomes the owner.** Only a Nexus-Auth `founder`, and only while nobody is a member yet (Task 12).
- **The build timeout covers clone and build.** A pull has its own timeout (Tasks 3, 9).

## File structure

All paths under `apps/Nexus-Deploy`.

| Path | Responsibility |
|---|---|
| `src/lib/hosts.ts` | App hostnames from a pattern; port-range parsing |
| `src/lib/names.ts` | Slug format, reserved names, the fail-closed availability check |
| `src/lib/safeguards.ts` | Caps, quotas, rate limit, disk guard (pure) |
| `src/lib/guard.ts` | "May this project build now?" from the database and measurements |
| `src/lib/members.ts` | Operators, members, ownership, first-owner bootstrap |
| `src/lib/source.ts` | Git/image sources, image-reference validation, private-repo credentials, redaction, public project view |
| `src/lib/env.ts` | Environment-variable validation and env-file rendering |
| `src/lib/project-input.ts` | Validating and applying everything a member can submit |
| `src/lib/urls.ts` | Notification-URL request-forgery guard |
| `src/lib/ports.ts` | Loopback port allocation |
| `src/lib/run-args.ts` | The exact `docker run` arguments (the container boundary) |
| `src/lib/docker.ts` | Sandbox-only Docker/builder execution and image helpers |
| `src/lib/docker-runtime.ts` | The real `ReleaseRuntime` over `docker.ts` |
| `src/lib/release.ts` | Blue/green and stop-then-start release policy |
| `src/lib/publisher.ts` | `RoutePublisher` interface and `NullPublisher` |
| `src/lib/build-steps.ts` | The three real steps: clone, build, pull |
| `src/lib/build.ts` | The deployment pipeline, log streaming, cancellation, rollback |
| `src/lib/containers.ts` | The container serving a project |
| `src/lib/audit.ts` | Audit entries and retention |
| `src/lib/prune.ts` | Which images the sweep may remove |
| `src/lib/status-sync.ts` | Background reconciliation and housekeeping |
| `src/lib/ws.ts` | Authorised WebSocket streams, routed by path |
| `src/lib/wiring.ts` | The real dependencies for `createApp` |
| `src/middleware/auth.ts` | Nexus-Auth authentication, injectable |
| `src/routes/*.ts` | `types` (shared types), `context` (membership, ownership, audit), `basic`, `ai`, `projects`, `lifecycle`, `webhook` |
| `src/app.ts`, `src/index.ts` | App factory; boot |
| `tests/support/*.ts` | Fakes and the HTTP test harness |

---

## Tasks

### Task 1: Node-specific configuration and host helpers

Everything that differs between nodes (domain, URL pattern, port range, sandbox socket) must be configuration, with no domain named in code. This task adds the helpers and the settings, plus a test that fails if any source file ever names a domain.

**Files:**

- Create: `src/lib/hosts.ts`
- Create: `tests/hosts.test.ts`
- Create: `tests/no-hardcoded-domain.test.ts`
- Modify: `src/lib/config.ts`

**Interfaces:**

- Consumes: Nothing.
- Produces: `appHost(slug, pattern, baseDomain): string`, `appUrl(...)`, `parsePortRange(raw): { min: number; max: number }`, and `config.controlHost`, `config.appHostPattern`, `config.appPortRange`, `config.sandboxDockerHost`, `config.sandboxStoragePath`.

- [ ] **Step 1: Write the tests**

**Create `tests/hosts.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { appHost, appUrl, parsePortRange } from "../src/lib/hosts.js";

test("the default pattern puts the slug one level under the base domain", () => {
  assert.equal(appHost("my-app", "{slug}.{BASE_DOMAIN}", "example.test"), "my-app.example.test");
  assert.equal(appUrl("my-app", "{slug}.{BASE_DOMAIN}", "example.test"), "https://my-app.example.test");
});

test("a node can choose a different pattern", () => {
  assert.equal(appHost("a", "{slug}-apps.{BASE_DOMAIN}", "node.test"), "a-apps.node.test");
});

test("a pattern without {slug} is refused, because every app would share one host", () => {
  assert.throws(() => appHost("a", "apps.{BASE_DOMAIN}", "node.test"), /\{slug\}/);
});

test("port ranges parse, and refuse privileged ports, reversed bounds and junk", () => {
  assert.deepEqual(parsePortRange("20000-29999"), { min: 20000, max: 29999 });
  assert.deepEqual(parsePortRange(" 3000-3001 "), { min: 3000, max: 3001 });
  for (const bad of ["", "abc", "1-2000", "30000-20000", "20000-70000", "20000", "20000-"]) {
    assert.throws(() => parsePortRange(bad), /APP_PORT_RANGE/, bad);
  }
});
````

**Create `tests/no-hardcoded-domain.test.ts`:**

````typescript
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { test } from "node:test";

// Portability rule: any node must be able to run this code with its own domain,
// so no source file may name a particular one. Tests and docs may.
function sourceFiles(dir: string): string[] {
  return fs.readdirSync(dir, { withFileTypes: true }).flatMap((entry) => {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) return sourceFiles(full);
    return entry.name.endsWith(".ts") ? [full] : [];
  });
}

test("no source file hardcodes a deployment domain", () => {
  const offenders = sourceFiles(path.resolve("src")).filter((file) =>
    /tnhc\.dev/i.test(fs.readFileSync(file, "utf8")),
  );
  assert.deepEqual(offenders, []);
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/hosts.test.ts tests/no-hardcoded-domain.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/hosts.js'` (for `hosts.test.ts`). `no-hardcoded-domain.test.ts` already passes.

- [ ] **Step 3: Implement**

**Modify `src/lib/config.ts`:**

````diff
--- a/src/lib/config.ts
+++ b/src/lib/config.ts
@@ -1,6 +1,7 @@
 import fs from "node:fs";
 import os from "node:os";
 import path from "node:path";
+import { parsePortRange } from "./hosts.js";
 
 // Default the data root somewhere the running user can actually write.
 //
@@ -15,10 +16,25 @@ import path from "node:path";
 const root =
   process.env.DATA_DIR ?? path.join(os.homedir(), "nexus-workspace", "deploy");
 
+const baseDomain = process.env.BASE_DOMAIN ?? "localhost";
+
 export const config = {
   port: Number(process.env.PORT ?? 3000),
   baseUrl: process.env.BASE_URL ?? "http://localhost:3000",
-  baseDomain: process.env.BASE_DOMAIN ?? "localhost",
+  baseDomain,
+  // Where the dashboard, the API and the short links (<controlHost>/<slug>) live.
+  controlHost: process.env.CONTROL_HOST ?? `deploy.${baseDomain}`,
+  // Where one app is reachable. `{slug}` and `{BASE_DOMAIN}` are substituted.
+  appHostPattern: process.env.APP_HOST_PATTERN ?? "{slug}.{BASE_DOMAIN}",
+  // Loopback ports apps are published on. The ingress reaches an app only here.
+  appPortRange: parsePortRange(process.env.APP_PORT_RANGE ?? "20000-29999"),
+  // The rootless sandbox daemon, e.g. unix:///run/user/1001/docker.sock. Empty
+  // means "not configured": every Docker call then refuses to run, so a
+  // misconfigured node can never fall back to the host's own daemon.
+  sandboxDockerHost: process.env.SANDBOX_DOCKER_HOST ?? "",
+  // Mount point of the sandbox's size-capped storage. Builds are refused when it,
+  // or the data directory's volume, is nearly full.
+  sandboxStoragePath: process.env.SANDBOX_STORAGE_PATH ?? "",
   dataDir: root,
   dbPath: path.join(root, "nexus-deploy.json"),
   appSecret: process.env.APP_SECRET ?? "change-me",
````

**Create `src/lib/hosts.ts`:**

````typescript
/**
 * Where an app is reachable, and which loopback ports it may use.
 *
 * Every node-specific value comes in as an argument so that nothing here names
 * a particular domain: the same code runs on any node with a different
 * BASE_DOMAIN. tests/no-hardcoded-domain.test.ts enforces that for all of src/.
 */

/** `{slug}` and `{BASE_DOMAIN}` are the only substitutions. */
export function appHost(slug: string, pattern: string, baseDomain: string): string {
  if (!pattern.includes("{slug}")) {
    throw new Error("APP_HOST_PATTERN must contain {slug}");
  }
  return pattern.replaceAll("{slug}", slug).replaceAll("{BASE_DOMAIN}", baseDomain);
}

export function appUrl(slug: string, pattern: string, baseDomain: string): string {
  return `https://${appHost(slug, pattern, baseDomain)}`;
}

export type PortRange = { min: number; max: number };

/** "20000-29999" → { min: 20000, max: 29999 }. Ports below 1024 are never allowed. */
export function parsePortRange(raw: string): PortRange {
  const match = /^(\d{1,5})-(\d{1,5})$/.exec(raw.trim());
  const min = match ? Number(match[1]) : Number.NaN;
  const max = match ? Number(match[2]) : Number.NaN;
  if (!match || min < 1024 || max > 65535 || min > max) {
    throw new Error(`APP_PORT_RANGE must look like 20000-29999 (got "${raw}")`);
  }
  return { min, max };
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **22 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/config.ts src/lib/hosts.ts tests/hosts.test.ts tests/no-hardcoded-domain.test.ts
git commit \
  -m "feat(config): node-specific settings and host helpers" \
  -m "The app domain pattern, control host, loopback port range and sandbox socket are
configuration, so any node can run the same code. A test fails if a source file
ever hardcodes a domain." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 2: Slug rules and the fail-closed name check

A slug becomes a hostname and a short link, on an apex shared with ecosystem hosts and Hosting's member sites. It is accepted only when well formed, not reserved, not taken here, and not taken anywhere else we can ask. If the outside registry cannot be asked, the name is refused.

**Files:**

- Create: `src/lib/names.ts`
- Create: `tests/names.test.ts`

**Interfaces:**

- Consumes: Nothing.
- Produces: `SLUG_RE`, `RESERVED_SLUGS`, `slugify(input): string`, `checkSlug(slug, deps: NameDeps): Promise<NameCheck>`, types `NameDeps` and `NameCheck`.

- [ ] **Step 1: Write the tests**

**Create `tests/names.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { checkSlug, slugify, SLUG_RE, type NameDeps } from "../src/lib/names.js";

const free: NameDeps = { takenLocally: () => false, takenElsewhere: async () => false };

test("slugify makes a hostname-shaped name out of free text", () => {
  assert.equal(slugify("My App!"), "my-app");
  assert.equal(slugify("  --Hello__World--  "), "hello-world");
  assert.equal(slugify("a".repeat(60)).length, 40);
  assert.equal(slugify("x".repeat(39) + "-y"), "x".repeat(39));
});

test("well-formed slugs pass the format rule and malformed ones do not", () => {
  for (const ok of ["abc", "my-app", "a1b", "web-2-api"]) assert.ok(SLUG_RE.test(ok), ok);
  for (const bad of ["", "ab", "1abc", "-abc", "abc-", "a--b", "Abc", "a_b", "a.b", "xn--abc", "a".repeat(41)]) {
    assert.ok(!SLUG_RE.test(bad), bad);
  }
});

test("a free, well-formed, unreserved name is accepted", async () => {
  assert.deepEqual(await checkSlug("my-app", free), { ok: true });
});

test("reserved names are refused, so an app can never shadow an ecosystem host", async () => {
  for (const name of ["app", "auth", "deploy", "hosting", "api", "assets"]) {
    const result = await checkSlug(name, free);
    assert.equal(result.ok, false, name);
    assert.equal((result as { reason: string }).reason, "reserved", name);
  }
});

test("a name used by another Deploy project is taken", async () => {
  const result = await checkSlug("my-app", { ...free, takenLocally: (s) => s === "my-app" });
  assert.deepEqual(result.ok, false);
  assert.equal((result as { reason: string }).reason, "taken");
});

test("a name used elsewhere (Cloud's registry or a Hosting site) is taken", async () => {
  const result = await checkSlug("my-app", { ...free, takenElsewhere: async () => true });
  assert.equal((result as { reason: string }).reason, "taken");
});

test("if the registry cannot be asked, the name is refused rather than given out", async () => {
  const result = await checkSlug("my-app", {
    ...free,
    takenElsewhere: async () => {
      throw new Error("hosting unreachable");
    },
  });
  assert.equal(result.ok, false);
  assert.equal((result as { reason: string }).reason, "unverified");
});

test("the outside registries are not consulted for a name that is already refused", async () => {
  let asked = 0;
  const deps: NameDeps = { takenLocally: () => false, takenElsewhere: async () => { asked++; return false; } };
  await checkSlug("Bad Name", deps);
  await checkSlug("deploy", deps);
  assert.equal(asked, 0);
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/names.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/names.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/names.ts`:**

````typescript
/**
 * Who may call an app what.
 *
 * A slug becomes a hostname (`<slug>.<base domain>`) and a short link
 * (`<control host>/<slug>`), and on a shared apex it competes with ecosystem
 * hosts and with Hosting's member sites. So a slug is accepted only when it is
 * well formed, not reserved, and not already used anywhere we can ask.
 */

/** Lowercase DNS label: starts with a letter, no "--" (punycode lives there). */
export const SLUG_RE = /^(?!.*--)[a-z][a-z0-9-]{1,38}[a-z0-9]$/;

/** Hosts the ecosystem uses, and paths the dashboard serves on the control host. */
export const RESERVED_SLUGS: ReadonlySet<string> = new Set([
  // ecosystem hosts
  "app", "auth", "api", "cloud", "chat", "mail", "email", "draw", "calendar", "project",
  "hosting", "deploy", "dashboard", "status", "storage", "www", "admin", "terminal",
  "docs", "vault", "forge", "edge", "router", "guardian", "tunnel", "tnhc", "nexus",
  "phantom", "support", "help", "blog", "community", "git",
  // control-host paths
  "assets", "health", "internal", "login", "logout", "static", "favicon", "robots",
  "index", "webhooks", "events", "projects", "deployments", "activity",
]);

/** "My App!" → "my-app". Not guaranteed valid: pass the result to checkSlug. */
export function slugify(input: string): string {
  return input
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 40)
    .replace(/-+$/, "");
}

export type NameCheck =
  | { ok: true }
  | { ok: false; reason: "invalid" | "reserved" | "taken" | "unverified"; message: string };

export type NameDeps = {
  /** Another Deploy project already owns the slug. */
  takenLocally(slug: string): boolean;
  /** Cloud's registry or Hosting already uses it. Throwing means "could not check". */
  takenElsewhere(slug: string): Promise<boolean>;
};

export async function checkSlug(slug: string, deps: NameDeps): Promise<NameCheck> {
  if (!SLUG_RE.test(slug)) {
    return {
      ok: false,
      reason: "invalid",
      message: "Use 3–40 lowercase letters, digits and single hyphens, starting with a letter.",
    };
  }
  if (RESERVED_SLUGS.has(slug)) {
    return { ok: false, reason: "reserved", message: `"${slug}" is reserved.` };
  }
  if (deps.takenLocally(slug)) {
    return { ok: false, reason: "taken", message: `"${slug}" is already taken.` };
  }
  try {
    if (await deps.takenElsewhere(slug)) {
      return { ok: false, reason: "taken", message: `"${slug}" is already taken.` };
    }
  } catch {
    // Fail closed: if we cannot tell whether a name is free, it is not given out.
    return {
      ok: false,
      reason: "unverified",
      message: "Could not check whether that name is free. Try again shortly.",
    };
  }
  return { ok: true };
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **30 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/names.ts tests/names.test.ts
git commit \
  -m "feat(names): slug rules with a fail-closed registry check" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 3: Safeguards: caps, quotas, rate limit and disk guard

Free public multi-tenancy on one machine makes abuse control load-bearing. These pure functions decide what a container may use, how many projects and builds a member may have, and when builds pause because the disk is filling. The same defaults apply to everyone; an operator changes them through `CAP_*` variables.

**Files:**

- Create: `src/lib/safeguards.ts`
- Create: `tests/safeguards.test.ts`
- Modify: `src/lib/config.ts`

**Interfaces:**

- Consumes: Nothing.
- Produces: `Limits`, `DEFAULT_LIMITS`, `limitsFromEnv(env)`, `parseMemoryMb`, `containerLimits(project, limits): ContainerLimits`, `Verdict`, `checkProjectQuota`, `checkBuildAllowed`, `countRecentBuilds`, `diskFreePercent`, and `config.limits`.

- [ ] **Step 1: Write the tests**

**Create `tests/safeguards.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import {
  checkBuildAllowed,
  checkProjectQuota,
  containerLimits,
  countRecentBuilds,
  DEFAULT_LIMITS,
  diskFreePercent,
  limitsFromEnv,
  parseMemoryMb,
} from "../src/lib/safeguards.js";

const calm = { activeBuildsForMember: 0, buildsLastHourForMember: 0, hostFreePercent: 60, sandboxUsedPercent: 20 };

test("the defaults are the documented ones", () => {
  assert.deepEqual(
    [DEFAULT_LIMITS.memoryMb, DEFAULT_LIMITS.cpus, DEFAULT_LIMITS.pids, DEFAULT_LIMITS.maxProjects],
    [512, 0.5, 256, 5],
  );
  assert.deepEqual(
    [DEFAULT_LIMITS.maxConcurrentBuilds, DEFAULT_LIMITS.maxBuildsPerHour, DEFAULT_LIMITS.buildTimeoutMs],
    [1, 6, 600_000],
  );
});

test("an operator overrides caps through CAP_* variables and junk is refused", () => {
  assert.equal(limitsFromEnv({}).memoryMb, 512);
  assert.equal(limitsFromEnv({ CAP_MEMORY_MB: "1024", CAP_MAX_PROJECTS: "9" }).memoryMb, 1024);
  assert.equal(limitsFromEnv({ CAP_MAX_PROJECTS: "9" }).maxProjects, 9);
  assert.equal(limitsFromEnv({ CAP_MEMORY_MB: "  " }).memoryMb, 512);
  for (const bad of ["0", "-1", "abc", "Infinity"]) {
    assert.throws(() => limitsFromEnv({ CAP_PIDS: bad }), /CAP_PIDS/, bad);
  }
});

test("memory sizes parse; a bare number means megabytes", () => {
  assert.equal(parseMemoryMb("512m"), 512);
  assert.equal(parseMemoryMb("1g"), 1024);
  assert.equal(parseMemoryMb("256"), 256);
  assert.equal(parseMemoryMb("1.5G"), 1536);
  for (const bad of ["", "abc", "12k", "-5m", "5 m"]) assert.equal(parseMemoryMb(bad), null, bad);
});

test("a project may lower its container limits but never raise them", () => {
  assert.deepEqual(containerLimits({}, DEFAULT_LIMITS), { memory: "512m", cpus: "0.5", pids: 256 });
  assert.deepEqual(containerLimits({ memoryLimit: "256m", cpus: "0.25" }, DEFAULT_LIMITS), {
    memory: "256m",
    cpus: "0.25",
    pids: 256,
  });
  assert.deepEqual(containerLimits({ memoryLimit: "8g", cpus: "16" }, DEFAULT_LIMITS), {
    memory: "512m",
    cpus: "0.5",
    pids: 256,
  });
  assert.deepEqual(containerLimits({ memoryLimit: "nonsense", cpus: "x" }, DEFAULT_LIMITS), {
    memory: "512m",
    cpus: "0.5",
    pids: 256,
  });
});

test("the project quota refuses the project after the last one allowed", () => {
  assert.deepEqual(checkProjectQuota(4, DEFAULT_LIMITS), { ok: true });
  const refused = checkProjectQuota(5, DEFAULT_LIMITS);
  assert.equal(refused.ok, false);
  assert.equal((refused as { code: string }).code, "quota");
});

test("a calm node allows a build", () => {
  assert.deepEqual(checkBuildAllowed(calm, DEFAULT_LIMITS), { ok: true });
});

test("each way a build can be refused has its own code", () => {
  const code = (patch: object) => {
    const verdict = checkBuildAllowed({ ...calm, ...patch }, DEFAULT_LIMITS);
    return verdict.ok ? "ok" : verdict.code;
  };
  assert.equal(code({ hostFreePercent: 14.9 }), "disk");
  assert.equal(code({ hostFreePercent: 15 }), "ok");
  assert.equal(code({ sandboxUsedPercent: 86 }), "disk");
  assert.equal(code({ sandboxUsedPercent: 85 }), "ok");
  assert.equal(code({ activeBuildsForMember: 1 }), "busy");
  assert.equal(code({ buildsLastHourForMember: 6 }), "rate");
  assert.equal(code({ buildsLastHourForMember: 5 }), "ok");
});

test("a full disk is reported before a busy member", () => {
  const verdict = checkBuildAllowed({ ...calm, hostFreePercent: 5, activeBuildsForMember: 3 }, DEFAULT_LIMITS);
  assert.equal(verdict.ok === false && verdict.code, "disk");
});

test("an unmeasured disk does not block builds", () => {
  assert.deepEqual(
    checkBuildAllowed({ ...calm, hostFreePercent: null, sandboxUsedPercent: null }, DEFAULT_LIMITS),
    { ok: true },
  );
});

test("only this member's builds in the last hour count, and rollbacks are free", () => {
  const now = 10_000_000;
  const deployments = [
    { projectId: "p1", triggeredBy: "manual", createdAt: now - 1_000 },
    { projectId: "p1", triggeredBy: "webhook", createdAt: now - 59 * 60_000 },
    { projectId: "p1", triggeredBy: "rollback", createdAt: now - 1_000 },
    { projectId: "p1", triggeredBy: "manual", createdAt: now - 61 * 60_000 },
    { projectId: "other", triggeredBy: "manual", createdAt: now - 1_000 },
  ];
  assert.equal(countRecentBuilds(deployments, new Set(["p1"]), now), 2);
});

test("free disk space is a percentage, and unreadable means unknown", () => {
  assert.equal(diskFreePercent("/x", () => ({ blocks: 1000, bavail: 250 })), 25);
  assert.equal(diskFreePercent("/x", () => ({ blocks: 0, bavail: 0 })), null);
  assert.equal(
    diskFreePercent("/x", () => {
      throw new Error("ENOENT");
    }),
    null,
  );
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/safeguards.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/safeguards.js'`.

- [ ] **Step 3: Implement**

**Modify `src/lib/config.ts`:**

````diff
--- a/src/lib/config.ts
+++ b/src/lib/config.ts
@@ -2,6 +2,7 @@ import fs from "node:fs";
 import os from "node:os";
 import path from "node:path";
 import { parsePortRange } from "./hosts.js";
+import { limitsFromEnv } from "./safeguards.js";
 
 // Default the data root somewhere the running user can actually write.
 //
@@ -35,6 +36,8 @@ export const config = {
   // Mount point of the sandbox's size-capped storage. Builds are refused when it,
   // or the data directory's volume, is nearly full.
   sandboxStoragePath: process.env.SANDBOX_STORAGE_PATH ?? "",
+  // Safeguards (CAP_* variables). The same for every member of the node.
+  limits: limitsFromEnv(process.env),
   dataDir: root,
   dbPath: path.join(root, "nexus-deploy.json"),
   appSecret: process.env.APP_SECRET ?? "change-me",
````

**Create `src/lib/safeguards.ts`:**

````typescript
/**
 * Safeguards: what keeps one member, or one runaway build, from taking the node
 * down. They are not plans or tiers. The same defaults apply to everyone, an
 * operator changes them for the whole node through CAP_* environment variables,
 * and the dashboard shows them so nobody is surprised.
 *
 * Everything here is a pure function of its inputs so it can be tested without
 * Docker, a disk or a clock.
 */
import fs from "node:fs";

export type Limits = {
  memoryMb: number;
  cpus: number;
  pids: number;
  maxProjects: number;
  maxConcurrentBuilds: number;
  maxBuildsPerHour: number;
  buildTimeoutMs: number;
  pullTimeoutMs: number;
  /** Builds are refused when a measured volume has less than this free. */
  minHostFreePercent: number;
  /** Builds are refused when the sandbox storage is fuller than this. */
  maxSandboxUsedPercent: number;
};

export const DEFAULT_LIMITS: Limits = {
  memoryMb: 512,
  cpus: 0.5,
  pids: 256,
  maxProjects: 5,
  maxConcurrentBuilds: 1,
  maxBuildsPerHour: 6,
  buildTimeoutMs: 10 * 60_000,
  pullTimeoutMs: 10 * 60_000,
  // Matches the webmaster check that already watches this node: warn at 15%.
  minHostFreePercent: 15,
  maxSandboxUsedPercent: 85,
};

function envNumber(env: Record<string, string | undefined>, key: string, fallback: number): number {
  const raw = env[key];
  if (raw === undefined || raw.trim() === "") return fallback;
  const value = Number(raw);
  if (!Number.isFinite(value) || value <= 0) {
    throw new Error(`${key} must be a positive number (got "${raw}")`);
  }
  return value;
}

export function limitsFromEnv(env: Record<string, string | undefined>): Limits {
  return {
    memoryMb: envNumber(env, "CAP_MEMORY_MB", DEFAULT_LIMITS.memoryMb),
    cpus: envNumber(env, "CAP_CPUS", DEFAULT_LIMITS.cpus),
    pids: envNumber(env, "CAP_PIDS", DEFAULT_LIMITS.pids),
    maxProjects: envNumber(env, "CAP_MAX_PROJECTS", DEFAULT_LIMITS.maxProjects),
    maxConcurrentBuilds: envNumber(env, "CAP_MAX_CONCURRENT_BUILDS", DEFAULT_LIMITS.maxConcurrentBuilds),
    maxBuildsPerHour: envNumber(env, "CAP_MAX_BUILDS_PER_HOUR", DEFAULT_LIMITS.maxBuildsPerHour),
    buildTimeoutMs: envNumber(env, "CAP_BUILD_TIMEOUT_MS", DEFAULT_LIMITS.buildTimeoutMs),
    pullTimeoutMs: envNumber(env, "CAP_PULL_TIMEOUT_MS", DEFAULT_LIMITS.pullTimeoutMs),
    minHostFreePercent: envNumber(env, "CAP_MIN_HOST_FREE_PERCENT", DEFAULT_LIMITS.minHostFreePercent),
    maxSandboxUsedPercent: envNumber(env, "CAP_MAX_SANDBOX_USED_PERCENT", DEFAULT_LIMITS.maxSandboxUsedPercent),
  };
}

/** "512m" / "1g" / "512" (megabytes) → megabytes, or null if unparseable. */
export function parseMemoryMb(value: string): number | null {
  const match = /^(\d+(?:\.\d+)?)([mg]?)$/i.exec(value.trim());
  if (!match) return null;
  const amount = Number(match[1]);
  return (match[2] ?? "").toLowerCase() === "g" ? amount * 1024 : amount;
}

export type ContainerLimits = { memory: string; cpus: string; pids: number };

/** A project may ask for less than the node's cap, never more. */
export function containerLimits(
  project: { memoryLimit?: string | undefined; cpus?: string | undefined },
  limits: Limits,
): ContainerLimits {
  const askedMb = project.memoryLimit ? parseMemoryMb(project.memoryLimit) : null;
  const mb = askedMb !== null && askedMb > 0 ? Math.min(askedMb, limits.memoryMb) : limits.memoryMb;
  const askedCpus = project.cpus ? Number(project.cpus) : Number.NaN;
  const cpus = Number.isFinite(askedCpus) && askedCpus > 0 ? Math.min(askedCpus, limits.cpus) : limits.cpus;
  return { memory: `${Math.floor(mb)}m`, cpus: String(cpus), pids: limits.pids };
}

export type Verdict =
  | { ok: true }
  | { ok: false; code: "quota" | "busy" | "rate" | "disk"; message: string };

export function checkProjectQuota(ownedProjects: number, limits: Limits): Verdict {
  if (ownedProjects >= limits.maxProjects) {
    return {
      ok: false,
      code: "quota",
      message: `You can have up to ${limits.maxProjects} projects on this node. Delete one to make room.`,
    };
  }
  return { ok: true };
}

export type BuildContextInputs = {
  activeBuildsForMember: number;
  buildsLastHourForMember: number;
  /** Lowest free percentage over the measured host volumes; null = not measured. */
  hostFreePercent: number | null;
  /** How full the sandbox storage is; null = not measured. */
  sandboxUsedPercent: number | null;
};

/** Disk is checked first: a full disk takes production down, a busy member does not. */
export function checkBuildAllowed(input: BuildContextInputs, limits: Limits): Verdict {
  if (input.hostFreePercent !== null && input.hostFreePercent < limits.minHostFreePercent) {
    return {
      ok: false,
      code: "disk",
      message: "This node is low on disk space, so new builds are paused. Try again later.",
    };
  }
  if (input.sandboxUsedPercent !== null && input.sandboxUsedPercent > limits.maxSandboxUsedPercent) {
    return {
      ok: false,
      code: "disk",
      message: "The build storage on this node is nearly full, so new builds are paused.",
    };
  }
  if (input.activeBuildsForMember >= limits.maxConcurrentBuilds) {
    return { ok: false, code: "busy", message: "One of your builds is still running. Wait for it to finish." };
  }
  if (input.buildsLastHourForMember >= limits.maxBuildsPerHour) {
    return {
      ok: false,
      code: "rate",
      message: `You have reached ${limits.maxBuildsPerHour} builds in the last hour. Try again later.`,
    };
  }
  return { ok: true };
}

const HOUR_MS = 60 * 60_000;

/** Builds a member started in the last hour. Rollbacks reuse an image and are free. */
export function countRecentBuilds(
  deployments: ReadonlyArray<{ projectId: string; triggeredBy: string; createdAt: number }>,
  memberProjectIds: ReadonlySet<string>,
  now: number,
): number {
  return deployments.filter(
    (d) => memberProjectIds.has(d.projectId) && d.triggeredBy !== "rollback" && now - d.createdAt < HOUR_MS,
  ).length;
}

type StatFs = (path: string) => { blocks: number; bavail: number };

/** Free space of the volume holding `path`, 0–100, or null when it cannot be read. */
export function diskFreePercent(path: string, statfs: StatFs = fs.statfsSync as unknown as StatFs): number | null {
  try {
    const stats = statfs(path);
    return stats.blocks > 0 ? (stats.bavail / stats.blocks) * 100 : null;
  } catch {
    return null;
  }
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **41 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/config.ts src/lib/safeguards.ts tests/safeguards.test.ts
git commit \
  -m "feat(safeguards): container caps, project quota, build rate limit and disk guard" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 4: Members, operators and project ownership

Nexus-Auth proves who someone is; an operator decides who may use Deploy. A member sees only their own projects, and an operator sees all. The first founder to arrive on a fresh node becomes the owner.

**Files:**

- Create: `src/lib/members.ts`
- Create: `tests/members.test.ts`
- Modify: `src/lib/store.ts`
- Modify: `src/types.ts`

**Interfaces:**

- Consumes: Nothing.
- Produces: `Member`, `MemberRole`, `MemberStatus`, `Project.ownerId`, `Database.members`; `isOperator`, `resolveAccess`, `bootstrapOwner`, `canAccessProject`, `adoptOrphanProjects`, `grantMember`, `setMemberStatus`, `Identity`, `Access`, `GrantedAccess`.

- [ ] **Step 1: Write the tests**

**Create `tests/members.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import {
  adoptOrphanProjects,
  bootstrapOwner,
  canAccessProject,
  grantMember,
  isOperator,
  resolveAccess,
  setMemberStatus,
} from "../src/lib/members.js";
import type { Member, Project } from "../src/types.js";

const member = (authUserId: string, patch: Partial<Member> = {}): Member => ({
  authUserId,
  role: "member",
  status: "active",
  grantedBy: "boss",
  grantedAt: 1,
  ...patch,
});

test("founders and admins are operators; nobody else is", () => {
  assert.equal(isOperator({ id: "a", role: "founder" }), true);
  assert.equal(isOperator({ id: "a", role: "admin" }), true);
  assert.equal(isOperator({ id: "a", role: "user" }), false);
  assert.equal(isOperator({ id: "a" }), false);
});

test("an account nobody let in is refused", () => {
  assert.deepEqual(resolveAccess([], { id: "u1", role: "user" }), { ok: false, reason: "not_a_member" });
});

test("a granted member is let in, without operator rights", () => {
  assert.deepEqual(resolveAccess([member("u1")], { id: "u1", role: "user" }), {
    ok: true,
    operator: false,
    memberId: "u1",
  });
});

test("operators are let in without being on the list", () => {
  assert.deepEqual(resolveAccess([], { id: "boss", role: "admin" }), { ok: true, operator: true, memberId: "boss" });
});

test("suspension shuts an account out, even an operator's", () => {
  const members = [member("u1", { status: "suspended" }), member("op", { status: "suspended" })];
  assert.deepEqual(resolveAccess(members, { id: "u1", role: "user" }), { ok: false, reason: "suspended" });
  assert.deepEqual(resolveAccess(members, { id: "op", role: "admin" }), { ok: false, reason: "suspended" });
});

test("the first founder becomes the owner, once", () => {
  const members: Member[] = [];
  const owner = bootstrapOwner(members, { id: "zajfan", role: "founder" }, 5);
  assert.equal(owner?.role, "owner");
  assert.equal(owner?.authUserId, "zajfan");
  assert.equal(members.length, 1);
  assert.equal(bootstrapOwner(members, { id: "someone", role: "founder" }, 6), null);
  assert.equal(members.length, 1);
});

test("an admin or an ordinary user cannot claim an empty node", () => {
  const members: Member[] = [];
  assert.equal(bootstrapOwner(members, { id: "a", role: "admin" }, 1), null);
  assert.equal(bootstrapOwner(members, { id: "u", role: "user" }, 1), null);
  assert.equal(members.length, 0);
});

test("a member sees their own projects, an operator sees all, nobody sees an unowned one by accident", () => {
  const mine = { ownerId: "u1" };
  const theirs = { ownerId: "u2" };
  const orphan = {};
  const asMember = { ok: true as const, operator: false, memberId: "u1" };
  const asOperator = { ok: true as const, operator: true, memberId: "boss" };
  assert.equal(canAccessProject(mine, asMember), true);
  assert.equal(canAccessProject(theirs, asMember), false);
  assert.equal(canAccessProject(orphan, asMember), false);
  assert.equal(canAccessProject(theirs, asOperator), true);
  assert.equal(canAccessProject(orphan, asOperator), true);
});

test("projects from before ownership are adopted by the owner, not by whoever asks", () => {
  const projects = [{ id: "a" }, { id: "b", ownerId: "u9" }] as Project[];
  assert.equal(adoptOrphanProjects(projects, "zajfan"), 1);
  assert.deepEqual(
    projects.map((p) => p.ownerId),
    ["zajfan", "u9"],
  );
});

test("granting twice does not duplicate, and re-activates a suspended member", () => {
  const members = [member("u1", { status: "suspended" })];
  const again = grantMember(members, { id: "boss", role: "admin" }, { authUserId: "u1", label: "Ada" }, 9);
  assert.equal(members.length, 1);
  assert.equal(again.status, "active");
  assert.equal(again.label, "Ada");
  const fresh = grantMember(members, { id: "boss", role: "admin" }, { authUserId: "u2" }, 9);
  assert.equal(members.length, 2);
  assert.equal(fresh.grantedBy, "boss");
  assert.equal(fresh.role, "member");
});

test("an owner cannot be suspended, so the node cannot lock out its own operators", () => {
  const members = [member("o", { role: "owner" }), member("u1")];
  assert.equal(setMemberStatus(members, "o", "suspended"), "is_owner");
  assert.equal(members[0]?.status, "active");
  assert.equal(setMemberStatus(members, "u1", "suspended"), "ok");
  assert.equal(members[1]?.status, "suspended");
  assert.equal(setMemberStatus(members, "missing", "active"), "not_found");
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/members.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/members.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/members.ts`:**

````typescript
/**
 * Who may use this node's Deploy, and which projects they may touch.
 *
 * Identity belongs to Nexus-Auth. Here an operator (an Auth founder or admin)
 * lets individual accounts in; there is no separate signup. Operators can always
 * use Deploy and see every project. Everyone else sees only their own.
 */
import type { Member, MemberStatus, Project } from "../types.js";

export type Identity = {
  id: string;
  role?: string | undefined;
  username?: string | undefined;
  email?: string | undefined;
};

export function isOperator(identity: Identity): boolean {
  return identity.role === "founder" || identity.role === "admin";
}

export type GrantedAccess = { ok: true; operator: boolean; memberId: string };
export type Access = GrantedAccess | { ok: false; reason: "not_a_member" | "suspended" };

export function resolveAccess(members: readonly Member[], identity: Identity): Access {
  const member = members.find((m) => m.authUserId === identity.id);
  // Suspension wins over everything, including operator status: it is how an
  // operator shuts one account out, and the check must not have a back door.
  if (member?.status === "suspended") return { ok: false, reason: "suspended" };
  if (isOperator(identity)) return { ok: true, operator: true, memberId: identity.id };
  if (member?.status === "active") return { ok: true, operator: false, memberId: identity.id };
  return { ok: false, reason: "not_a_member" };
}

/**
 * Make the first founder to arrive the owner. Only when nobody is a member yet,
 * and only for a founder, so a fresh node cannot be claimed by a lesser role.
 */
export function bootstrapOwner(members: Member[], identity: Identity, now: number): Member | null {
  if (members.length > 0 || identity.role !== "founder") return null;
  const owner: Member = {
    authUserId: identity.id,
    role: "owner",
    status: "active",
    grantedBy: identity.id,
    grantedAt: now,
  };
  members.push(owner);
  return owner;
}

export function canAccessProject(project: Pick<Project, "ownerId">, access: GrantedAccess): boolean {
  return access.operator || (project.ownerId !== undefined && project.ownerId === access.memberId);
}

/** Give projects from before ownership existed to `ownerId`. Returns how many moved. */
export function adoptOrphanProjects(projects: Project[], ownerId: string): number {
  let adopted = 0;
  for (const project of projects) {
    if (project.ownerId === undefined) {
      project.ownerId = ownerId;
      adopted++;
    }
  }
  return adopted;
}

/** Granting again re-activates a suspended member rather than duplicating them. */
export function grantMember(
  members: Member[],
  actor: Identity,
  target: { authUserId: string; label?: string },
  now: number,
): Member {
  const existing = members.find((m) => m.authUserId === target.authUserId);
  if (existing) {
    existing.status = "active";
    if (target.label !== undefined) existing.label = target.label;
    return existing;
  }
  const member: Member = {
    authUserId: target.authUserId,
    role: "member",
    status: "active",
    grantedBy: actor.id,
    grantedAt: now,
    ...(target.label !== undefined ? { label: target.label } : {}),
  };
  members.push(member);
  return member;
}

export function setMemberStatus(
  members: Member[],
  authUserId: string,
  status: MemberStatus,
): "ok" | "not_found" | "is_owner" {
  const member = members.find((m) => m.authUserId === authUserId);
  if (!member) return "not_found";
  // An owner cannot be suspended, or the node could lock out everyone who can fix it.
  if (member.role === "owner" && status === "suspended") return "is_owner";
  member.status = status;
  return "ok";
}
````

**Modify `src/lib/store.ts`:**

````diff
--- a/src/lib/store.ts
+++ b/src/lib/store.ts
@@ -8,13 +8,14 @@
 import fs from "node:fs";
 import { config, ensureDataDir } from "./config.js";
 import { decryptJson, encryptJson } from "./crypto.js";
-import type { Deployment, Project, Session, User } from "../types.js";
+import type { Deployment, Member, Project, Session, User } from "../types.js";
 
 export type Database = {
   users: User[];
   projects: Project[];
   deployments: Deployment[];
   sessions: Session[];
+  members: Member[];
 };
 
 const emptyDb: Database = {
@@ -22,6 +23,7 @@ const emptyDb: Database = {
   projects: [],
   deployments: [],
   sessions: [],
+  members: [],
 };
 
 // ── Read ───────────────────────────────────────────────────────────────────
@@ -32,6 +34,7 @@ export function loadDb(): Database {
   if (!raw.trim()) return structuredClone(emptyDb);
   try {
     const db = decryptJson<Database>(config.appSecret, raw);
+    db.members ??= [];
     for (const project of db.projects) {
       if (project.buildContext === undefined) project.buildContext = ".";
     }
````

**Modify `src/types.ts`:**

````diff
--- a/src/types.ts
+++ b/src/types.ts
@@ -8,10 +8,25 @@ export type User = {
   createdAt: number;
 };
 
+export type MemberRole = "owner" | "member";
+export type MemberStatus = "active" | "suspended";
+
+/** Someone an operator has let use this node's Deploy. Identity itself lives in Nexus-Auth. */
+export type Member = {
+  authUserId: string;
+  role: MemberRole;
+  status: MemberStatus;
+  grantedBy: string;
+  grantedAt: number;
+  label?: string;
+};
+
 export type ProjectStatus = "idle" | "building" | "live" | "failed" | "stopped";
 
 export type Project = {
   id: string;
+  /** Nexus-Auth id of the member who owns it. Absent only on projects from before ownership existed. */
+  ownerId?: string;
   name: string;
   repo: string;
   branch: string;
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **52 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/members.ts src/lib/store.ts src/types.ts tests/members.test.ts
git commit \
  -m "feat(members): invite-only access with operators and per-project ownership" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 5: Deploy sources, private-repository credentials, redaction and env validation

A project has exactly one source: a Git repository or a container image. Both are input from a member, so both are validated before storage. A private-repository token reaches git through a helper script and the child's environment, never through a URL, argument, log or API response.

**Files:**

- Create: `src/lib/env.ts`
- Create: `src/lib/source.ts`
- Create: `tests/source.test.ts`
- Modify: `src/types.ts`

**Interfaces:**

- Consumes: `Project` from `src/types.ts`.
- Produces: `GitSource`, `ImageSource`, `Source`, `Project.source`; `sourceOf`, `validateImageRef`, `pinnedImageRef`, `validateBranch`, `validateToken`, `gitCredentialEnv(token, workDir): { env, cleanup }`, `redact(line, secrets)`, `publicProject`, `PublicProject`; `validateEnv`, `toEnvFile`.

- [ ] **Step 1: Write the tests**

**Create `tests/source.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { spawnSync } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import { test } from "node:test";
import { toEnvFile, validateEnv } from "../src/lib/env.js";
import {
  gitCredentialEnv,
  pinnedImageRef,
  publicProject,
  redact,
  sourceOf,
  validateBranch,
  validateImageRef,
  validateToken,
} from "../src/lib/source.js";
import type { Project } from "../src/types.js";

const DIGEST = `sha256:${"a".repeat(64)}`;

test("image references: real ones are accepted", () => {
  for (const ok of [
    "nginx",
    "nginx:1.27",
    "library/nginx:latest",
    "ghcr.io/org/app:v1.2.3",
    "localhost:5000/foo/bar:tag",
    "registry.example.test:8443/team/app_name.v2",
    `repo/app@${DIGEST}`,
    `ghcr.io/org/app:1.0@${DIGEST}`,
  ]) {
    assert.equal(validateImageRef(ok), ok);
  }
});

test("image references: anything a shell or docker option could read is refused", () => {
  for (const bad of [
    "",
    "a b",
    "nginx;rm -rf /",
    "$(id)",
    "`id`",
    "nginx\nbusybox",
    "-v /:/host nginx",
    "--privileged",
    "Nginx",
    "nginx:",
    "nginx@sha256:short",
    `${"a".repeat(256)}`,
    "ghcr.io//app",
  ]) {
    assert.throws(() => validateImageRef(bad), /image must look like/, JSON.stringify(bad));
  }
  assert.throws(() => validateImageRef(undefined), /image must look like/);
  assert.throws(() => validateImageRef(42), /image must look like/);
});

test("pinning replaces the tag with the exact digest", () => {
  assert.equal(pinnedImageRef("nginx:1.27", DIGEST), `nginx@${DIGEST}`);
  assert.equal(pinnedImageRef("localhost:5000/foo/bar:tag", DIGEST), `localhost:5000/foo/bar@${DIGEST}`);
  assert.equal(pinnedImageRef(`ghcr.io/o/a:1@${DIGEST}`, DIGEST), `ghcr.io/o/a@${DIGEST}`);
  assert.equal(pinnedImageRef("busybox", DIGEST), `busybox@${DIGEST}`);
  assert.throws(() => pinnedImageRef("nginx", "latest"), /digest/);
});

test("branches are plain names, never options or traversal", () => {
  for (const ok of ["main", "release/1.2", "feat_x-1", "v2.0"]) assert.equal(validateBranch(ok), ok);
  for (const bad of ["", "-x", "--upload-pack=sh", "a..b", "a b", "a;b", "x".repeat(101)]) {
    assert.throws(() => validateBranch(bad), /branch must be/, bad);
  }
});

test("tokens: sane length, no whitespace", () => {
  assert.equal(validateToken("ghp_abcdef123456"), "ghp_abcdef123456");
  for (const bad of ["", "abc", "with space", "line\nbreak", "x".repeat(513), undefined]) {
    assert.throws(() => validateToken(bad), /token must be/);
  }
});

test("an older project with only repo and branch is a Git project", () => {
  assert.deepEqual(sourceOf({ repo: "o/r", branch: "main" }), { type: "git", repo: "o/r", branch: "main" });
  assert.deepEqual(sourceOf({ repo: "x", branch: "", source: { type: "image", image: "nginx" } }), {
    type: "image",
    image: "nginx",
  });
});

test("the git credential helper hands git the token without the token being in the script", () => {
  const { env, cleanup } = gitCredentialEnv("ghp_supersecret123", os.tmpdir());
  try {
    const script = env["GIT_ASKPASS"] as string;
    assert.ok(fs.existsSync(script));
    assert.notEqual(fs.statSync(script).mode & 0o100, 0, "script is executable");
    assert.equal(fs.statSync(script).mode & 0o077, 0, "script is private to the owner");
    assert.ok(!fs.readFileSync(script, "utf8").includes("ghp_supersecret123"), "the script holds no secret");
    assert.equal(env["GIT_TERMINAL_PROMPT"], "0");

    const ask = (prompt: string) =>
      spawnSync(script, [prompt], { env, encoding: "utf8" }).stdout;
    assert.equal(ask("Username for 'https://example.test': "), "x-access-token");
    assert.equal(ask("Password for 'https://x-access-token@example.test': "), "ghp_supersecret123");
  } finally {
    cleanup();
  }
});

test("user:token supplies the username, and cleanup removes the helper", () => {
  const { env, cleanup } = gitCredentialEnv("oauth2:glpat-abcdef", os.tmpdir());
  const script = env["GIT_ASKPASS"] as string;
  assert.equal(spawnSync(script, ["Username for x"], { env, encoding: "utf8" }).stdout, "oauth2");
  assert.equal(spawnSync(script, ["Password for x"], { env, encoding: "utf8" }).stdout, "glpat-abcdef");
  cleanup();
  assert.ok(!fs.existsSync(script));
});

test("with no token, git is simply told never to prompt", () => {
  const { env, cleanup } = gitCredentialEnv(undefined);
  assert.deepEqual(env, { GIT_TERMINAL_PROMPT: "0" });
  cleanup();
});

test("secrets are replaced wherever they appear in a log line", () => {
  assert.equal(redact("clone https://u:ghp_abc123@host failed: ghp_abc123", ["ghp_abc123"]), "clone https://u:***@host failed: ***");
  assert.equal(redact("nothing to hide", ["ghp_abc123", undefined, ""]), "nothing to hide");
  assert.equal(redact("ab is too short to redact", ["ab"]), "ab is too short to redact");
});

test("the API shows that a token exists but never its value", () => {
  const project = {
    id: "p",
    repo: "o/r",
    branch: "main",
    source: { type: "git", repo: "o/r", branch: "main", token: "ghp_topsecret" },
    webhookSecret: "w",
  } as unknown as Project;
  const shown = publicProject(project);
  assert.deepEqual(shown.source, { type: "git", repo: "o/r", branch: "main", hasToken: true });
  assert.ok(!JSON.stringify(shown).includes("ghp_topsecret"));
  assert.deepEqual(publicProject({ ...project, source: { type: "git", repo: "o/r", branch: "main" } }).source, {
    type: "git",
    repo: "o/r",
    branch: "main",
    hasToken: false,
  });
});

test("environment variables: names and values that docker's env file cannot hold are refused", () => {
  assert.deepEqual(validateEnv({ A: "1", _B2: "two words" }), { A: "1", _B2: "two words" });
  assert.throws(() => validateEnv([]), /object/);
  assert.throws(() => validateEnv(null), /object/);
  assert.throws(() => validateEnv({ "1A": "x" }), /valid environment variable name/);
  assert.throws(() => validateEnv({ "A B": "x" }), /valid environment variable name/);
  assert.throws(() => validateEnv({ A: 1 }), /must be a string/);
  assert.throws(() => validateEnv({ A: "line\nbreak" }), /line break/);
  assert.throws(() => validateEnv({ A: "x".repeat(4097) }), /longer than/);
  assert.throws(() => validateEnv(Object.fromEntries(Array.from({ length: 101 }, (_, i) => [`K${i}`, "v"]))), /at most/);
});

test("the env file is one name=value per line", () => {
  assert.equal(toEnvFile({ A: "1", B: "x=y" }), "A=1\nB=x=y\n");
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/source.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/source.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/env.ts`:**

````typescript
/**
 * Environment variables a member sets on a project. They end up in an env file
 * read by `docker run --env-file`, which has no quoting: a newline would end one
 * variable and start another. So a value may not contain one.
 */
const KEY_RE = /^[A-Za-z_][A-Za-z0-9_]{0,127}$/;
const MAX_ENTRIES = 100;
const MAX_VALUE = 4096;

export function validateEnv(input: unknown): Record<string, string> {
  if (typeof input !== "object" || input === null || Array.isArray(input)) {
    throw new Error("env must be an object of names to string values");
  }
  const entries = Object.entries(input as Record<string, unknown>);
  if (entries.length > MAX_ENTRIES) throw new Error(`env may have at most ${MAX_ENTRIES} variables`);
  const out: Record<string, string> = {};
  for (const [key, value] of entries) {
    if (!KEY_RE.test(key)) throw new Error(`"${key}" is not a valid environment variable name`);
    if (typeof value !== "string") throw new Error(`${key} must be a string`);
    if (value.length > MAX_VALUE) throw new Error(`${key} is longer than ${MAX_VALUE} characters`);
    if (/[\n\r\0]/.test(value)) throw new Error(`${key} must not contain a line break`);
    out[key] = value;
  }
  return out;
}

/** Name=value lines for `docker run --env-file`. */
export function toEnvFile(env: Record<string, string>): string {
  return `${Object.entries(env)
    .map(([key, value]) => `${key}=${value}`)
    .join("\n")}\n`;
}
````

**Create `src/lib/source.ts`:**

````typescript
/**
 * Where a project's code comes from, and the care that takes.
 *
 * Two sources: a Git repository (cloned and built) and a container image
 * (pulled). Both are input from a member, so both are validated before they are
 * stored, and neither is ever passed to a shell. A private-repository token is
 * handled so that it appears in no URL, no command line, no log and no API
 * response.
 */
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import type { GitSource, ImageSource, Project, Source } from "../types.js";

/** The authoritative source, falling back to the repo/branch of an older project. */
export function sourceOf(project: Pick<Project, "repo" | "branch" | "source">): Source {
  return project.source ?? { type: "git", repo: project.repo, branch: project.branch };
}

// ── Image references ───────────────────────────────────────────────────────
const COMPONENT = "[a-z0-9]+(?:(?:[._]|__|-+)[a-z0-9]+)*";
const LABEL = "(?:[a-zA-Z0-9]|[a-zA-Z0-9][a-zA-Z0-9-]*[a-zA-Z0-9])";
const DOMAIN = `${LABEL}(?:\\.${LABEL})*(?::[0-9]+)?`;
const IMAGE_RE = new RegExp(
  `^(?:${DOMAIN}/)?${COMPONENT}(?:/${COMPONENT})*` +
    "(?::[A-Za-z0-9_][A-Za-z0-9_.-]{0,127})?(?:@sha256:[a-f0-9]{64})?$",
);

/**
 * `[registry[:port]/]repository[:tag][@sha256:digest]`, nothing else. The first
 * character must be a letter or digit, so a reference can never be read as a
 * command-line option, and no character that a shell treats specially is allowed.
 */
export function validateImageRef(ref: unknown): string {
  if (typeof ref !== "string" || ref.length === 0 || ref.length > 255 || !IMAGE_RE.test(ref)) {
    throw new Error("image must look like [registry/]name[:tag] or name@sha256:<digest>");
  }
  return ref;
}

/** `nginx:1.27` + a digest → `nginx@sha256:…`: the exact bytes, whatever the tag does later. */
export function pinnedImageRef(ref: string, digest: string): string {
  if (!/^sha256:[a-f0-9]{64}$/.test(digest)) throw new Error("not a sha256 digest");
  const name = ref.split("@")[0] ?? ref;
  const colon = name.indexOf(":", name.lastIndexOf("/") + 1);
  return `${colon === -1 ? name : name.slice(0, colon)}@${digest}`;
}

// ── Git sources ────────────────────────────────────────────────────────────
const BRANCH_RE = /^[A-Za-z0-9][A-Za-z0-9._/-]{0,99}$/;

export function validateBranch(branch: unknown): string {
  if (typeof branch !== "string" || !BRANCH_RE.test(branch) || branch.includes("..")) {
    throw new Error("branch must be a plain branch name such as main or release/1.2");
  }
  return branch;
}

export function validateToken(token: unknown): string {
  if (typeof token !== "string" || token.length < 4 || token.length > 512 || /\s/.test(token)) {
    throw new Error("token must be 4–512 characters with no whitespace");
  }
  return token;
}

/**
 * Environment for a `git` child process that needs to authenticate. The
 * credential travels in the child's environment and is handed to git by a small
 * helper script that holds no secret itself, so it is on no command line and in
 * no URL. "user:token" supplies a username; a bare token uses `x-access-token`,
 * which GitHub accepts for personal access tokens.
 */
export function gitCredentialEnv(
  token: string | undefined,
  workDir: string = os.tmpdir(),
): { env: Record<string, string>; cleanup: () => void } {
  const base: Record<string, string> = { GIT_TERMINAL_PROMPT: "0" };
  if (!token) return { env: base, cleanup: () => {} };

  const colon = token.indexOf(":");
  const user = colon > 0 ? token.slice(0, colon) : "x-access-token";
  const secret = colon > 0 ? token.slice(colon + 1) : token;

  const dir = fs.mkdtempSync(path.join(workDir, "nexus-askpass-"));
  const script = path.join(dir, "askpass.sh");
  fs.writeFileSync(
    script,
    '#!/bin/sh\ncase "$1" in\n  [Uu]sername*) printf "%s" "$NEXUS_GIT_USER" ;;\n  *) printf "%s" "$NEXUS_GIT_SECRET" ;;\nesac\n',
    { mode: 0o700 },
  );
  return {
    env: { ...base, GIT_ASKPASS: script, NEXUS_GIT_USER: user, NEXUS_GIT_SECRET: secret },
    cleanup: () => fs.rmSync(dir, { recursive: true, force: true }),
  };
}

/** Replace every secret with `***` before a line is stored or streamed. */
export function redact(line: string, secrets: readonly (string | undefined)[]): string {
  let out = line;
  for (const secret of secrets) {
    if (secret && secret.length >= 4) out = out.split(secret).join("***");
  }
  return out;
}

// ── What the API may show ──────────────────────────────────────────────────
export type PublicSource =
  | { type: "git"; repo: string; branch: string; hasToken: boolean }
  | ImageSource;

export type PublicProject = Omit<Project, "source"> & { source: PublicSource };

/** A project as returned to its owner: the token is reported, never repeated. */
export function publicProject(project: Project): PublicProject {
  const source = sourceOf(project);
  const shown: PublicSource =
    source.type === "git"
      ? { type: "git", repo: source.repo, branch: source.branch, hasToken: Boolean((source as GitSource).token) }
      : source;
  return { ...project, source: shown };
}
````

**Modify `src/types.ts`:**

````diff
--- a/src/types.ts
+++ b/src/types.ts
@@ -21,6 +21,11 @@ export type Member = {
   label?: string;
 };
 
+/** Exactly one source per project. A Git source is cloned and built; an image source is pulled. */
+export type GitSource = { type: "git"; repo: string; branch: string; token?: string };
+export type ImageSource = { type: "image"; image: string };
+export type Source = GitSource | ImageSource;
+
 export type ProjectStatus = "idle" | "building" | "live" | "failed" | "stopped";
 
 export type Project = {
@@ -28,8 +33,12 @@ export type Project = {
   /** Nexus-Auth id of the member who owns it. Absent only on projects from before ownership existed. */
   ownerId?: string;
   name: string;
+  /** Display copy of the source: the repo URL, or the image reference for an image project. */
   repo: string;
+  /** Branch for a Git source; empty for an image project. */
   branch: string;
+  /** Authoritative source. Absent on projects from before sources existed: see sourceOf(). */
+  source?: Source;
   buildContext: string;
   dockerfile?: string;      // relative to the repo root; default <buildContext>/Dockerfile
   healthPath?: string;      // must answer 2xx before a release is live; default: any non-5xx on "/"
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **65 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/env.ts src/lib/source.ts src/types.ts tests/source.test.ts
git commit \
  -m "feat(source): git and image sources, private-repo credentials that never leak, env validation" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 6: Loopback port allocator

Each running app is published on a loopback port. A blue/green release needs a second port while the first still serves, so ports are handed out one release at a time and checked against the operating system.

**Files:**

- Create: `src/lib/ports.ts`
- Create: `tests/ports.test.ts`

**Interfaces:**

- Consumes: `PortRange` from `src/lib/hosts.ts` (Task 1).
- Produces: `allocatePort(range, used, isFree, start?): Promise<number | null>`, `loopbackPortFree(port): Promise<boolean>`, `portsInUse(projects): Set<number>`.

- [ ] **Step 1: Write the tests**

**Create `tests/ports.test.ts`:**

````typescript
import assert from "node:assert/strict";
import net from "node:net";
import { test } from "node:test";
import { allocatePort, loopbackPortFree, portsInUse } from "../src/lib/ports.js";

const yes = async () => true;

test("a free port in the range is chosen, starting where asked", async () => {
  assert.equal(await allocatePort({ min: 20000, max: 20009 }, new Set(), yes, 3), 20003);
});

test("ports already in use are skipped, wrapping around the end of the range", async () => {
  const used = new Set([20008, 20009, 20000]);
  assert.equal(await allocatePort({ min: 20000, max: 20009 }, used, yes, 8), 20001);
});

test("ports the operating system says are taken are skipped too", async () => {
  const busy = new Set([20000, 20001]);
  assert.equal(await allocatePort({ min: 20000, max: 20003 }, new Set(), async (p) => !busy.has(p), 0), 20002);
});

test("when the whole range is taken there is no port", async () => {
  assert.equal(await allocatePort({ min: 20000, max: 20002 }, new Set([20000, 20001, 20002]), yes, 0), null);
  assert.equal(await allocatePort({ min: 20000, max: 20002 }, new Set(), async () => false, 0), null);
});

test("a single-port range works", async () => {
  assert.equal(await allocatePort({ min: 31000, max: 31000 }, new Set(), yes), 31000);
});

test("ports held by projects, current or reserved, are all counted", () => {
  const used = portsInUse([{ hostPort: 20001 }, { pendingPort: 20002 }, { hostPort: 20003, pendingPort: 20004 }, {}]);
  assert.deepEqual([...used].sort(), [20001, 20002, 20003, 20004]);
});

test("the real check sees a port that something is listening on", async () => {
  const server = net.createServer();
  await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
  const { port } = server.address() as net.AddressInfo;
  assert.equal(await loopbackPortFree(port), false);
  await new Promise<void>((resolve) => server.close(() => resolve()));
  assert.equal(await loopbackPortFree(port), true);
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/ports.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/ports.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/ports.ts`:**

````typescript
/**
 * Loopback ports for apps.
 *
 * Each running app is published on 127.0.0.1:<port> and the ingress reaches it
 * only there. A blue/green release needs a second port while the first is still
 * serving, so ports are handed out one release at a time and the caller says
 * which are already in use.
 */
import net from "node:net";
import type { PortRange } from "./hosts.js";

/**
 * First free port in `range` not in `used`, scanning from `start` and wrapping.
 * `isFree` asks the operating system, because something outside Deploy may hold
 * a port in the range. Returns null when the whole range is taken.
 */
export async function allocatePort(
  range: PortRange,
  used: ReadonlySet<number>,
  isFree: (port: number) => Promise<boolean>,
  start: number = Math.floor(Math.random() * (range.max - range.min + 1)),
): Promise<number | null> {
  const size = range.max - range.min + 1;
  for (let step = 0; step < size; step++) {
    const port = range.min + ((start + step) % size);
    if (used.has(port)) continue;
    if (await isFree(port)) return port;
  }
  return null;
}

/** True when nothing is listening on 127.0.0.1:<port>. */
export function loopbackPortFree(port: number): Promise<boolean> {
  return new Promise((resolve) => {
    const probe = net.createServer();
    probe.once("error", () => resolve(false));
    probe.listen(port, "127.0.0.1", () => probe.close(() => resolve(true)));
  });
}

/** Every port a project currently holds or has reserved for a release in progress. */
export function portsInUse(
  projects: ReadonlyArray<{ hostPort?: number | undefined; pendingPort?: number | undefined }>,
): Set<number> {
  const used = new Set<number>();
  for (const project of projects) {
    if (project.hostPort !== undefined) used.add(project.hostPort);
    if (project.pendingPort !== undefined) used.add(project.pendingPort);
  }
  return used;
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **72 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/ports.ts tests/ports.test.ts
git commit \
  -m "feat(ports): loopback port allocation for apps" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 7: Sandbox-only Docker execution and the hardened run arguments

This is the container boundary. Every `docker` and `nixpacks` call is forced to the sandbox daemon and refuses to run when none is configured, so a misconfiguration can never fall back to the host's own Docker. App containers are started only through one pure function whose output is asserted flag by flag: no capabilities, no privilege gain, hard CPU/memory/process caps, loopback-only publishing, capped logs, an env file instead of command-line variables.

**Files:**

- Create: `src/lib/run-args.ts`
- Create: `tests/run-args.test.ts`
- Create: `tests/sandbox.test.ts`
- Modify: `src/lib/docker.ts`
- Modify: `src/lib/health.ts`

**Interfaces:**

- Consumes: `config.sandboxDockerHost` (Task 1), `ContainerLimits` (Task 3), `toEnvFile` (Task 5).
- Produces: `childEnv`, `SandboxNotConfigured`, `SpawnOptions`, extended `spawnStream`/`spawnStreaming`; `RunSpec`, `buildRunArgs`, `validateVolumePath`; `dockerRunHardened`, `dockerPull`, `dockerImageDigest`, `parseRepoDigest`, `parseImageList`, `dockerListImages`, `dockerRemoveImage`, `dockerRemoveVolume`, `ListedImage`; `probeLoopback` in `health.ts`.

- [ ] **Step 1: Write the tests**

**Create `tests/run-args.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { buildRunArgs, validateVolumePath, type RunSpec } from "../src/lib/run-args.js";

const spec: RunSpec = {
  name: "nexus-app-demo-1a2b3c4d",
  image: "nexus/demo:1a2b3c4d",
  appPort: 3000,
  hostPort: 20123,
  envFile: "/tmp/nexus-env-x/env",
  limits: { memory: "512m", cpus: "0.5", pids: 256 },
};

const pairs = (args: string[]) => args.flatMap((a, i) => (a.startsWith("-") ? [[a, args[i + 1] ?? ""]] : []));
const valuesOf = (args: string[], flag: string) => pairs(args).filter(([f]) => f === flag).map(([, v]) => v);

test("the container drops every capability and cannot gain any", () => {
  const args = buildRunArgs(spec);
  assert.deepEqual(valuesOf(args, "--cap-drop"), ["ALL"]);
  assert.deepEqual(valuesOf(args, "--security-opt"), ["no-new-privileges"]);
  assert.ok(!args.includes("--privileged"));
  assert.ok(!args.some((a) => a.startsWith("--cap-add")));
});

test("memory, swap, cpu and process caps are set, and swap equals memory (none)", () => {
  const args = buildRunArgs(spec);
  assert.deepEqual(valuesOf(args, "--memory"), ["512m"]);
  assert.deepEqual(valuesOf(args, "--memory-swap"), ["512m"]);
  assert.deepEqual(valuesOf(args, "--cpus"), ["0.5"]);
  assert.deepEqual(valuesOf(args, "--pids-limit"), ["256"]);
});

test("logs use the capped local driver, not Docker's unbounded default", () => {
  const args = buildRunArgs(spec);
  assert.deepEqual(valuesOf(args, "--log-driver"), ["local"]);
  assert.deepEqual(valuesOf(args, "--log-opt"), ["max-size=10m", "max-file=2", "compress=false"]);
});

test("the app is published on loopback only", () => {
  const publishes = valuesOf(buildRunArgs(spec), "-p");
  assert.deepEqual(publishes, ["127.0.0.1:20123:3000"]);
});

test("the environment travels in a file, never on the command line", () => {
  const args = buildRunArgs(spec);
  assert.deepEqual(valuesOf(args, "--env-file"), ["/tmp/nexus-env-x/env"]);
  assert.ok(!args.includes("-e") && !args.includes("--env"));
});

test("nothing from the host is mounted and the host network is never used", () => {
  const args = buildRunArgs(spec);
  assert.ok(!args.includes("-v") && !args.includes("--volume") && !args.includes("--mount"));
  assert.ok(!args.includes("--network") && !args.includes("--net"));
  assert.ok(!args.some((a) => a.startsWith("traefik.")));
});

test("a project volume is a named volume at an absolute path, nothing else", () => {
  const args = buildRunArgs({ ...spec, volume: { name: "nexus-vol-demo", path: "/data" } });
  assert.deepEqual(valuesOf(args, "-v"), ["nexus-vol-demo:/data"]);
});

test("volume and container names that could smuggle options are refused", () => {
  for (const bad of ["", "relative", "/a:/b", "/a/../etc", "/a b", "/a,b", "/a;b", "/x".repeat(150)]) {
    assert.throws(() => validateVolumePath(bad), /volumePath/, bad);
  }
  assert.equal(validateVolumePath("/data/app-1"), "/data/app-1");
  assert.throws(() => buildRunArgs({ ...spec, name: "bad name" }), /container name/);
  assert.throws(() => buildRunArgs({ ...spec, name: "--rm" }), /container name/);
  assert.throws(() => buildRunArgs({ ...spec, volume: { name: "../x", path: "/data" } }), /volume name/);
  assert.throws(() => buildRunArgs({ ...spec, volume: { name: "v", path: "/a:/b" } }), /volumePath/);
});

test("images are marked as managed, and the image is the last argument", () => {
  const args = buildRunArgs(spec);
  assert.deepEqual(valuesOf(args, "-l"), ["nexus.managed=true"]);
  assert.equal(args.at(-1), "nexus/demo:1a2b3c4d");
});
````

**Create `tests/sandbox.test.ts`:**

````typescript
import assert from "node:assert/strict";
import http from "node:http";
import type { AddressInfo } from "node:net";
import { test } from "node:test";

// node --test runs each file in its own process, so this cannot leak into other
// tests. The config is read when the module loads, hence the dynamic imports.
process.env["SANDBOX_DOCKER_HOST"] = "";
const { childEnv, parseImageList, parseRepoDigest, SandboxNotConfigured, spawnStream } = await import(
  "../src/lib/docker.js"
);
const { probeLoopback } = await import("../src/lib/health.js");

const SANDBOX = "unix:///run/user/1001/docker.sock";

test("docker and the builder are pointed at the sandbox, overriding anything ambient", () => {
  const env = childEnv("docker", SANDBOX, {
    PATH: "/bin",
    DOCKER_HOST: "unix:///var/run/docker.sock",
    DOCKER_CONTEXT: "default",
    DOCKER_TLS_VERIFY: "1",
    DOCKER_CERT_PATH: "/certs",
  });
  assert.equal(env["DOCKER_HOST"], SANDBOX);
  assert.equal(env["PATH"], "/bin");
  assert.equal("DOCKER_CONTEXT" in env, false);
  assert.equal("DOCKER_TLS_VERIFY" in env, false);
  assert.equal("DOCKER_CERT_PATH" in env, false);
  assert.equal(childEnv("nixpacks", SANDBOX, {})["DOCKER_HOST"], SANDBOX);
});

test("with no sandbox configured, docker and the builder refuse to run at all", () => {
  assert.throws(() => childEnv("docker", "", { DOCKER_HOST: "unix:///var/run/docker.sock" }), SandboxNotConfigured);
  assert.throws(() => childEnv("nixpacks", ""), /Refusing to use the host's Docker daemon/);
});

test("other commands are untouched, so git needs no sandbox", () => {
  const base = { PATH: "/bin", FOO: "1" };
  assert.deepEqual(childEnv("git", "", base), base);
});

test("a docker command fails closed through the real spawn path when no sandbox is set", async () => {
  // The test environment sets no SANDBOX_DOCKER_HOST, which is exactly the misconfiguration.
  await assert.rejects(spawnStream("docker", ["version"], () => {}), SandboxNotConfigured);
});

test("a command that outlives its timeout is killed and reported", async () => {
  await assert.rejects(
    spawnStream("sleep", ["5"], () => {}, undefined, { timeoutMs: 100 }),
    /timed out/,
  );
});

test("spawnStream passes extra environment to the child and streams its output", async () => {
  const lines: string[] = [];
  await spawnStream("sh", ["-c", 'echo "$NEXUS_T"'], (l) => lines.push(l), undefined, { env: { NEXUS_T: "hello" } });
  assert.deepEqual(lines, ["hello"]);
});

test("a digest is read out of a repo-digest line, and only a real one", () => {
  const digest = `sha256:${"b".repeat(64)}`;
  assert.equal(parseRepoDigest(`nginx@${digest}`), digest);
  assert.equal(parseRepoDigest("nginx:latest"), null);
  assert.equal(parseRepoDigest("nginx@sha256:short"), null);
});

test("the image listing is parsed, skipping malformed lines", () => {
  const rows = parseImageList([
    "nginx\t1.27\tsha256:abc\t0123456789ab",
    "<none>\t<none>\t<none>\tdeadbeef0000",
    "garbage",
  ]);
  assert.deepEqual(rows, [
    { repository: "nginx", tag: "1.27", digest: "sha256:abc", id: "0123456789ab" },
    { repository: "<none>", tag: "<none>", digest: "<none>", id: "deadbeef0000" },
  ]);
});

test("the loopback probe returns the status an app answers with, or null", async () => {
  const server = http.createServer((req, res) => {
    res.statusCode = req.url === "/health" ? 200 : 503;
    res.end("x");
  });
  await new Promise<void>((resolve) => server.listen(0, "127.0.0.1", resolve));
  const { port } = server.address() as AddressInfo;
  try {
    assert.equal(await probeLoopback(port, "/health"), 200);
    assert.equal(await probeLoopback(port, "/other"), 503);
  } finally {
    await new Promise<void>((resolve) => server.close(() => resolve()));
  }
  assert.equal(await probeLoopback(port, "/health", 300), null);
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/run-args.test.ts tests/sandbox.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/run-args.js'` and `childEnv is not a function`.

- [ ] **Step 3: Implement**

**Modify `src/lib/docker.ts`:**

````diff
--- a/src/lib/docker.ts
+++ b/src/lib/docker.ts
@@ -1,24 +1,87 @@
 import { spawn, type ChildProcess } from "node:child_process";
+import fs from "node:fs";
+import os from "node:os";
+import path from "node:path";
+import { config } from "./config.js";
+import { toEnvFile } from "./env.js";
+import { buildRunArgs } from "./run-args.js";
+import type { ContainerLimits } from "./safeguards.js";
 
 export type LogLine = (line: string) => void;
 
+/** Thrown instead of ever running a build or a container on the host's own Docker. */
+export class SandboxNotConfigured extends Error {
+  constructor() {
+    super(
+      "The build sandbox is not configured (SANDBOX_DOCKER_HOST is empty). "
+        + "Refusing to use the host's Docker daemon.",
+    );
+    this.name = "SandboxNotConfigured";
+  }
+}
+
+const SANDBOXED_COMMANDS = new Set(["docker", "nixpacks"]);
+
+/**
+ * Environment for a child process. Docker and the builder are pointed at the
+ * sandbox daemon and nothing else: DOCKER_HOST is forced and any DOCKER_CONTEXT
+ * the operator happens to have set is dropped, so no ambient setting can send a
+ * member's build to the host daemon. With no sandbox configured they do not run.
+ */
+export function childEnv(
+  cmd: string,
+  sandboxHost: string,
+  base: NodeJS.ProcessEnv = process.env,
+): NodeJS.ProcessEnv {
+  if (!SANDBOXED_COMMANDS.has(cmd)) return base;
+  if (!sandboxHost) throw new SandboxNotConfigured();
+  const env: NodeJS.ProcessEnv = { ...base, DOCKER_HOST: sandboxHost };
+  delete env["DOCKER_CONTEXT"];
+  delete env["DOCKER_TLS_VERIFY"];
+  delete env["DOCKER_CERT_PATH"];
+  return env;
+}
+
+export type SpawnOptions = { env?: Record<string, string>; timeoutMs?: number };
+
 /** Stream a command's stdout/stderr line-by-line. Resolves on exit code 0. */
 export function spawnStream(
   cmd: string,
   args: string[],
   onLine: LogLine,
-  cwd?: string
+  cwd?: string,
+  opts: SpawnOptions = {},
 ): Promise<void> {
   return new Promise((resolve, reject) => {
-    const proc = spawn(cmd, args, { cwd, stdio: ["ignore", "pipe", "pipe"] });
+    const proc = spawn(cmd, args, {
+      cwd,
+      stdio: ["ignore", "pipe", "pipe"],
+      env: { ...childEnv(cmd, config.sandboxDockerHost), ...opts.env },
+    });
+    const timeoutMs = opts.timeoutMs;
+    let timedOut = false;
+    const timer = timeoutMs
+      ? setTimeout(() => {
+          timedOut = true;
+          proc.kill("SIGKILL");
+        }, timeoutMs)
+      : undefined;
     proc.stdout.on("data", (buf: Buffer) =>
       buf.toString().split("\n").filter(Boolean).forEach(onLine)
     );
     proc.stderr.on("data", (buf: Buffer) =>
       buf.toString().split("\n").filter(Boolean).forEach(onLine)
     );
-    proc.on("close", code => (code === 0 ? resolve() : reject(new Error(`${cmd} exited with code ${code}`))));
-    proc.on("error", reject);
+    proc.on("close", code => {
+      if (timer) clearTimeout(timer);
+      if (timedOut) reject(new Error(`${cmd} timed out after ${Math.round((timeoutMs ?? 0) / 1000)}s`));
+      else if (code === 0) resolve();
+      else reject(new Error(`${cmd} exited with code ${code}`));
+    });
+    proc.on("error", err => {
+      if (timer) clearTimeout(timer);
+      reject(err);
+    });
   });
 }
 
@@ -30,9 +93,19 @@ export function spawnStreaming(
   cmd: string,
   args: string[],
   onLine: LogLine,
-  onEnd?: (err?: Error) => void
+  onEnd?: (err?: Error) => void,
+  opts: SpawnOptions = {},
 ): () => void {
-  const proc: ChildProcess = spawn(cmd, args, { stdio: ["ignore", "pipe", "pipe"] });
+  let proc: ChildProcess;
+  try {
+    proc = spawn(cmd, args, {
+      stdio: ["ignore", "pipe", "pipe"],
+      env: { ...childEnv(cmd, config.sandboxDockerHost), ...opts.env },
+    });
+  } catch (err) {
+    onEnd?.(err as Error);
+    return () => {};
+  }
 
   proc.stdout!.on("data", (buf: Buffer) =>
     buf.toString().split("\n").filter(Boolean).forEach(onLine)
@@ -169,3 +242,106 @@ export async function dockerStats(name: string): Promise<ContainerStats | null>
 export function dockerLogs(name: string, onLine: LogLine, onEnd?: (err?: Error) => void): () => void {
   return spawnStreaming("docker", ["logs", "--tail", "100", "-f", name], onLine, onEnd);
 }
+
+// ── Hardened run (the only way an app container is started) ───────────────
+export async function dockerRunHardened(opts: {
+  name: string;
+  image: string;
+  env: Record<string, string>;
+  appPort: number;
+  hostPort: number;
+  limits: ContainerLimits;
+  volume?: { name: string; path: string } | undefined;
+  onLine: LogLine;
+}): Promise<string> {
+  await spawnStream("docker", ["rm", "-f", opts.name], () => {}).catch(() => {});
+
+  // The environment goes in a 0600 file, not on the command line where `ps` shows it.
+  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "nexus-env-"));
+  const envFile = path.join(dir, "env");
+  fs.writeFileSync(envFile, toEnvFile(opts.env), { mode: 0o600 });
+  try {
+    let containerId = "";
+    await spawnStream(
+      "docker",
+      buildRunArgs({
+        name: opts.name,
+        image: opts.image,
+        appPort: opts.appPort,
+        hostPort: opts.hostPort,
+        envFile,
+        volume: opts.volume,
+        limits: opts.limits,
+      }),
+      line => {
+        containerId = line.trim();
+        opts.onLine(`[nexus] Container started: ${containerId.slice(0, 12)}`);
+      },
+    );
+    return containerId;
+  } finally {
+    fs.rmSync(dir, { recursive: true, force: true });
+  }
+}
+
+// ── Images ─────────────────────────────────────────────────────────────────
+export function parseRepoDigest(line: string): string | null {
+  return /@(sha256:[a-f0-9]{64})/.exec(line)?.[1] ?? null;
+}
+
+/** The content digest of a pulled image, or null for one built locally. */
+export async function dockerImageDigest(image: string): Promise<string | null> {
+  const lines: string[] = [];
+  await spawnStream(
+    "docker",
+    ["image", "inspect", "-f", "{{range .RepoDigests}}{{println .}}{{end}}", image],
+    l => lines.push(l),
+  ).catch(() => {});
+  for (const line of lines) {
+    const digest = parseRepoDigest(line);
+    if (digest) return digest;
+  }
+  return null;
+}
+
+/** Pull an image and return its digest, so a release can be pinned to exact bytes. */
+export async function dockerPull(image: string, onLine: LogLine, timeoutMs: number): Promise<string> {
+  await spawnStream("docker", ["pull", image], onLine, undefined, { timeoutMs });
+  const digest = await dockerImageDigest(image);
+  if (!digest) throw new Error("pulled the image but could not read its digest");
+  return digest;
+}
+
+export type ListedImage = { repository: string; tag: string; digest: string; id: string };
+
+/** Parses `docker images --digests --format "{{.Repository}}\t{{.Tag}}\t{{.Digest}}\t{{.ID}}"`. */
+export function parseImageList(lines: readonly string[]): ListedImage[] {
+  const out: ListedImage[] = [];
+  for (const line of lines) {
+    const [repository, tag, digest, id] = line.split("\t");
+    if (repository !== undefined && tag !== undefined && digest !== undefined && id) {
+      out.push({ repository, tag, digest, id });
+    }
+  }
+  return out;
+}
+
+/** Every image in the sandbox daemon. It is dedicated to Deploy, so all of them are ours. */
+export async function dockerListImages(): Promise<ListedImage[]> {
+  const lines: string[] = [];
+  await spawnStream(
+    "docker",
+    ["images", "--digests", "--format", "{{.Repository}}\t{{.Tag}}\t{{.Digest}}\t{{.ID}}"],
+    l => lines.push(l),
+  ).catch(() => {});
+  return parseImageList(lines);
+}
+
+/** Never forced: Docker itself refuses to remove an image a container still uses. */
+export async function dockerRemoveImage(ref: string): Promise<boolean> {
+  return spawnStream("docker", ["rmi", ref], () => {}).then(() => true, () => false);
+}
+
+export async function dockerRemoveVolume(name: string): Promise<void> {
+  await spawnStream("docker", ["volume", "rm", "-f", name], () => {}).catch(() => {});
+}
````

**Modify `src/lib/health.ts`:**

````diff
--- a/src/lib/health.ts
+++ b/src/lib/health.ts
@@ -37,3 +37,17 @@ export async function waitForHealthy(opts: {
       : `app answered ${last} instead of a healthy status within ${Math.round(opts.timeoutMs / 1000)}s`,
   };
 }
+
+/** One HTTP probe of an app published on loopback. Null when nothing answers. */
+export async function probeLoopback(port: number, path: string, timeoutMs = 2_000): Promise<number | null> {
+  try {
+    const response = await fetch(`http://127.0.0.1:${port}${path}`, {
+      redirect: "manual",
+      signal: AbortSignal.timeout(timeoutMs),
+    });
+    await response.body?.cancel();
+    return response.status;
+  } catch {
+    return null;
+  }
+}
````

**Create `src/lib/run-args.ts`:**

````typescript
/**
 * The exact `docker run` arguments for a member's app.
 *
 * This is the container boundary, so it is a pure function with tests that read
 * the result: every flag that makes the container safe is asserted there. The
 * container gets no capabilities and no way to gain any, hard CPU/memory/process
 * caps, a loopback-only published port, size-capped logs, and nothing from the
 * host except its own named volume.
 */
import type { ContainerLimits } from "./safeguards.js";

export type RunSpec = {
  name: string;
  image: string;
  /** Port the app listens on inside the container. */
  appPort: number;
  /** Port on the host's loopback it is published on. */
  hostPort: number;
  /** Path of a 0600 file of NAME=value lines (kept off the command line). */
  envFile: string;
  volume?: { name: string; path: string } | undefined;
  limits: ContainerLimits;
};

const NAME_RE = /^[a-z0-9][a-z0-9_.-]{0,100}$/;
const VOLUME_NAME_RE = /^[a-z0-9][a-z0-9_.-]{0,100}$/;
const VOLUME_PATH_RE = /^\/[A-Za-z0-9._/-]{0,200}$/;

export function validateVolumePath(path: unknown): string {
  if (typeof path !== "string" || !VOLUME_PATH_RE.test(path) || path.split("/").includes("..")) {
    throw new Error("volumePath must be an absolute path such as /data");
  }
  return path;
}

export function buildRunArgs(spec: RunSpec): string[] {
  if (!NAME_RE.test(spec.name)) throw new Error(`unsafe container name: ${spec.name}`);
  if (spec.volume) {
    if (!VOLUME_NAME_RE.test(spec.volume.name)) throw new Error(`unsafe volume name: ${spec.volume.name}`);
    validateVolumePath(spec.volume.path);
  }
  return [
    "run",
    "-d",
    "--name",
    spec.name,
    "--restart",
    "unless-stopped",
    // No capabilities, and no way to acquire any (setuid binaries included).
    "--cap-drop",
    "ALL",
    "--security-opt",
    "no-new-privileges",
    "--pids-limit",
    String(spec.limits.pids),
    // Swap equal to memory means no swap at all.
    "--memory",
    spec.limits.memory,
    "--memory-swap",
    spec.limits.memory,
    "--cpus",
    spec.limits.cpus,
    // Docker's default driver keeps logs forever. These are capped, and the
    // charter promises we keep nothing longer than we need.
    "--log-driver",
    "local",
    "--log-opt",
    "max-size=10m",
    "--log-opt",
    "max-file=2",
    "--log-opt",
    "compress=false",
    // Reachable from the ingress on loopback only; never on a public interface.
    "-p",
    `127.0.0.1:${spec.hostPort}:${spec.appPort}`,
    "--env-file",
    spec.envFile,
    ...(spec.volume ? ["-v", `${spec.volume.name}:${spec.volume.path}`] : []),
    // Lets the hourly prune recognise images and containers this service made.
    "-l",
    "nexus.managed=true",
    spec.image,
  ];
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **90 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/docker.ts src/lib/health.ts src/lib/run-args.ts tests/run-args.test.ts tests/sandbox.test.ts
git commit \
  -m "feat(sandbox): docker runs only in the sandbox daemon, with hardened container arguments" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 8: Blue/green release orchestrator and the route publisher interface

A release starts the new container on its own port, proves it healthy, tells the ingress, and only then stops the old one. A project with a data volume releases stop-then-start instead, because two writers could corrupt the volume. The policy talks to interfaces so it is tested with fakes. This task also adds a TypeScript config that type-checks the tests.

**Files:**

- Create: `src/lib/publisher.ts`
- Create: `src/lib/release.ts`
- Create: `tests/release.test.ts`
- Create: `tests/support/fakes.ts`
- Create: `tsconfig.test.json`
- Modify: `package.json`

**Interfaces:**

- Consumes: `waitForHealthy` and `HealthResult` (existing), `ContainerLimits` (Task 3).
- Produces: `RoutePublisher`, `PublishedRoute`, `NullPublisher`; `ReleaseRuntime`, `ContainerSpec`, `ReleaseProject`, `ReleaseDeps`, `ReleaseOutcome`, `releaseContainerName`, `performRelease`; test support `FakeRuntime`, `RecordingPublisher`, `fakeClock`, `makeDeps`; `npm run typecheck:tests`.

- [ ] **Step 1: Write the tests**

**Create `tests/release.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { performRelease, releaseContainerName, type ReleaseProject } from "../src/lib/release.js";
import { FakeRuntime, makeDeps, RecordingPublisher } from "./support/fakes.js";

const LIMITS = { memory: "512m", cpus: "0.5", pids: 256 };

const project = (patch: Partial<ReleaseProject> = {}): ReleaseProject => ({
  id: "prj_1",
  slug: "demo",
  host: "demo.example.test",
  appPort: 3000,
  env: { FOO: "bar" },
  limits: LIMITS,
  ...patch,
});

const OLD = { containerName: "nexus-app-demo-oldoldol", imageRef: "nexus/demo:old", hostPort: 20001 };

test("the container name is derived from the slug and the deployment", () => {
  assert.equal(releaseContainerName("demo", "dep_0123456789abcdef"), "nexus-app-demo-89abcdef");
});

test("blue/green: the old release keeps serving until the route has moved, then it stops", async () => {
  const runtime = new FakeRuntime();
  runtime.seed(OLD.containerName);
  const order: string[] = [];
  const publisher = new RecordingPublisher(order);
  const deps = makeDeps(runtime, publisher, [20002]);

  const outcome = await performRelease(project({ current: OLD }), "dep_new00001", "nexus/demo:new", deps);

  assert.equal(outcome.ok, true);
  const calls = runtime.calls;
  assert.ok(calls.indexOf("run:nexus-app-demo-new00001:20002") < calls.indexOf("probe:20002"));
  const removedOld = calls.indexOf(`remove:${OLD.containerName}`);
  assert.ok(removedOld > calls.indexOf("probe:20002"), "old stops only after the new one is proven");
  assert.deepEqual(order, ["publish:20002"], "the route moved exactly once, to the new port");
  assert.equal(runtime.isAlive(OLD.containerName), false);
  assert.equal(runtime.isAlive("nexus-app-demo-new00001"), true);
  assert.deepEqual(deps.clock.slept.filter((ms) => ms === 10_000), [10_000], "the old release drained first");
  assert.deepEqual(outcome, {
    ok: true,
    containerName: "nexus-app-demo-new00001",
    hostPort: 20002,
    imageRef: "nexus/demo:new",
    mode: "blue-green",
  });
});

test("an unhealthy release is removed and the old one never stops serving", async () => {
  const runtime = new FakeRuntime();
  runtime.seed(OLD.containerName);
  runtime.probes.set(20002, [503]);
  runtime.tails.set("nexus-app-demo-new00001", ["Error: cannot find module", "exit 1"]);
  const publisher = new RecordingPublisher();
  const deps = makeDeps(runtime, publisher, [20002]);

  const outcome = await performRelease(project({ current: OLD, healthPath: "/health" }), "dep_new00001", "nexus/demo:new", deps);

  assert.equal(outcome.ok, false);
  assert.equal((outcome as { servingPrevious: boolean }).servingPrevious, true);
  assert.match((outcome as { reason: string }).reason, /503/);
  assert.equal(publisher.published.length, 0, "the route was never touched");
  assert.equal(runtime.isAlive(OLD.containerName), true, "the old release is still running");
  assert.equal(runtime.isAlive("nexus-app-demo-new00001"), false, "the failed release is gone");
  assert.ok(!runtime.calls.includes(`remove:${OLD.containerName}`));
  assert.ok(deps.logs.some((l) => l.includes("[app] Error: cannot find module")), "the app's last output is shown");
});

test("a first release that fails has nothing to fall back on", async () => {
  const runtime = new FakeRuntime();
  runtime.probes.set(20002, [null]);
  const deps = makeDeps(runtime, new RecordingPublisher(), [20002]);
  const outcome = await performRelease(project(), "dep_new00001", "nexus/demo:new", deps);
  assert.equal(outcome.ok, false);
  assert.equal((outcome as { servingPrevious: boolean }).servingPrevious, false);
});

test("a container that will not even start counts as an unhealthy release", async () => {
  const runtime = new FakeRuntime();
  runtime.seed(OLD.containerName);
  runtime.failRun.add("nexus-app-demo-new00001");
  const deps = makeDeps(runtime, new RecordingPublisher(), [20002]);
  const outcome = await performRelease(project({ current: OLD }), "dep_new00001", "nexus/demo:new", deps);
  assert.equal(outcome.ok, false);
  assert.match((outcome as { reason: string }).reason, /did not start: port is already allocated/);
  assert.equal(runtime.isAlive(OLD.containerName), true);
});

test("if the route cannot be published the new release is removed and the old keeps serving", async () => {
  const runtime = new FakeRuntime();
  runtime.seed(OLD.containerName);
  const publisher = new RecordingPublisher();
  publisher.fail = true;
  const deps = makeDeps(runtime, publisher, [20002]);
  const outcome = await performRelease(project({ current: OLD }), "dep_new00001", "nexus/demo:new", deps);
  assert.equal(outcome.ok, false);
  assert.match((outcome as { reason: string }).reason, /could not publish the route: cloud unreachable/);
  assert.equal((outcome as { servingPrevious: boolean }).servingPrevious, true);
  assert.equal(runtime.isAlive(OLD.containerName), true);
  assert.equal(runtime.isAlive("nexus-app-demo-new00001"), false);
});

test("a slow start that becomes healthy in time is accepted", async () => {
  const runtime = new FakeRuntime();
  runtime.probes.set(20002, [null, null, 200]);
  const deps = makeDeps(runtime, new RecordingPublisher(), [20002]);
  const outcome = await performRelease(project(), "dep_new00001", "nexus/demo:new", deps);
  assert.equal(outcome.ok, true);
});

test("the container gets the node's limits, the app port, and PORT in its environment", async () => {
  const runtime = new FakeRuntime();
  const deps = makeDeps(runtime, new RecordingPublisher(), [20002]);
  await performRelease(project({ appPort: 8080, env: { PORT: "9999", FOO: "bar" } }), "dep_new00001", "img", deps);
  const spec = runtime.specs[0];
  assert.deepEqual(spec?.limits, LIMITS);
  assert.equal(spec?.appPort, 8080);
  assert.equal(spec?.hostPort, 20002);
  assert.equal(spec?.env["NODE_ENV"], "production");
  assert.equal(spec?.env["FOO"], "bar");
  assert.equal(spec?.env["PORT"], "9999", "a member's own PORT wins, as before");
});

test("a declared health path is probed and must answer 2xx; otherwise any non-5xx on / will do", async () => {
  const strict = new FakeRuntime();
  strict.probes.set(20002, [404]);
  const a = await performRelease(project({ healthPath: "/health" }), "dep_new00001", "img", makeDeps(strict, new RecordingPublisher(), [20002]));
  assert.equal(a.ok, false, "404 is not healthy when a path is declared");

  const lenient = new FakeRuntime();
  lenient.probes.set(20002, [404]);
  const b = await performRelease(project(), "dep_new00001", "img", makeDeps(lenient, new RecordingPublisher(), [20002]));
  assert.equal(b.ok, true, "404 on / still proves the app is up when no path is declared");
});

test("a project with a volume stops the old release first, because two writers could corrupt it", async () => {
  const runtime = new FakeRuntime();
  runtime.seed(OLD.containerName);
  const deps = makeDeps(runtime, new RecordingPublisher(), [20002]);
  const volume = { name: "nexus-vol-demo", path: "/data" };
  const outcome = await performRelease(project({ current: OLD, volume }), "dep_new00001", "nexus/demo:new", deps);

  assert.equal(outcome.ok, true);
  assert.equal((outcome as { mode: string }).mode, "stop-start");
  const calls = runtime.calls;
  assert.ok(calls.indexOf(`remove:${OLD.containerName}`) < calls.indexOf("run:nexus-app-demo-new00001:20002"));
  assert.deepEqual(runtime.specs[0]?.volume, volume);
  assert.deepEqual(deps.clock.slept.filter((ms) => ms === 10_000), [], "no drain: the old one is already gone");
});

test("a failed volume release puts the previous image back on its own port", async () => {
  const runtime = new FakeRuntime();
  runtime.seed(OLD.containerName);
  runtime.probes.set(20002, [503]);
  const publisher = new RecordingPublisher();
  const deps = makeDeps(runtime, publisher, [20002]);
  const volume = { name: "nexus-vol-demo", path: "/data" };
  const outcome = await performRelease(project({ current: OLD, volume }), "dep_new00001", "nexus/demo:new", deps);

  assert.equal(outcome.ok, false);
  assert.equal((outcome as { servingPrevious: boolean }).servingPrevious, true);
  assert.ok(runtime.calls.includes(`run:${OLD.containerName}:20001`), "the previous image restarted on its old port");
  assert.equal(runtime.isAlive(OLD.containerName), true);
  assert.equal(publisher.published.length, 0, "the route never moved, so it still points at the old port");
});

test("if the restored volume release is unhealthy too, nothing is left running and we say so", async () => {
  const runtime = new FakeRuntime();
  runtime.seed(OLD.containerName);
  runtime.probes.set(20002, [503]);
  runtime.probes.set(20001, [503]);
  const deps = makeDeps(runtime, new RecordingPublisher(), [20002]);
  const outcome = await performRelease(
    project({ current: OLD, volume: { name: "nexus-vol-demo", path: "/data" } }),
    "dep_new00001",
    "nexus/demo:new",
    deps,
  );
  assert.equal(outcome.ok, false);
  assert.equal((outcome as { servingPrevious: boolean }).servingPrevious, false);
  assert.equal(runtime.isAlive(OLD.containerName), false);
});
````

**Create `tests/support/fakes.ts`:**

````typescript
import type { PublishedRoute, RoutePublisher } from "../../src/lib/publisher.js";
import type { ContainerSpec, ReleaseDeps, ReleaseRuntime } from "../../src/lib/release.js";

/** Records every call in order, so a test can assert what happened *before* what. */
export class FakeRuntime implements ReleaseRuntime {
  readonly calls: string[] = [];
  readonly specs: ContainerSpec[] = [];
  private readonly alive = new Set<string>();
  /** Status each probe on a port returns, in order; the last one repeats. */
  probes = new Map<number, Array<number | null>>();
  failRun = new Set<string>();
  tails = new Map<string, string[]>();

  async run(spec: ContainerSpec): Promise<string> {
    this.calls.push(`run:${spec.name}:${spec.hostPort}`);
    this.specs.push(spec);
    if (this.failRun.has(spec.name)) throw new Error("port is already allocated");
    this.alive.add(spec.name);
    return `id-${spec.name}`;
  }

  async probe(hostPort: number): Promise<number | null> {
    this.calls.push(`probe:${hostPort}`);
    const queue = this.probes.get(hostPort) ?? [200];
    return queue.length > 1 ? (queue.shift() as number | null) : (queue[0] ?? null);
  }

  async status(name: string): Promise<string> {
    return this.alive.has(name) ? "running" : "missing";
  }

  async logTail(name: string): Promise<string[]> {
    return this.tails.get(name) ?? [];
  }

  async remove(name: string): Promise<void> {
    this.calls.push(`remove:${name}`);
    this.alive.delete(name);
  }

  isAlive(name: string): boolean {
    return this.alive.has(name);
  }

  /** Pretend this container was already running before the release began. */
  seed(name: string): void {
    this.alive.add(name);
  }
}

export class RecordingPublisher implements RoutePublisher {
  readonly published: PublishedRoute[] = [];
  readonly unpublished: string[] = [];
  readonly log: string[];
  fail = false;

  constructor(log: string[] = []) {
    this.log = log;
  }

  async publish(route: PublishedRoute): Promise<void> {
    if (this.fail) throw new Error("cloud unreachable");
    this.log.push(`publish:${route.hostPort}`);
    this.published.push(route);
  }

  async unpublish(slug: string): Promise<void> {
    this.log.push(`unpublish:${slug}`);
    this.unpublished.push(slug);
  }
}

/** A clock that only moves when something sleeps, so timeouts take no real time. */
export function fakeClock() {
  let t = 0;
  const slept: number[] = [];
  return {
    now: () => t,
    sleep: async (ms: number) => {
      slept.push(ms);
      t += ms;
    },
    slept,
  };
}

export function makeDeps(runtime: FakeRuntime, publisher: RecordingPublisher, ports: number[]): ReleaseDeps & {
  clock: ReturnType<typeof fakeClock>;
  logs: string[];
} {
  const clock = fakeClock();
  const logs: string[] = [];
  return {
    runtime,
    publisher,
    allocatePort: async () => ports.shift() ?? 29999,
    health: { timeoutMs: 5_000, intervalMs: 500, now: clock.now, sleep: clock.sleep },
    drainMs: 10_000,
    sleep: clock.sleep,
    log: (line) => logs.push(line),
    clock,
    logs,
  };
}
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/release.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/release.js'`.

- [ ] **Step 3: Implement**

**Modify `package.json`:**

````diff
--- a/package.json
+++ b/package.json
@@ -13,6 +13,7 @@
     "start": "node dist/index.js",
     "test": "node --import tsx --test tests/*.test.ts",
     "typecheck": "tsc -p tsconfig.json --noEmit",
+    "typecheck:tests": "tsc -p tsconfig.test.json",
     "lint": "biome check src web/src",
     "format": "biome check --write src web/src",
     "check": "npm run typecheck && npm run lint"
````

**Create `src/lib/publisher.ts`:**

````typescript
/**
 * How a live app becomes reachable. Deploy decides *that* an app is live on a
 * loopback port; a publisher tells the ingress. Keeping this behind an
 * interface is what lets any node's ingress serve Deploy's apps: on the
 * reference node the publisher registers the route with Nexus Cloud, whose
 * registry the ecosystem proxy reads.
 */
export type PublishedRoute = {
  projectId: string;
  slug: string;
  /** The public hostname, e.g. my-app.<base domain>. */
  host: string;
  /** Loopback port the container is published on. */
  hostPort: number;
};

export interface RoutePublisher {
  /** Point `route.host` at `route.hostPort`, replacing any earlier port for the same slug. */
  publish(route: PublishedRoute): Promise<void>;
  /** Stop serving `slug`. Idempotent. */
  unpublish(slug: string): Promise<void>;
}

/** For a node with no ingress to tell, and for tests that do not care. */
export class NullPublisher implements RoutePublisher {
  async publish(): Promise<void> {}
  async unpublish(): Promise<void> {}
}
````

**Create `src/lib/release.ts`:**

````typescript
/**
 * Turning an image into the version people are served.
 *
 * Blue/green: the new container starts on its own loopback port while the old
 * one keeps serving; only when the new one answers its health check is the
 * route switched, and only then does the old one stop. A release that never
 * becomes healthy is removed and nobody noticed.
 *
 * A project with a persistent volume cannot do that safely, because two
 * containers writing one volume (a SQLite file, say) can corrupt it. Those
 * release stop-then-start: a short outage, with the previous image put back if
 * the new one fails.
 *
 * Everything here talks to interfaces, so the whole policy is tested with
 * fakes. build.ts wires in the real Docker and the real publisher.
 */
import { type HealthResult, waitForHealthy } from "./health.js";
import type { RoutePublisher } from "./publisher.js";
import type { ContainerLimits } from "./safeguards.js";

export type ContainerSpec = {
  name: string;
  image: string;
  env: Record<string, string>;
  appPort: number;
  hostPort: number;
  limits: ContainerLimits;
  volume?: { name: string; path: string } | undefined;
};

export type ReleaseRuntime = {
  run(spec: ContainerSpec, onLine: (line: string) => void): Promise<string>;
  /** HTTP status of one probe on the loopback port, or null if nothing answered. */
  probe(hostPort: number, path: string): Promise<number | null>;
  status(name: string): Promise<string>;
  logTail(name: string): Promise<string[]>;
  remove(name: string): Promise<void>;
};

export type ReleaseProject = {
  id: string;
  slug: string;
  host: string;
  appPort: number;
  healthPath?: string | undefined;
  env: Record<string, string>;
  limits: ContainerLimits;
  /** Present when the project persists data: releases then stop-then-start. */
  volume?: { name: string; path: string } | undefined;
  /** What is serving right now, if anything. */
  current?: { containerName: string; imageRef: string; hostPort: number } | undefined;
};

export type ReleaseDeps = {
  runtime: ReleaseRuntime;
  publisher: RoutePublisher;
  allocatePort: () => Promise<number>;
  health: {
    timeoutMs: number;
    intervalMs: number;
    now?: () => number;
    sleep?: (ms: number) => Promise<void>;
  };
  /** How long the old container keeps running after the route has moved. */
  drainMs: number;
  sleep?: (ms: number) => Promise<void>;
  log: (line: string) => void;
};

export type ReleaseOutcome =
  | { ok: true; containerName: string; hostPort: number; imageRef: string; mode: "blue-green" | "stop-start" }
  | { ok: false; reason: string; servingPrevious: boolean };

export function releaseContainerName(slug: string, deploymentId: string): string {
  return `nexus-app-${slug}-${deploymentId.slice(-8)}`;
}

const wait = (ms: number) => new Promise<void>((resolve) => setTimeout(resolve, ms));

async function startAndProve(
  project: ReleaseProject,
  name: string,
  imageRef: string,
  hostPort: number,
  deps: ReleaseDeps,
): Promise<HealthResult> {
  const { runtime, health, log } = deps;
  try {
    await runtime.run(
      {
        name,
        image: imageRef,
        env: { PORT: String(project.appPort), NODE_ENV: "production", ...project.env },
        appPort: project.appPort,
        hostPort,
        limits: project.limits,
        volume: project.volume,
      },
      log,
    );
  } catch (error) {
    return { ok: false, reason: `container did not start: ${(error as Error).message}` };
  }
  const healthPath = project.healthPath || "/";
  log(`[nexus] Waiting for ${healthPath} to answer on 127.0.0.1:${hostPort}…`);
  return waitForHealthy({
    probe: () => runtime.probe(hostPort, healthPath),
    isRunning: async () => ["running", "created"].includes(await runtime.status(name)),
    // A declared health path must answer 2xx; without one any non-5xx on "/" will do.
    requireOk: Boolean(project.healthPath),
    timeoutMs: health.timeoutMs,
    intervalMs: health.intervalMs,
    ...(health.now ? { now: health.now } : {}),
    ...(health.sleep ? { sleep: health.sleep } : {}),
  });
}

async function logFailure(name: string, deps: ReleaseDeps): Promise<void> {
  for (const line of await deps.runtime.logTail(name).catch(() => [] as string[])) {
    deps.log(`[app] ${line}`);
  }
}

export async function performRelease(
  project: ReleaseProject,
  deploymentId: string,
  imageRef: string,
  deps: ReleaseDeps,
): Promise<ReleaseOutcome> {
  const { runtime, publisher, log } = deps;
  const sleep = deps.sleep ?? wait;
  const name = releaseContainerName(project.slug, deploymentId);
  const previous = project.current;

  // A volume means one writer at a time: take the old container down first.
  if (project.volume && previous) {
    log(`[nexus] ${project.slug} keeps data on a volume, so the old release stops before the new one starts`);
    await runtime.remove(previous.containerName);
  }

  const hostPort = await deps.allocatePort();
  log(`[nexus] Starting ${imageRef} on 127.0.0.1:${hostPort}`);
  const health = await startAndProve(project, name, imageRef, hostPort, deps);

  if (!health.ok) {
    log(`[nexus] ✗ Health check failed: ${health.reason}`);
    await logFailure(name, deps);
    await runtime.remove(name);

    if (project.volume && previous) {
      log(`[nexus] Restoring previous release → ${previous.imageRef}`);
      const restored = await startAndProve(project, previous.containerName, previous.imageRef, previous.hostPort, deps);
      if (restored.ok) {
        log("[nexus] ✓ Previous release is serving again");
        return { ok: false, reason: health.reason, servingPrevious: true };
      }
      log(`[nexus] ✗ Previous release is unhealthy too: ${restored.reason}`);
      await runtime.remove(previous.containerName);
      return { ok: false, reason: health.reason, servingPrevious: false };
    }
    return { ok: false, reason: health.reason, servingPrevious: Boolean(previous) };
  }

  try {
    await publisher.publish({ projectId: project.id, slug: project.slug, host: project.host, hostPort });
  } catch (error) {
    const reason = `could not publish the route: ${(error as Error).message}`;
    log(`[nexus] ✗ ${reason}`);
    await runtime.remove(name);
    return { ok: false, reason, servingPrevious: Boolean(previous) && !project.volume };
  }

  if (previous && !project.volume && previous.containerName !== name) {
    log(`[nexus] Route moved; stopping the old release in ${Math.round(deps.drainMs / 1000)}s`);
    await sleep(deps.drainMs);
    await runtime.remove(previous.containerName);
  }

  return {
    ok: true,
    containerName: name,
    hostPort,
    imageRef,
    mode: project.volume ? "stop-start" : "blue-green",
  };
}
````

**Create `tsconfig.test.json`:**

````json
{
  "extends": "./tsconfig.json",
  "compilerOptions": {
    "rootDir": ".",
    "noEmit": true
  },
  "include": ["src/**/*.ts", "tests/**/*.ts"],
  "exclude": ["dist", "web", "node_modules"]
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **102 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add package.json src/lib/publisher.ts src/lib/release.ts tests/release.test.ts tests/support/fakes.ts tsconfig.test.json
git commit \
  -m "feat(release): blue/green releases with a route publisher interface" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 9: The deployment pipeline on sources, safeguards, blue/green release and a publisher

Rewrites `build.ts` so a deployment flows through source handling (clone and build, or pull and pin the digest), the build timeout, secret redaction, the release orchestrator and the publisher. The real Git and Docker steps are injectable, so the whole flow is tested without either. Containers are now named per release.

**Files:**

- Create: `src/lib/build-steps.ts`
- Create: `src/lib/containers.ts`
- Create: `src/lib/docker-runtime.ts`
- Create: `src/lib/guard.ts`
- Create: `tests/deploy-flow.test.ts`
- Create: `tests/guard.test.ts`
- Modify: `src/lib/build.ts`
- Modify: `src/lib/docker.ts`
- Modify: `src/types.ts`

**Interfaces:**

- Consumes: Tasks 1-8.
- Produces: `DeploySteps`, `DeployDeps`, `defaultDeps`, `setRoutePublisher`, `getRoutePublisher`, `guardBuild`, `runDeploy(project, id, deps?)`, `runRollback(project, id, imageRef, deps?)`; `evaluateBuildGuard`; `containerNameOf`; `cloneRepo`, `buildImage`, `pullImage`, `Track`, `BuildOptions`; `dockerReleaseRuntime`; `Project.containerName`, `hostPort`, `pendingPort`, `previousImageTag`. Removes `dockerRun` (Traefik labels) and `dockerContainerHost`.

- [ ] **Step 1: Write the tests**

**Create `tests/deploy-flow.test.ts`:**

````typescript
import assert from "node:assert/strict";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { test } from "node:test";
import { DEFAULT_LIMITS } from "../src/lib/safeguards.js";
import type { Project } from "../src/types.js";
import { FakeRuntime, RecordingPublisher } from "./support/fakes.js";

// The config is read when the modules load, and node --test gives every file its
// own process, so a throwaway data directory here cannot touch anything else.
const dataDir = fs.mkdtempSync(path.join(os.tmpdir(), "deploy-flow-"));
process.env["DATA_DIR"] = dataDir;
process.env["APP_SECRET"] = "test-secret-test-secret-1234";
process.env["BASE_DOMAIN"] = "example.test";
process.env["SANDBOX_DOCKER_HOST"] = "unix:///does-not-exist.sock";
const store = await import("../src/lib/store.js");
const build = await import("../src/lib/build.js");
type Deps = import("../src/lib/build.js").DeployDeps;

const DIGEST = `sha256:${"c".repeat(64)}`;

function project(patch: Partial<Project> = {}): Project {
  return {
    id: "prj_1",
    ownerId: "ada",
    name: "demo",
    repo: "o/r",
    branch: "main",
    buildContext: ".",
    buildCommand: "",
    startCommand: "",
    volumePath: "/workspace",
    port: 3000,
    env: {},
    status: "idle",
    autoDeployEnabled: true,
    createdAt: 1,
    updatedAt: 1,
    ...patch,
  };
}

function seed(...projects: Project[]) {
  store.saveDb({ users: [], projects, deployments: [], sessions: [], members: [] });
}

const projectOf = (id = "prj_1") => store.loadDb().projects.find((p) => p.id === id) as Project;
const deploymentOf = (id: string) => store.loadDb().deployments.find((d) => d.id === id);

function makeDeps(patch: Partial<Deps> = {}) {
  const calls: string[] = [];
  const runtime = new FakeRuntime();
  const publisher = new RecordingPublisher();
  const deps: Deps = {
    steps: {
      clone: async (_source, dir) => {
        calls.push("clone");
        fs.writeFileSync(path.join(dir, "Dockerfile"), "FROM scratch\n");
      },
      buildImage: async (opts) => {
        calls.push(`build:${opts.tag}`);
      },
      pullImage: async (image) => {
        calls.push(`pull:${image}`);
        return DIGEST;
      },
    },
    runtime,
    publisher,
    limits: DEFAULT_LIMITS,
    baseDomain: "example.test",
    hostPattern: "{slug}.{BASE_DOMAIN}",
    portRange: { min: 20000, max: 20009 },
    isPortFree: async () => true,
    health: { timeoutMs: 200, intervalMs: 10 },
    drainMs: 0,
    tmpDir: dataDir,
    ...patch,
  };
  return { deps, calls, runtime, publisher };
}

async function until(check: () => boolean, label: string) {
  for (let i = 0; i < 200; i++) {
    if (check()) return;
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  assert.fail(`timed out waiting for ${label}`);
}

test("a Git deployment clones, builds, starts, publishes the route and goes live", async () => {
  const p = project();
  seed(p);
  const { deps, calls, runtime, publisher } = makeDeps();
  const dep = build.createDeployment(p, "abc123", "manual");

  await build.runDeploy(p, dep.id, deps);

  const finished = deploymentOf(dep.id);
  assert.equal(finished?.status, "live");
  assert.equal(finished?.imageTag, `nexus/demo:${dep.id.slice(-8)}`);
  const live = projectOf();
  assert.equal(live.status, "live");
  assert.equal(live.containerName, `nexus-app-demo-${dep.id.slice(-8)}`);
  assert.equal(live.domain, "demo.example.test");
  assert.ok(live.hostPort !== undefined && live.hostPort >= 20000 && live.hostPort <= 20009);
  assert.equal(live.pendingPort, undefined, "the reservation is released once the release is done");
  assert.deepEqual(calls, ["clone", `build:nexus/demo:${dep.id.slice(-8)}`]);
  assert.equal(publisher.published.length, 1);
  assert.equal(publisher.published[0]?.host, "demo.example.test");
  assert.equal(publisher.published[0]?.hostPort, live.hostPort);
  assert.equal(runtime.isAlive(live.containerName as string), true);
  assert.ok(finished?.logs.some((l) => l.includes("Live at https://demo.example.test")));
});

test("an image deployment pulls, pins the exact digest, and neither clones nor builds", async () => {
  const p = project({ repo: "nginx:1.27", branch: "", source: { type: "image", image: "nginx:1.27" } });
  seed(p);
  const { deps, calls } = makeDeps();
  const dep = build.createDeployment(p, "manual", "manual");

  await build.runDeploy(p, dep.id, deps);

  assert.deepEqual(calls, ["pull:nginx:1.27"]);
  assert.equal(deploymentOf(dep.id)?.status, "live");
  assert.equal(projectOf().imageTag, `nginx@${DIGEST}`, "rollback will restore these exact bytes");
});

test("a failed clone fails the deployment, and a token in the error never reaches the log", async () => {
  const token = "ghp_secret999abc";
  const p = project({ source: { type: "git", repo: "o/r", branch: "main", token } });
  seed(p);
  const { deps } = makeDeps({
    steps: {
      clone: async () => {
        throw new Error(`fatal: could not read from https://x-access-token:${token}@example.test/o/r`);
      },
      buildImage: async () => {},
      pullImage: async () => DIGEST,
    },
  });
  const dep = build.createDeployment(p, "abc", "manual");

  await build.runDeploy(p, dep.id, deps);

  const finished = deploymentOf(dep.id);
  assert.equal(finished?.status, "failed");
  assert.equal(projectOf().status, "failed");
  const text = (finished?.logs ?? []).join("\n");
  assert.ok(text.includes("Build failed"));
  assert.ok(!text.includes(token), "the token is redacted");
  assert.ok(text.includes("***"));
});

test("a build that fails leaves the release that was already serving untouched", async () => {
  const p = project({
    status: "live",
    containerName: "nexus-app-demo-oldoldol",
    hostPort: 20005,
    imageTag: "nexus/demo:old",
  });
  seed(p);
  const { deps, runtime } = makeDeps({
    steps: {
      clone: async () => {
        throw new Error("network down");
      },
      buildImage: async () => {},
      pullImage: async () => DIGEST,
    },
  });
  runtime.seed("nexus-app-demo-oldoldol");
  const dep = build.createDeployment(p, "abc", "manual");

  await build.runDeploy(p, dep.id, deps);

  assert.equal(deploymentOf(dep.id)?.status, "failed");
  assert.equal(projectOf().status, "live", "the project is still serving its previous release");
  assert.equal(runtime.isAlive("nexus-app-demo-oldoldol"), true);
});

test("an unhealthy release is rejected and the previous one keeps serving", async () => {
  const p = project({
    status: "live",
    containerName: "nexus-app-demo-oldoldol",
    hostPort: 20005,
    imageTag: "nexus/demo:old",
  });
  seed(p);
  const { deps, runtime, publisher } = makeDeps();
  runtime.seed("nexus-app-demo-oldoldol");
  for (let port = 20000; port <= 20009; port++) runtime.probes.set(port, [503]);
  const dep = build.createDeployment(p, "abc", "manual");

  await build.runDeploy(p, dep.id, deps);

  assert.equal(deploymentOf(dep.id)?.status, "failed");
  assert.equal(projectOf().status, "live");
  assert.equal(projectOf().containerName, "nexus-app-demo-oldoldol");
  assert.equal(projectOf().pendingPort, undefined);
  assert.equal(publisher.published.length, 0);
  assert.equal(runtime.isAlive("nexus-app-demo-oldoldol"), true);
  assert.ok(deploymentOf(dep.id)?.logs.some((l) => l.includes("Release rejected")));
});

test("a successful release records the image it replaced, so rollback survives pruning", async () => {
  const p = project({
    status: "live",
    containerName: "nexus-app-demo-oldoldol",
    hostPort: 20005,
    imageTag: "nexus/demo:old",
  });
  seed(p);
  const { deps, runtime } = makeDeps();
  runtime.seed("nexus-app-demo-oldoldol");
  const dep = build.createDeployment(p, "abc", "manual");
  await build.runDeploy(p, dep.id, deps);
  assert.equal(projectOf().previousImageTag, "nexus/demo:old");
  assert.equal(projectOf().imageTag, `nexus/demo:${dep.id.slice(-8)}`);
  assert.equal(runtime.isAlive("nexus-app-demo-oldoldol"), false, "the old release stopped after the route moved");
});

test("cancelling a running build stops it and marks it cancelled", async () => {
  const p = project();
  seed(p);
  const { deps, calls } = makeDeps({
    steps: {
      clone: (_source, _dir, _log, track) =>
        new Promise((_resolve, reject) => {
          calls.push("clone");
          track(() => reject(new Error("killed")));
        }),
      buildImage: async () => {},
      pullImage: async () => DIGEST,
    },
  });
  const dep = build.createDeployment(p, "abc", "manual");
  const running = build.runDeploy(p, dep.id, deps);
  await until(() => calls.includes("clone"), "the clone to start");

  assert.equal(await build.cancelBuild(dep.id), true);
  await running;

  assert.equal(deploymentOf(dep.id)?.status, "cancelled");
  assert.equal(build.isBuilding(p.id), false);
});

test("a build that outlives its time limit is stopped and reported", async () => {
  const p = project();
  seed(p);
  const { deps } = makeDeps({
    limits: { ...DEFAULT_LIMITS, buildTimeoutMs: 40 },
    steps: {
      clone: (_source, _dir, _log, track) => new Promise((_resolve, reject) => track(() => reject(new Error("killed")))),
      buildImage: async () => {},
      pullImage: async () => DIGEST,
    },
  });
  const dep = build.createDeployment(p, "abc", "manual");
  await build.runDeploy(p, dep.id, deps);

  assert.equal(deploymentOf(dep.id)?.status, "failed");
  assert.ok(deploymentOf(dep.id)?.logs.some((l) => l.includes("Build exceeded")));
});

test("a second build of the same project is refused while one is running", async () => {
  const p = project();
  seed(p);
  const calls: string[] = [];
  const { deps } = makeDeps({
    steps: {
      clone: (_source, _dir, _log, track) =>
        new Promise((_resolve, reject) => {
          calls.push("clone");
          track(() => reject(new Error("killed")));
        }),
      buildImage: async () => {},
      pullImage: async () => DIGEST,
    },
  });
  const first = build.createDeployment(p, "one", "manual");
  const running = build.runDeploy(p, first.id, deps);
  await until(() => calls.includes("clone"), "the first clone to start");

  const second = build.createDeployment(p, "two", "manual");
  await build.runDeploy(p, second.id, deps);
  assert.equal(deploymentOf(second.id)?.status, "failed");
  assert.ok(deploymentOf(second.id)?.logs.some((l) => l.includes("Another build is already running")));

  await build.cancelBuild(first.id);
  await running;
});

test("a rollback starts the recorded image without cloning or building", async () => {
  const p = project({ status: "live", containerName: "nexus-app-demo-oldoldol", hostPort: 20005, imageTag: "nexus/demo:new" });
  seed(p);
  const { deps, calls, runtime } = makeDeps();
  runtime.seed("nexus-app-demo-oldoldol");
  const dep = build.createDeployment(p, "rollback:abc", "rollback");

  await build.runRollback(p, dep.id, "nexus/demo:old", deps);

  assert.deepEqual(calls, []);
  assert.equal(deploymentOf(dep.id)?.status, "live");
  assert.equal(projectOf().imageTag, "nexus/demo:old");
  assert.equal(projectOf().previousImageTag, "nexus/demo:new");
});

test("two projects releasing at once never pick the same port", async () => {
  const a = project({ id: "prj_a", name: "alpha" });
  const b = project({ id: "prj_b", name: "bravo" });
  seed(a, b);
  const first = makeDeps();
  const second = makeDeps();
  const depA = build.createDeployment(a, "x", "manual");
  const depB = build.createDeployment(b, "y", "manual");

  await Promise.all([build.runDeploy(a, depA.id, first.deps), build.runDeploy(b, depB.id, second.deps)]);

  const portA = projectOf("prj_a").hostPort;
  const portB = projectOf("prj_b").hostPort;
  assert.ok(portA !== undefined && portB !== undefined);
  assert.notEqual(portA, portB);
});

test("the queued deployment's first log lines never show a token", () => {
  const p = project({ source: { type: "git", repo: "o/r", branch: "main", token: "ghp_visible123" } });
  seed(p);
  const dep = build.createDeployment(p, "abc", "manual");
  assert.ok(!dep.logs.join("\n").includes("ghp_visible123"));
  assert.ok(dep.logs.join("\n").includes("repo=o/r"));
});
````

**Create `tests/guard.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { containerNameOf } from "../src/lib/containers.js";
import { evaluateBuildGuard, type GuardContext } from "../src/lib/guard.js";
import { DEFAULT_LIMITS } from "../src/lib/safeguards.js";

const now = 50_000_000;
const calm: GuardContext = { isBuilding: () => false, hostFreePercent: 60, sandboxUsedPercent: 10, now };
const data = {
  projects: [
    { id: "a1", ownerId: "ada" },
    { id: "a2", ownerId: "ada" },
    { id: "b1", ownerId: "bob" },
    { id: "old" },
  ],
  deployments: [] as Array<{ projectId: string; triggeredBy: string; createdAt: number }>,
};
const code = (v: ReturnType<typeof evaluateBuildGuard>) => (v.ok ? "ok" : v.code);

test("a calm node lets a member build", () => {
  assert.equal(code(evaluateBuildGuard(data, { ownerId: "ada" }, calm, DEFAULT_LIMITS)), "ok");
});

test("a build running in another of the member's projects counts against them", () => {
  const ctx = { ...calm, isBuilding: (id: string) => id === "a2" };
  assert.equal(code(evaluateBuildGuard(data, { ownerId: "ada" }, ctx, DEFAULT_LIMITS)), "busy");
});

test("someone else's running build does not", () => {
  const ctx = { ...calm, isBuilding: (id: string) => id === "b1" };
  assert.equal(code(evaluateBuildGuard(data, { ownerId: "ada" }, ctx, DEFAULT_LIMITS)), "ok");
});

test("builds in the last hour across all of a member's projects are rate limited", () => {
  const recent = Array.from({ length: 6 }, (_, i) => ({
    projectId: i % 2 ? "a1" : "a2",
    triggeredBy: "manual",
    createdAt: now - 1_000 * (i + 1),
  }));
  const verdict = evaluateBuildGuard({ ...data, deployments: recent }, { ownerId: "ada" }, calm, DEFAULT_LIMITS);
  assert.equal(code(verdict), "rate");
  assert.equal(code(evaluateBuildGuard({ ...data, deployments: recent }, { ownerId: "bob" }, calm, DEFAULT_LIMITS)), "ok");
});

test("a low disk pauses everyone's builds", () => {
  const ctx = { ...calm, hostFreePercent: 9 };
  assert.equal(code(evaluateBuildGuard(data, { ownerId: "bob" }, ctx, DEFAULT_LIMITS)), "disk");
});

test("a project with no owner is not grouped with other ownerless projects", () => {
  const ctx = { ...calm, isBuilding: (id: string) => id === "old" };
  assert.equal(code(evaluateBuildGuard(data, {}, ctx, DEFAULT_LIMITS)), "ok");
});

test("the serving container is named per release, with the old fixed name as a fallback", () => {
  assert.equal(containerNameOf({ name: "demo", containerName: "nexus-app-demo-abcd1234" }), "nexus-app-demo-abcd1234");
  assert.equal(containerNameOf({ name: "demo" }), "nexus-app-demo");
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/deploy-flow.test.ts tests/guard.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/guard.js'` and, for `deploy-flow.test.ts`, missing exports such as `defaultDeps`.

- [ ] **Step 3: Implement**

**Create `src/lib/build-steps.ts`:**

````typescript
/**
 * The three things a deployment actually does to the outside world: clone a
 * repository, build an image, pull an image. They are separate from the
 * pipeline in build.ts so the pipeline can be tested with fakes, and because
 * each is only a thin, easily-read wrapper around one command.
 *
 * Each step settles when its process ends. Cancelling kills the process, which
 * makes the step reject; build.ts then reads its own flags to say *why*.
 */
import { dockerPull, spawnStreaming, type LogLine } from "./docker.js";
import { resolveRepoUrl } from "./repo.js";
import { gitCredentialEnv } from "./source.js";
import type { GitSource } from "../types.js";

/** Registers a way to stop the running step. */
export type Track = (kill: () => void) => void;

export type BuildOptions = {
  contextDir: string;
  /** Resolved Dockerfile path, or null to let Nixpacks work out the stack. */
  dockerfile: string | null;
  tag: string;
};

function run(cmd: string, args: string[], log: LogLine, track: Track, env?: Record<string, string>): Promise<void> {
  return new Promise((resolve, reject) => {
    const kill = spawnStreaming(cmd, args, log, (err) => (err ? reject(err) : resolve()), env ? { env } : {});
    track(kill);
  });
}

export async function cloneRepo(
  source: GitSource,
  dir: string,
  allowLocal: boolean,
  log: LogLine,
  track: Track,
): Promise<void> {
  const url = resolveRepoUrl(source.repo, allowLocal);
  const credentials = gitCredentialEnv(source.token);
  try {
    // `--` ends option parsing, so a repository URL can never be read as a flag.
    await run("git", ["clone", "--depth", "1", "--branch", source.branch, "--", url, dir], log, track, credentials.env);
  } finally {
    credentials.cleanup();
  }
}

export function buildImage(opts: BuildOptions, log: LogLine, track: Track): Promise<void> {
  return opts.dockerfile
    ? run("docker", ["build", "-t", opts.tag, "-f", opts.dockerfile, opts.contextDir], log, track)
    : run("nixpacks", ["build", opts.contextDir, "--name", opts.tag], log, track);
}

export function pullImage(image: string, log: LogLine, timeoutMs: number): Promise<string> {
  return dockerPull(image, log, timeoutMs);
}
````

**Modify `src/lib/build.ts`:**

````diff
--- a/src/lib/build.ts
+++ b/src/lib/build.ts
@@ -1,21 +1,77 @@
 import fs from "node:fs";
-import path from "node:path";
 import os from "node:os";
-import { resolveBuildContext, resolveDockerfile, validateDockerfile } from "./build-context.js";
-export { resolveBuildContext } from "./build-context.js";
-import { spawnStream, spawnStreaming, dockerRun, dockerRemove, dockerStatus, dockerContainerHost, dockerLogTail } from "./docker.js";
-import { waitForHealthy, type HealthResult } from "./health.js";
-import { resolveRepoUrl } from "./repo.js";
-import { loadDb, writeDb, trimDeployments, capLogs, saveDb } from "./store.js";
+import path from "node:path";
 import { newId } from "./auth.js";
+import { resolveBuildContext, resolveDockerfile, validateDockerfile } from "./build-context.js";
+import { buildImage, cloneRepo, pullImage, type BuildOptions, type Track } from "./build-steps.js";
 import { config } from "./config.js";
+import { dockerReleaseRuntime } from "./docker-runtime.js";
 import { emitStatusChange } from "./events.js";
-import type { Project, Deployment, DeployTrigger } from "../types.js";
+import { evaluateBuildGuard } from "./guard.js";
+import { appHost, type PortRange } from "./hosts.js";
+import { allocatePort, loopbackPortFree, portsInUse } from "./ports.js";
+import { NullPublisher, type RoutePublisher } from "./publisher.js";
+import { performRelease, type ReleaseProject, type ReleaseRuntime } from "./release.js";
+import { containerLimits, diskFreePercent, type Limits, type Verdict } from "./safeguards.js";
+import { pinnedImageRef, redact, sourceOf } from "./source.js";
+import { capLogs, loadDb, saveDb, trimDeployments, writeDb } from "./store.js";
+import type { Deployment, DeployTrigger, GitSource, Project } from "../types.js";
+
+export { resolveBuildContext } from "./build-context.js";
+
+type LogFn = (line: string) => void;
+
+/** Everything the pipeline does to the outside world, replaceable in tests. */
+export type DeploySteps = {
+  clone: (source: GitSource, dir: string, log: LogFn, track: Track) => Promise<void>;
+  buildImage: (opts: BuildOptions, log: LogFn, track: Track) => Promise<void>;
+  pullImage: (image: string, log: LogFn, timeoutMs: number) => Promise<string>;
+};
+
+export type DeployDeps = {
+  steps: DeploySteps;
+  runtime: ReleaseRuntime;
+  publisher: RoutePublisher;
+  limits: Limits;
+  baseDomain: string;
+  hostPattern: string;
+  portRange: PortRange;
+  isPortFree: (port: number) => Promise<boolean>;
+  health: { timeoutMs: number; intervalMs: number };
+  drainMs: number;
+  tmpDir: string;
+};
 
-/** A release that started but never became healthy. runContainer has already
- *  recorded the outcome (and restored the previous image if it could), so the
- *  callers must not overwrite the statuses it set. */
-class ReleaseRejected extends Error {}
+// The ingress is told about live apps through this. The reference node installs
+// a publisher that registers routes with Nexus Cloud; until one is installed
+// nothing is announced, which is correct for a node with no ingress to tell.
+let routePublisher: RoutePublisher = new NullPublisher();
+export function setRoutePublisher(publisher: RoutePublisher): void {
+  routePublisher = publisher;
+}
+export function getRoutePublisher(): RoutePublisher {
+  return routePublisher;
+}
+
+export function defaultDeps(): DeployDeps {
+  return {
+    steps: {
+      clone: (source, dir, log, track) => cloneRepo(source, dir, config.allowLocalRepos, log, track),
+      buildImage,
+      pullImage,
+    },
+    runtime: dockerReleaseRuntime,
+    publisher: routePublisher,
+    limits: config.limits,
+    baseDomain: config.baseDomain,
+    hostPattern: config.appHostPattern,
+    portRange: config.appPortRange,
+    isPortFree: loopbackPortFree,
+    health: { timeoutMs: config.healthTimeoutMs, intervalMs: config.healthIntervalMs },
+    drainMs: 10_000,
+    tmpDir: os.tmpdir(),
+  };
+}
 
 // ── Log subscriber registry ────────────────────────────────────────────────
 type Subscriber = (line: string) => void;
@@ -23,20 +79,24 @@ const subscribers = new Map<string, Set<Subscriber>>();
 
 export function subscribeToDeployment(id: string, fn: Subscriber): () => void {
   if (!subscribers.has(id)) subscribers.set(id, new Set());
-  subscribers.get(id)!.add(fn);
-  return () => { subscribers.get(id)?.delete(fn); if (!subscribers.get(id)?.size) subscribers.delete(id); };
+  subscribers.get(id)?.add(fn);
+  return () => {
+    subscribers.get(id)?.delete(fn);
+    if (!subscribers.get(id)?.size) subscribers.delete(id);
+  };
 }
 
 function broadcast(id: string, line: string) {
-  subscribers.get(id)?.forEach(fn => fn(line));
+  subscribers.get(id)?.forEach((fn) => fn(line));
 }
 
 // ── Per-project build lock ─────────────────────────────────────────────────
 const buildingProjects = new Set<string>();
-export function isBuilding(projectId: string) { return buildingProjects.has(projectId); }
+export function isBuilding(projectId: string) {
+  return buildingProjects.has(projectId);
+}
 
 // ── Per-deployment cancel registry ────────────────────────────────────────
-// Stores a kill() fn for each active build so it can be cancelled
 const buildCancellers = new Map<string, () => void>();
 
 export async function cancelBuild(deploymentId: string): Promise<boolean> {
@@ -49,15 +109,15 @@ export async function cancelBuild(deploymentId: string): Promise<boolean> {
 // ── DB helpers ─────────────────────────────────────────────────────────────
 async function pushLog(depId: string, line: string) {
   broadcast(depId, line);
-  await writeDb(db => {
-    const dep = db.deployments.find(d => d.id === depId);
+  await writeDb((db) => {
+    const dep = db.deployments.find((d) => d.id === depId);
     if (dep) dep.logs = capLogs([...dep.logs, line]);
   });
 }
 
 async function setDepStatus(depId: string, status: Deployment["status"]) {
-  await writeDb(db => {
-    const dep = db.deployments.find(d => d.id === depId);
+  await writeDb((db) => {
+    const dep = db.deployments.find((d) => d.id === depId);
     if (dep) {
       dep.status = status;
       if (status === "live" || status === "failed" || status === "cancelled") dep.finishedAt = Date.now();
@@ -66,15 +126,37 @@ async function setDepStatus(depId: string, status: Deployment["status"]) {
 }
 
 async function setProjStatus(projectId: string, status: Project["status"], extra: Partial<Project> = {}) {
-  await writeDb(db => {
-    const p = db.projects.find(p => p.id === projectId);
+  await writeDb((db) => {
+    const p = db.projects.find((p) => p.id === projectId);
     if (p) Object.assign(p, { status, updatedAt: Date.now(), ...extra });
   });
   emitStatusChange(projectId, status);
 }
 
+// ── May this project build right now? ──────────────────────────────────────
+function lowestPercent(values: Array<number | null>): number | null {
+  const known = values.filter((v): v is number => v !== null);
+  return known.length ? Math.min(...known) : null;
+}
+
+export function guardBuild(project: Project, now: number = Date.now()): Verdict {
+  const sandboxFree = config.sandboxStoragePath ? diskFreePercent(config.sandboxStoragePath) : null;
+  return evaluateBuildGuard(
+    loadDb(),
+    project,
+    {
+      isBuilding,
+      hostFreePercent: lowestPercent([diskFreePercent(config.dataDir), diskFreePercent(os.tmpdir())]),
+      sandboxUsedPercent: sandboxFree === null ? null : 100 - sandboxFree,
+      now,
+    },
+    config.limits,
+  );
+}
+
 // ── Create deployment ──────────────────────────────────────────────────────
 export function createDeployment(project: Project, commitSha: string, triggeredBy: DeployTrigger = "manual"): Deployment {
+  const source = sourceOf(project);
   const dep: Deployment = {
     id: newId("dep"),
     projectId: project.id,
@@ -84,7 +166,7 @@ export function createDeployment(project: Project, commitSha: string, triggeredB
     imageTag: "",
     logs: [
       `[nexus] Deploy queued → ${project.name}`,
-      `[nexus] repo=${project.repo}  branch=${project.branch}`,
+      source.type === "git" ? `[nexus] repo=${source.repo}  branch=${source.branch}` : `[nexus] image=${source.image}`,
     ],
     createdAt: Date.now(),
   };
@@ -97,9 +179,7 @@ export function createDeployment(project: Project, commitSha: string, triggeredB
 // ── Notification webhook ───────────────────────────────────────────────────
 async function notify(project: Project, deployment: Deployment) {
   if (!project.notifyUrl) return;
-  const duration = deployment.finishedAt
-    ? Math.round((deployment.finishedAt - deployment.createdAt) / 1000)
-    : null;
+  const duration = deployment.finishedAt ? Math.round((deployment.finishedAt - deployment.createdAt) / 1000) : null;
   const body = {
     project: project.name,
     status: deployment.status,
@@ -121,199 +201,224 @@ async function notify(project: Project, deployment: Deployment) {
       body: JSON.stringify(payload),
       signal: AbortSignal.timeout(5000),
     });
-  } catch (e: any) {
-    console.warn(`[nexus] Notify failed for ${project.name}: ${e.message}`);
+  } catch (e) {
+    console.warn(`[nexus] Notify failed for ${project.name}: ${(e as Error).message}`);
   }
 }
 
+// ── Ports: one release at a time picks, and holds its pick ─────────────────
+let portChain: Promise<unknown> = Promise.resolve();
+function reservePort(projectId: string, deps: DeployDeps): Promise<number> {
+  const attempt = portChain.then(async () => {
+    const port = await allocatePort(deps.portRange, portsInUse(loadDb().projects), deps.isPortFree);
+    if (port === null) throw new Error("no free port is left in the app port range");
+    await writeDb((db) => {
+      const p = db.projects.find((p) => p.id === projectId);
+      if (p) p.pendingPort = port;
+    });
+    return port;
+  });
+  portChain = attempt.catch(() => {});
+  return attempt;
+}
+
+// ── Release: start the image, prove it, move the route ─────────────────────
+async function release(
+  snapshot: Project,
+  deploymentId: string,
+  imageRef: string,
+  deps: DeployDeps,
+  log: LogFn,
+): Promise<void> {
+  const project = loadDb().projects.find((p) => p.id === snapshot.id) ?? snapshot;
+  const host = appHost(project.name, deps.hostPattern, deps.baseDomain);
+  const hasVolume = Boolean(project.volumePath) && project.volumePath !== "/workspace";
+  const releaseProject: ReleaseProject = {
+    id: project.id,
+    slug: project.name,
+    host,
+    appPort: project.port ?? 3000,
+    healthPath: project.healthPath,
+    env: project.env,
+    limits: containerLimits(project, deps.limits),
+    volume: hasVolume ? { name: `nexus-vol-${project.name}`, path: project.volumePath } : undefined,
+    current:
+      project.containerName && project.hostPort !== undefined && project.imageTag
+        ? { containerName: project.containerName, imageRef: project.imageTag, hostPort: project.hostPort }
+        : undefined,
+  };
+
+  try {
+    const outcome = await performRelease(releaseProject, deploymentId, imageRef, {
+      runtime: deps.runtime,
+      publisher: deps.publisher,
+      allocatePort: () => reservePort(project.id, deps),
+      health: deps.health,
+      drainMs: deps.drainMs,
+      log,
+    });
+    if (outcome.ok) {
+      await setDepStatus(deploymentId, "live");
+      await setProjStatus(project.id, "live", {
+        containerName: outcome.containerName,
+        hostPort: outcome.hostPort,
+        imageTag: outcome.imageRef,
+        previousImageTag:
+          project.imageTag && project.imageTag !== outcome.imageRef ? project.imageTag : project.previousImageTag,
+        domain: host,
+      });
+      log(`[nexus] ✅ Live at https://${host}`);
+      return;
+    }
+    log(`[nexus] ✗ Release rejected: ${outcome.reason}`);
+    await setDepStatus(deploymentId, "failed");
+    if (outcome.servingPrevious) {
+      await setProjStatus(project.id, "live");
+    } else {
+      if (releaseProject.current) await deps.publisher.unpublish(project.name).catch(() => {});
+      await setProjStatus(project.id, "failed", { containerName: undefined, hostPort: undefined });
+    }
+  } finally {
+    await writeDb((db) => {
+      const p = db.projects.find((p) => p.id === project.id);
+      if (p) delete p.pendingPort;
+    });
+  }
+}
+
+/** A project that was serving keeps serving when a build fails before the release. */
+function wasServing(project: Project): boolean {
+  return project.status === "live" && Boolean(project.containerName);
+}
+
 // ── Full build pipeline ────────────────────────────────────────────────────
-export async function runDeploy(project: Project, deploymentId: string): Promise<void> {
+export async function runDeploy(
+  project: Project,
+  deploymentId: string,
+  deps: DeployDeps = defaultDeps(),
+): Promise<void> {
   if (buildingProjects.has(project.id)) {
     await pushLog(deploymentId, "[nexus] ✗ Another build is already running");
     await setDepStatus(deploymentId, "failed");
     return;
   }
   buildingProjects.add(project.id);
-  const buildDir = path.join(os.tmpdir(), `nexus-build-${deploymentId}`);
+
+  const source = sourceOf(project);
+  const secrets = [source.type === "git" ? source.token : undefined];
+  const emit = (line: string) => pushLog(deploymentId, redact(line, secrets));
+  const buildDir = path.join(deps.tmpDir, `nexus-build-${deploymentId}`);
+  const servedBefore = wasServing(project);
   let cancelled = false;
+  let timedOut = false;
 
-  // Register a cancel function
   const killFns: Array<() => void> = [];
+  const track: Track = (kill) => {
+    killFns.push(kill);
+  };
+  const stopSteps = () => {
+    for (const kill of killFns) kill();
+  };
   buildCancellers.set(deploymentId, () => {
     cancelled = true;
-    killFns.forEach(fn => fn());
+    stopSteps();
   });
+  const log: LogFn = (line) => {
+    if (!cancelled) void emit(line);
+  };
+  const assertActive = () => {
+    if (cancelled) throw new Error("cancelled");
+    if (timedOut) throw new Error("timed out");
+  };
+  let timer: NodeJS.Timeout | undefined;
 
   try {
-    const log = (line: string) => { if (!cancelled) pushLog(deploymentId, line); };
     await setDepStatus(deploymentId, "building");
     await setProjStatus(project.id, "building");
+    timer = setTimeout(() => {
+      timedOut = true;
+      stopSteps();
+    }, deps.limits.buildTimeoutMs);
 
-    // 1. Clone
-    log(`[nexus] Cloning ${project.repo} @ ${project.branch}…`);
-    fs.mkdirSync(buildDir, { recursive: true });
-    const repoUrl = resolveRepoUrl(project.repo, config.allowLocalRepos);
-
-    await new Promise<void>((resolve, reject) => {
-      const kill = spawnStreaming("git", ["clone", "--depth", "1", "--branch", project.branch, repoUrl, buildDir], log,
-        err => { if (cancelled) resolve(); else if (err) reject(err); else resolve(); });
-      killFns.push(kill);
-    });
-    if (cancelled) throw new Error("cancelled");
-    log(`[nexus] ✓ Clone complete`);
+    let imageRef: string;
+    if (source.type === "git") {
+      log(`[nexus] Cloning ${source.repo} @ ${source.branch}…`);
+      fs.mkdirSync(buildDir, { recursive: true });
+      await deps.steps.clone(source, buildDir, log, track);
+      assertActive();
+      log("[nexus] ✓ Clone complete");
 
-    // 2. Build image
-    const imageTag = `nexus/${project.name.toLowerCase().replace(/[^a-z0-9]/g, "-")}:${deploymentId.slice(-8)}`;
-    const contextDir = resolveBuildContext(buildDir, project.buildContext);
-    const dockerfile = resolveDockerfile(buildDir, validateDockerfile(project.dockerfile, project.buildContext ?? "."));
-    log(`[nexus] Building with ${dockerfile ? "Docker" : "Nixpacks"} → ${imageTag}`);
+      const tag = `nexus/${project.name.toLowerCase().replace(/[^a-z0-9]/g, "-")}:${deploymentId.slice(-8)}`;
+      const contextDir = resolveBuildContext(buildDir, project.buildContext);
+      const dockerfile = resolveDockerfile(buildDir, validateDockerfile(project.dockerfile, project.buildContext ?? "."));
+      log(`[nexus] Building with ${dockerfile ? "Docker" : "Nixpacks"} → ${tag}`);
+      await deps.steps.buildImage({ contextDir, dockerfile, tag }, log, track);
+      assertActive();
+      log("[nexus] ✓ Image built");
+      imageRef = tag;
+    } else {
+      log(`[nexus] Pulling ${source.image}…`);
+      const digest = await deps.steps.pullImage(source.image, log, deps.limits.pullTimeoutMs);
+      assertActive();
+      imageRef = pinnedImageRef(source.image, digest);
+      log(`[nexus] ✓ Pulled ${imageRef}`);
+    }
+    clearTimeout(timer);
 
-    await new Promise<void>((resolve, reject) => {
-      const cmd = dockerfile ? "docker" : "nixpacks";
-      const args = dockerfile ? ["build", "-t", imageTag, "-f", dockerfile, contextDir] : ["build", contextDir, "--name", imageTag];
-      const kill = spawnStreaming(cmd, args, log,
-        err => { if (cancelled) resolve(); else if (err) reject(err); else resolve(); });
-      killFns.push(kill);
+    await writeDb((db) => {
+      const d = db.deployments.find((d) => d.id === deploymentId);
+      if (d) d.imageTag = imageRef;
     });
-    if (cancelled) throw new Error("cancelled");
-    log(`[nexus] ✓ Image built`);
-
-    await writeDb(db => { const d = db.deployments.find(d => d.id === deploymentId); if (d) d.imageTag = imageTag; });
-    await runContainer(project, deploymentId, imageTag, log);
-
-  } catch (err: any) {
-    if (err instanceof ReleaseRejected) {
-      await pushLog(deploymentId, `[nexus] ✗ Release rejected: ${err.message}`);
-    } else if (cancelled || err?.message === "cancelled") {
-      await pushLog(deploymentId, `[nexus] ✗ Build cancelled`);
-      await setDepStatus(deploymentId, "cancelled");
-      await setProjStatus(project.id, "failed");
+    await release(project, deploymentId, imageRef, deps, log);
+  } catch (err) {
+    const message = (err as Error)?.message ?? String(err);
+    let status: Deployment["status"] = "failed";
+    if (cancelled || message === "cancelled") {
+      status = "cancelled";
+      await emit("[nexus] ✗ Build cancelled");
+    } else if (timedOut) {
+      await emit(`[nexus] ✗ Build exceeded ${Math.round(deps.limits.buildTimeoutMs / 60_000)} minutes and was stopped`);
     } else {
-      await pushLog(deploymentId, `[nexus] ✗ Build failed: ${err?.message ?? String(err)}`);
-      await setDepStatus(deploymentId, "failed");
-      await setProjStatus(project.id, "failed");
+      await emit(`[nexus] ✗ Build failed: ${message}`);
     }
+    await setDepStatus(deploymentId, status);
+    await setProjStatus(project.id, servedBefore ? "live" : "failed");
   } finally {
+    if (timer) clearTimeout(timer);
     buildCancellers.delete(deploymentId);
     buildingProjects.delete(project.id);
     fs.rmSync(buildDir, { recursive: true, force: true });
     trimDeployments(project.id).catch(() => {});
-    // Send notification
-    const dep = loadDb().deployments.find(d => d.id === deploymentId);
+    const dep = loadDb().deployments.find((d) => d.id === deploymentId);
     if (dep) notify(project, dep).catch(() => {});
   }
 }
 
 // ── Rollback ───────────────────────────────────────────────────────────────
-export async function runRollback(project: Project, deploymentId: string, imageTag: string): Promise<void> {
+export async function runRollback(
+  project: Project,
+  deploymentId: string,
+  imageRef: string,
+  deps: DeployDeps = defaultDeps(),
+): Promise<void> {
   if (buildingProjects.has(project.id)) return;
   buildingProjects.add(project.id);
+  const servedBefore = wasServing(project);
   try {
-    const log = (line: string) => pushLog(deploymentId, line);
+    const log: LogFn = (line) => {
+      void pushLog(deploymentId, line);
+    };
     await setDepStatus(deploymentId, "building");
     await setProjStatus(project.id, "building");
-    log(`[nexus] Rolling back → ${imageTag}`);
-    await runContainer(project, deploymentId, imageTag, log);
-  } catch (err: any) {
-    await pushLog(deploymentId, `[nexus] ✗ Rollback failed: ${err?.message}`);
-    if (!(err instanceof ReleaseRejected)) {
-      await setDepStatus(deploymentId, "failed");
-      await setProjStatus(project.id, "failed");
-    }
+    log(`[nexus] Rolling back → ${imageRef}`);
+    await release(project, deploymentId, imageRef, deps, log);
+  } catch (err) {
+    await pushLog(deploymentId, `[nexus] ✗ Rollback failed: ${(err as Error)?.message}`);
+    await setDepStatus(deploymentId, "failed");
+    await setProjStatus(project.id, servedBefore ? "live" : "failed");
   } finally {
     buildingProjects.delete(project.id);
-    const dep = loadDb().deployments.find(d => d.id === deploymentId);
+    const dep = loadDb().deployments.find((d) => d.id === deploymentId);
     if (dep) notify(project, dep).catch(() => {});
   }
 }
-
-// ── Shared: start container, then prove it serves ──────────────────────────
-// "live" is a claim that the app answers, so it is only recorded after the
-// health probe passes. A release that never becomes healthy is rejected and
-// the image that was live before it is started again, so a bad release costs
-// a short outage instead of leaving the app down. (The old container is
-// replaced in place; zero-downtime blue/green would need a second port.)
-async function runContainer(snapshot: Project, deploymentId: string, imageTag: string, log: (l: string) => void) {
-  const project = loadDb().projects.find(p => p.id === snapshot.id) ?? snapshot;
-  // imageTag on the project is only ever written when a release passed its probe.
-  const previousImage = project.imageTag && project.imageTag !== imageTag ? project.imageTag : undefined;
-  const domain = project.customDomain ?? `${project.name}.${config.baseDomain}`;
-
-  log(`[nexus] Starting container → https://${domain}`);
-  const containerId = await startContainer(project, imageTag, domain, log);
-  const health = await checkHealth(project, log);
-  if (health.ok) {
-    log(`[nexus] ✅ Live at https://${domain}`);
-    await setDepStatus(deploymentId, "live");
-    await setProjStatus(project.id, "live", { containerId, domain, imageTag });
-    return;
-  }
-
-  log(`[nexus] ✗ Health check failed: ${health.reason}`);
-  for (const line of await dockerLogTail(containerName(project))) log(`[app] ${line}`);
-  await setDepStatus(deploymentId, "failed");
-
-  if (previousImage) {
-    log(`[nexus] Restoring previous release → ${previousImage}`);
-    const restoredId = await startContainer(project, previousImage, domain, log);
-    const restored = await checkHealth(project, log);
-    if (restored.ok) {
-      log(`[nexus] ✓ Previous release is serving again`);
-      await setProjStatus(project.id, "live", { containerId: restoredId, domain, imageTag: previousImage });
-      throw new ReleaseRejected(health.reason);
-    }
-    log(`[nexus] ✗ Previous release is unhealthy too: ${restored.reason}`);
-  }
-  // Nothing healthy to serve: stop the crash loop rather than leave it restarting.
-  await dockerRemove(containerName(project));
-  await setProjStatus(project.id, "failed");
-  throw new ReleaseRejected(health.reason);
-}
-
-function containerName(project: Project) {
-  return `nexus-app-${project.name}`;
-}
-
-function startContainer(project: Project, imageTag: string, domain: string, log: (l: string) => void) {
-  const env: Record<string, string> = { PORT: String(project.port ?? 3000), NODE_ENV: "production", ...project.env };
-  // Named volume for persistence across redeploys
-  const volumes: string[] = [];
-  if (project.volumePath && project.volumePath !== "/workspace") {
-    volumes.push(`nexus-vol-${project.name}:${project.volumePath}`);
-  }
-  return dockerRun({
-    image: imageTag,
-    name: containerName(project),
-    env,
-    domain,
-    port: project.port ?? 3000,
-    network: config.dockerNetwork,
-    onLine: log,
-    volumes,
-    memoryLimit: project.memoryLimit || undefined,
-    cpus: project.cpus || undefined,
-  });
-}
-
-async function checkHealth(project: Project, log: (l: string) => void): Promise<HealthResult> {
-  const name = containerName(project);
-  const healthPath = project.healthPath || "/";
-  log(`[nexus] Waiting for ${healthPath} to answer (up to ${Math.round(config.healthTimeoutMs / 1000)}s)…`);
-  return waitForHealthy({
-    requireOk: Boolean(project.healthPath),
-    timeoutMs: config.healthTimeoutMs,
-    intervalMs: config.healthIntervalMs,
-    // A crash-looping container under --restart flips through "restarting".
-    isRunning: async () => ["running", "created"].includes(await dockerStatus(name)),
-    probe: async () => {
-      const host = await dockerContainerHost(name, config.dockerNetwork);
-      if (!host) return null;
-      const response = await fetch(`http://${host}:${project.port ?? 3000}${healthPath}`, {
-        redirect: "manual",
-        signal: AbortSignal.timeout(2_000),
-      });
-      await response.body?.cancel();
-      return response.status;
-    },
-  });
-}
````

**Create `src/lib/containers.ts`:**

````typescript
import type { Project } from "../types.js";

/**
 * The container serving a project now. Releases are blue/green, so each one gets
 * its own name; projects from before that fall back to the original fixed name.
 */
export function containerNameOf(project: Pick<Project, "name" | "containerName">): string {
  return project.containerName ?? `nexus-app-${project.name}`;
}
````

**Create `src/lib/docker-runtime.ts`:**

````typescript
import { dockerLogTail, dockerRemove, dockerRunHardened, dockerStatus } from "./docker.js";
import { probeLoopback } from "./health.js";
import type { ReleaseRuntime } from "./release.js";

/** The real runtime: everything goes to the sandbox daemon through docker.ts. */
export const dockerReleaseRuntime: ReleaseRuntime = {
  run: (spec, onLine) =>
    dockerRunHardened({
      name: spec.name,
      image: spec.image,
      env: spec.env,
      appPort: spec.appPort,
      hostPort: spec.hostPort,
      limits: spec.limits,
      volume: spec.volume,
      onLine,
    }),
  probe: (hostPort, path) => probeLoopback(hostPort, path),
  status: (name) => dockerStatus(name),
  logTail: (name) => dockerLogTail(name),
  remove: (name) => dockerRemove(name),
};
````

**Modify `src/lib/docker.ts`:**

````diff
--- a/src/lib/docker.ts
+++ b/src/lib/docker.ts
@@ -124,76 +124,12 @@ export function spawnStreaming(
   };
 }
 
-// ── docker run ─────────────────────────────────────────────────────────────
-export async function dockerRun(opts: {
-  image: string;
-  name: string;
-  env: Record<string, string>;
-  domain: string;
-  port: number;
-  network: string;
-  onLine: LogLine;
-  volumes?: string[];       // ["host:container", ...]
-  memoryLimit?: string;     // e.g. "512m", "1g"
-  cpus?: string;            // e.g. "0.5", "2"
-}): Promise<string> {
-  const { image, name, env, domain, port, network, onLine, volumes = [], memoryLimit, cpus } = opts;
-
-  await spawnStream("docker", ["rm", "-f", name], () => {}).catch(() => {});
-
-  const envFlags   = Object.entries(env).flatMap(([k, v]) => ["-e", `${k}=${v}`]);
-  const volFlags   = volumes.flatMap(v => ["-v", v]);
-  const limitFlags = [
-    ...(memoryLimit ? ["--memory", memoryLimit] : []),
-    ...(cpus        ? ["--cpus",   cpus]        : []),
-  ];
-
-  const labels = [
-    `traefik.enable=true`,
-    `traefik.http.routers.${name}.rule=Host(\`${domain}\`)`,
-    `traefik.http.routers.${name}.tls=true`,
-    `traefik.http.routers.${name}.tls.certresolver=letsencrypt`,
-    `traefik.http.services.${name}.loadbalancer.server.port=${port}`,
-  ];
-  const labelFlags = labels.flatMap(l => ["-l", l]);
-
-  const args = [
-    "run", "-d",
-    "--name", name,
-    "--network", network,
-    "--restart", "unless-stopped",
-    ...envFlags,
-    ...volFlags,
-    ...limitFlags,
-    ...labelFlags,
-    image,
-  ];
-
-  let containerId = "";
-  await spawnStream("docker", args, line => {
-    containerId = line.trim();
-    onLine(`[nexus] Container started: ${line.trim().slice(0, 12)}`);
-  });
-  return containerId;
-}
-
 // ── docker stop / start / restart / remove ─────────────────────────────────
 export async function dockerStop(name: string)    { await spawnStream("docker", ["stop",    name], () => {}).catch(() => {}); }
 export async function dockerStart(name: string)   { await spawnStream("docker", ["start",   name], () => {}).catch(() => {}); }
 export async function dockerRestart(name: string) { await spawnStream("docker", ["restart", name], () => {}).catch(() => {}); }
 export async function dockerRemove(name: string)  { await spawnStream("docker", ["rm", "-f", name], () => {}).catch(() => {}); }
 
-// ── Where to reach a container from this process ──────────────────────────
-// On the host network the app listens on the host's loopback; on a bridge
-// network it has its own address on that network, reachable from the host.
-export async function dockerContainerHost(name: string, network: string): Promise<string | null> {
-  if (network === "host") return "127.0.0.1";
-  let out = "";
-  // spawnStream merges stderr into the lines, so only trust output on success.
-  return spawnStream("docker", ["inspect", "-f", `{{with index .NetworkSettings.Networks "${network}"}}{{.IPAddress}}{{end}}`, name], l => { out = l; })
-    .then(() => out.trim() || null, () => null);
-}
-
 /** Last lines of a container's output, for the deployment log when it fails. */
 export async function dockerLogTail(name: string, lines = 20): Promise<string[]> {
   const out: string[] = [];
````

**Create `src/lib/guard.ts`:**

````typescript
/**
 * The question "may this project build right now?", answered from the database
 * and a few measurements passed in, so it needs no disk, clock or Docker to test.
 */
import { checkBuildAllowed, countRecentBuilds, type Limits, type Verdict } from "./safeguards.js";

export type GuardData = {
  projects: ReadonlyArray<{ id: string; ownerId?: string | undefined }>;
  deployments: ReadonlyArray<{ projectId: string; triggeredBy: string; createdAt: number }>;
};

export type GuardContext = {
  isBuilding: (projectId: string) => boolean;
  hostFreePercent: number | null;
  sandboxUsedPercent: number | null;
  now: number;
};

/** A member's builds are counted across all the projects they own. */
export function evaluateBuildGuard(
  data: GuardData,
  project: { ownerId?: string | undefined },
  ctx: GuardContext,
  limits: Limits,
): Verdict {
  const owned = new Set(
    data.projects
      .filter((p) => p.ownerId !== undefined && p.ownerId === project.ownerId)
      .map((p) => p.id),
  );
  return checkBuildAllowed(
    {
      activeBuildsForMember: [...owned].filter((id) => ctx.isBuilding(id)).length,
      buildsLastHourForMember: countRecentBuilds(data.deployments, owned, ctx.now),
      hostFreePercent: ctx.hostFreePercent,
      sandboxUsedPercent: ctx.sandboxUsedPercent,
    },
    limits,
  );
}
````

**Modify `src/types.ts`:**

````diff
--- a/src/types.ts
+++ b/src/types.ts
@@ -51,7 +51,15 @@ export type Project = {
   domain?: string;
   customDomain?: string;
   containerId?: string;
+  /** Name of the container serving now. Releases are blue/green, so it changes on every release. */
+  containerName?: string;
+  /** Loopback port the serving container is published on. */
+  hostPort?: number;
+  /** Port reserved for a release in progress, so no other project picks it. */
+  pendingPort?: number;
   imageTag?: string;
+  /** The image the serving release replaced; kept so a rollback target survives pruning. */
+  previousImageTag?: string;
   webhookSecret?: string;
   memoryLimit?: string;
   cpus?: string;
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **121 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/build-steps.ts src/lib/build.ts src/lib/containers.ts src/lib/docker-runtime.ts src/lib/docker.ts src/lib/guard.ts src/types.ts tests/deploy-flow.test.ts tests/guard.test.ts
git commit \
  -m "feat(build): deployments flow through sources, safeguards and blue/green release" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 10: Audit log without addresses, kept thirty days

An operator must be able to answer who suspended an account or deleted a project. The log records the account, the action and the time only, never an address or user agent, and forgets after thirty days, in line with the Charter.

**Files:**

- Create: `src/lib/audit.ts`
- Create: `tests/audit.test.ts`
- Modify: `src/lib/store.ts`
- Modify: `tests/deploy-flow.test.ts`

**Interfaces:**

- Consumes: `newId` from `src/lib/auth.ts` (existing).
- Produces: `AuditEntry`, `AuditAction`, `AUDIT_RETENTION_MS`, `recordAudit`, `pruneAudit`, `Database.audit`.

- [ ] **Step 1: Write the tests**

**Create `tests/audit.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { AUDIT_RETENTION_MS, pruneAudit, recordAudit, type AuditEntry } from "../src/lib/audit.js";

const DAY = 24 * 60 * 60 * 1000;

test("an entry records who did what to what, and when — and nothing about where from", () => {
  const log: AuditEntry[] = [];
  const entry = recordAudit(log, { actor: "ada", action: "project.delete", subject: "prj_1" }, 1_000);
  assert.deepEqual(Object.keys(entry).sort(), ["action", "actor", "at", "id", "subject"]);
  assert.equal(entry.at, 1_000);
  assert.equal(log[0], entry);
});

test("the subject is optional", () => {
  const log: AuditEntry[] = [];
  const entry = recordAudit(log, { actor: "ada", action: "member.grant" }, 1);
  assert.equal("subject" in entry, false);
});

test("newest first", () => {
  const log: AuditEntry[] = [];
  recordAudit(log, { actor: "a", action: "deploy.start" }, 1);
  recordAudit(log, { actor: "b", action: "deploy.cancel" }, 2);
  assert.deepEqual(
    log.map((e) => e.actor),
    ["b", "a"],
  );
});

test("entries older than thirty days are forgotten as new ones arrive", () => {
  const log: AuditEntry[] = [];
  const now = 100 * DAY;
  recordAudit(log, { actor: "old", action: "deploy.start" }, now - 31 * DAY);
  recordAudit(log, { actor: "edge", action: "deploy.start" }, now - 29 * DAY);
  recordAudit(log, { actor: "new", action: "deploy.start" }, now);
  assert.deepEqual(
    log.map((e) => e.actor),
    ["new", "edge"],
  );
});

test("pruning reports how many it dropped and is idempotent", () => {
  const now = 100 * DAY;
  const log: AuditEntry[] = [
    { id: "1", at: now - AUDIT_RETENTION_MS - 1, actor: "x", action: "deploy.start" },
    { id: "2", at: now, actor: "y", action: "deploy.start" },
  ];
  assert.equal(pruneAudit(log, now), 1);
  assert.equal(pruneAudit(log, now), 0);
  assert.equal(log.length, 1);
});

test("the retention is thirty days", () => {
  assert.equal(AUDIT_RETENTION_MS, 30 * DAY);
});
````

**Modify `tests/deploy-flow.test.ts`:**

````diff
--- a/tests/deploy-flow.test.ts
+++ b/tests/deploy-flow.test.ts
@@ -42,7 +42,7 @@ function project(patch: Partial<Project> = {}): Project {
 }
 
 function seed(...projects: Project[]) {
-  store.saveDb({ users: [], projects, deployments: [], sessions: [], members: [] });
+  store.saveDb({ users: [], projects, deployments: [], sessions: [], members: [], audit: [] });
 }
 
 const projectOf = (id = "prj_1") => store.loadDb().projects.find((p) => p.id === id) as Project;
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/audit.test.ts tests/deploy-flow.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/audit.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/audit.ts`:**

````typescript
/**
 * The audit log: which account did what, and when.
 *
 * It exists so an operator can answer "who suspended this?" and "who deleted
 * that project?". It deliberately records no address and no user agent, and it
 * forgets after thirty days, the same as sign-in history, because the Charter
 * promises we keep only what we need.
 */
import { newId } from "./auth.js";

export type AuditAction =
  | "member.grant"
  | "member.suspend"
  | "member.reactivate"
  | "project.create"
  | "project.update"
  | "project.delete"
  | "project.suspend"
  | "deploy.start"
  | "deploy.rollback"
  | "deploy.cancel"
  | "project.stop"
  | "project.start"
  | "project.restart"
  | "logs.delete";

export type AuditEntry = {
  id: string;
  at: number;
  /** Nexus-Auth id of the account that acted. */
  actor: string;
  action: AuditAction;
  /** The project or account acted upon, when there is one. */
  subject?: string;
};

export const AUDIT_RETENTION_MS = 30 * 24 * 60 * 60 * 1000;

/** Adds an entry to `log` and drops anything older than the retention window. */
export function recordAudit(
  log: AuditEntry[],
  entry: { actor: string; action: AuditAction; subject?: string },
  now: number,
): AuditEntry {
  const full: AuditEntry = {
    id: newId("aud"),
    at: now,
    actor: entry.actor,
    action: entry.action,
    ...(entry.subject !== undefined ? { subject: entry.subject } : {}),
  };
  log.unshift(full);
  pruneAudit(log, now);
  return full;
}

/** In place. Returns how many entries were dropped. */
export function pruneAudit(log: AuditEntry[], now: number): number {
  const keep = log.filter((e) => now - e.at < AUDIT_RETENTION_MS);
  const dropped = log.length - keep.length;
  if (dropped > 0) log.splice(0, log.length, ...keep);
  return dropped;
}
````

**Modify `src/lib/store.ts`:**

````diff
--- a/src/lib/store.ts
+++ b/src/lib/store.ts
@@ -7,6 +7,7 @@
  */
 import fs from "node:fs";
 import { config, ensureDataDir } from "./config.js";
+import type { AuditEntry } from "./audit.js";
 import { decryptJson, encryptJson } from "./crypto.js";
 import type { Deployment, Member, Project, Session, User } from "../types.js";
 
@@ -16,6 +17,8 @@ export type Database = {
   deployments: Deployment[];
   sessions: Session[];
   members: Member[];
+  /** Who did what, newest first, kept thirty days. No addresses. */
+  audit: AuditEntry[];
 };
 
 const emptyDb: Database = {
@@ -24,6 +27,7 @@ const emptyDb: Database = {
   deployments: [],
   sessions: [],
   members: [],
+  audit: [],
 };
 
 // ── Read ───────────────────────────────────────────────────────────────────
@@ -35,6 +39,7 @@ export function loadDb(): Database {
   try {
     const db = decryptJson<Database>(config.appSecret, raw);
     db.members ??= [];
+    db.audit ??= [];
     for (const project of db.projects) {
       if (project.buildContext === undefined) project.buildContext = ".";
     }
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **127 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/audit.ts src/lib/store.ts tests/audit.test.ts tests/deploy-flow.test.ts
git commit \
  -m "feat(audit): who-did-what log with no addresses, kept thirty days" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 11: Strict validation for everything a member can submit

Every field a member can set is validated at the boundary before it is stored. This also closes a request-forgery hole: a project's notification URL makes this server send a request to an address a member chose, so it must be https and must not name a private or local host.

**Files:**

- Create: `src/lib/project-input.ts`
- Create: `src/lib/urls.ts`
- Create: `tests/project-input.test.ts`

**Interfaces:**

- Consumes: `validateBuildContext`, `validateDockerfile` (existing); Task 5 validators; `validateVolumePath` (Task 7); `parseMemoryMb` (Task 3).
- Produces: `validateNotifyUrl`, `isPrivateHost`; `parseSource`, `parseNewProject`, `parsePatch`, `applyPatch`, `validatePort`, `validateHealthPath`, `NewProject`, `ProjectPatch`.

- [ ] **Step 1: Write the tests**

**Create `tests/project-input.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { applyPatch, parseNewProject, parsePatch, parseSource, validateHealthPath, validatePort } from "../src/lib/project-input.js";
import { isPrivateHost, validateNotifyUrl } from "../src/lib/urls.js";
import type { Project } from "../src/types.js";

const git = { repo: "o/r", branch: "main", source: undefined as Project["source"] };

test("a webhook URL must be https and must not name a private or local address", () => {
  assert.equal(validateNotifyUrl("https://hooks.example.test/x"), "https://hooks.example.test/x");
  for (const bad of [
    "http://hooks.example.test/x",
    "https://user:pw@hooks.example.test/x",
    "https://localhost/x",
    "https://127.0.0.1/x",
    "https://10.1.2.3/x",
    "https://172.16.0.9/x",
    "https://172.31.255.255/x",
    "https://192.168.1.1/x",
    "https://169.254.169.254/latest/meta-data",
    "https://100.64.0.1/x",
    "https://[::1]/x",
    "https://[fd00::1]/x",
    "https://[::ffff:127.0.0.1]/x",
    "https://printer.local/x",
    "ftp://example.test/x",
    "not a url",
    "",
  ]) {
    assert.throws(() => validateNotifyUrl(bad), /notifyUrl/, bad);
  }
  assert.equal(isPrivateHost("172.32.0.1"), false, "172.32 is public; only 172.16/12 is private");
  assert.equal(isPrivateHost("8.8.8.8"), false);
});

test("ports and health paths", () => {
  assert.equal(validatePort(3000), 3000);
  assert.equal(validatePort("8080"), 8080);
  for (const bad of [0, 65536, 1.5, "abc", null, undefined, -1]) assert.throws(() => validatePort(bad), /port/);
  assert.equal(validateHealthPath("/health"), "/health");
  assert.equal(validateHealthPath("/"), "/");
  for (const bad of ["health", "/a b", "/a\nb", "", 5]) assert.throws(() => validateHealthPath(bad), /healthPath/);
});

test("a source is either the nested form or the older flat repo and branch", () => {
  assert.deepEqual(parseSource({ repo: "o/r" }, false), { type: "git", repo: "o/r", branch: "main" });
  assert.deepEqual(parseSource({ repo: "o/r", branch: "dev", token: "ghp_abcd1234" }, false), {
    type: "git",
    repo: "o/r",
    branch: "dev",
    token: "ghp_abcd1234",
  });
  assert.deepEqual(parseSource({ source: { type: "image", image: "nginx:1.27" } }, false), {
    type: "image",
    image: "nginx:1.27",
  });
  assert.throws(() => parseSource({}, false), /Missing source/);
  assert.throws(() => parseSource({ source: { type: "svn" } }, false), /source.type/);
  assert.throws(() => parseSource({ source: { type: "image", image: "x; rm -rf /" } }, false), /image must look like/);
  assert.throws(() => parseSource({ repo: "file:///etc" }, false), /not allowed/);
  assert.throws(() => parseSource({ repo: "o/r", branch: "--upload-pack=x" }, false), /branch/);
  assert.throws(() => parseSource({ source: 5 }, false), /source must be/);
});

test("a new project needs a name and a source, and gets sensible defaults", () => {
  const p = parseNewProject({ name: "My App", repo: "o/r" }, false);
  assert.equal(p.name, "My App");
  assert.deepEqual(p.source, { type: "git", repo: "o/r", branch: "main" });
  assert.equal(p.port, 3000);
  assert.equal(p.volumePath, "/workspace");
  assert.equal(p.buildContext, ".");
  assert.throws(() => parseNewProject({ repo: "o/r" }, false), /Missing name/);
  assert.throws(() => parseNewProject("nope", false), /JSON object/);
  assert.throws(() => parseNewProject({ name: "x", repo: "o/r", volumePath: "/a:/b" }, false), /volumePath/);
  assert.throws(() => parseNewProject({ name: "x", repo: "o/r", port: 99999 }, false), /port/);
  assert.throws(() => parseNewProject({ name: "x", repo: "o/r", buildContext: "../up" }, false), /buildContext/);
  assert.throws(() => parseNewProject({ name: "x", repo: "o/r", buildCommand: "a\nb" }, false), /buildCommand/);
});

test("an image project ignores build settings that only apply to a repository", () => {
  const p = parseNewProject(
    { name: "web", source: { type: "image", image: "nginx" }, buildContext: "sub", dockerfile: "x/Dockerfile" },
    false,
  );
  assert.equal(p.buildContext, ".");
  assert.equal("dockerfile" in p, false);
});

test("a patch changes only what it names, and null clears an optional field", () => {
  const patch = parsePatch({ healthPath: "/up", memoryLimit: "256m", cpus: null, notifyUrl: "" }, git, false);
  assert.deepEqual(patch, { healthPath: "/up", memoryLimit: "256m", cpus: null, notifyUrl: null });
  assert.deepEqual(parsePatch({}, git, false), {});
  assert.throws(() => parsePatch({ memoryLimit: "lots" }, git, false), /memoryLimit/);
  assert.throws(() => parsePatch({ cpus: -1 }, git, false), /cpus/);
  assert.throws(() => parsePatch({ notifyUrl: "http://169.254.169.254/" }, git, false), /notifyUrl/);
  assert.throws(() => parsePatch("x", git, false), /JSON object/);
});

test("changing the branch keeps the repository and the stored token", () => {
  const current = { repo: "o/r", branch: "main", source: { type: "git" as const, repo: "o/r", branch: "main", token: "ghp_keepme123" } };
  assert.deepEqual(parsePatch({ branch: "dev" }, current, false).source, {
    type: "git",
    repo: "o/r",
    branch: "dev",
    token: "ghp_keepme123",
  });
  assert.deepEqual(parsePatch({ token: "ghp_newtoken1" }, current, false).source, {
    type: "git",
    repo: "o/r",
    branch: "main",
    token: "ghp_newtoken1",
  });
  assert.deepEqual(parsePatch({ token: null }, current, false).source, { type: "git", repo: "o/r", branch: "main" });
});

test("an image project's image can be changed, and a repo field is ignored for it", () => {
  const current = { repo: "nginx", branch: "", source: { type: "image" as const, image: "nginx" } };
  assert.deepEqual(parsePatch({ image: "nginx:1.28" }, current, false).source, { type: "image", image: "nginx:1.28" });
  assert.equal(parsePatch({ repo: "o/r" }, current, false).source, undefined);
});

test("applying a patch updates the project and keeps the display copy of the source in step", () => {
  const project = {
    repo: "o/r",
    branch: "main",
    source: { type: "git", repo: "o/r", branch: "main" },
    healthPath: "/old",
    cpus: "1",
    updatedAt: 1,
  } as unknown as Project;
  applyPatch(project, { source: { type: "image", image: "nginx:1" }, healthPath: null, cpus: null, port: 8080 }, 99);
  assert.equal(project.repo, "nginx:1");
  assert.equal(project.branch, "");
  assert.equal(project.port, 8080);
  assert.equal("healthPath" in project, false);
  assert.equal("cpus" in project, false);
  assert.equal(project.updatedAt, 99);
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/project-input.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/project-input.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/project-input.ts`:**

````typescript
/**
 * Turning a request body into something safe to store.
 *
 * Every field a member can set is validated here, at the boundary, before it is
 * persisted: a project that could never build, or that would clone off this
 * machine's disk, or smuggle an option into a command, is refused at the door.
 * Errors are plain messages that the route returns as a 400.
 */
import { validateBuildContext, validateDockerfile } from "./build-context.js";
import { resolveRepoUrl } from "./repo.js";
import { validateVolumePath } from "./run-args.js";
import { parseMemoryMb } from "./safeguards.js";
import { sourceOf, validateBranch, validateImageRef, validateToken } from "./source.js";
import { validateNotifyUrl } from "./urls.js";
import type { Project, Source } from "../types.js";

type Body = Record<string, unknown>;

function asBody(value: unknown, what = "request body"): Body {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error(`${what} must be a JSON object`);
  }
  return value as Body;
}

const present = (v: unknown) => v !== undefined && v !== null && v !== "";

function gitSource(body: Body, allowLocalRepos: boolean, fallbackBranch = "main"): Source {
  const repo = typeof body["repo"] === "string" ? body["repo"] : "";
  if (!repo) throw new Error("Missing repo");
  resolveRepoUrl(repo, allowLocalRepos);
  return {
    type: "git",
    repo,
    branch: body["branch"] === undefined ? fallbackBranch : validateBranch(body["branch"]),
    ...(present(body["token"]) ? { token: validateToken(body["token"]) } : {}),
  };
}

/** `{ source: { type, … } }`, or the older flat `{ repo, branch }`. */
export function parseSource(body: Body, allowLocalRepos: boolean): Source {
  if (body["source"] !== undefined) {
    const source = asBody(body["source"], "source");
    if (source["type"] === "image") return { type: "image", image: validateImageRef(source["image"]) };
    if (source["type"] === "git") return gitSource(source, allowLocalRepos);
    throw new Error('source.type must be "git" or "image"');
  }
  if (body["repo"] === undefined) throw new Error("Missing source (or repo)");
  return gitSource(body, allowLocalRepos);
}

export function validatePort(value: unknown): number {
  const port = typeof value === "string" ? Number(value) : value;
  if (typeof port !== "number" || !Number.isInteger(port) || port < 1 || port > 65535) {
    throw new Error("port must be a whole number between 1 and 65535");
  }
  return port;
}

export function validateHealthPath(value: unknown): string {
  if (typeof value !== "string" || !/^\/[\x21-\x7e]{0,255}$/.test(value)) {
    throw new Error("healthPath must be an absolute path such as /health");
  }
  return value;
}

function validateCommand(value: unknown, field: string): string {
  if (typeof value !== "string" || value.length > 500 || /[\n\r\0]/.test(value)) {
    throw new Error(`${field} must be a single line of at most 500 characters`);
  }
  return value;
}

function validateMemory(value: unknown): string {
  if (typeof value !== "string" || parseMemoryMb(value) === null || (parseMemoryMb(value) ?? 0) <= 0) {
    throw new Error("memoryLimit must look like 256m or 1g");
  }
  return value;
}

function validateCpus(value: unknown): string {
  const n = typeof value === "string" ? Number(value) : value;
  if (typeof n !== "number" || !Number.isFinite(n) || n <= 0 || n > 64) throw new Error("cpus must be a positive number");
  return String(n);
}

function validateVolume(value: unknown): string {
  return value === "/workspace" ? "/workspace" : validateVolumePath(value);
}

export type NewProject = {
  name: string;
  source: Source;
  buildContext: string;
  dockerfile?: string;
  healthPath?: string;
  buildCommand: string;
  startCommand: string;
  volumePath: string;
  port: number;
};

export function parseNewProject(raw: unknown, allowLocalRepos: boolean): NewProject {
  const body = asBody(raw);
  if (typeof body["name"] !== "string" || !body["name"].trim()) throw new Error("Missing name");
  const source = parseSource(body, allowLocalRepos);
  return {
    name: body["name"],
    source,
    buildContext: source.type === "git" ? validateBuildContext(body["buildContext"] ?? ".") : ".",
    ...(source.type === "git" && present(body["dockerfile"])
      ? { dockerfile: validateDockerfile(body["dockerfile"], ".") }
      : {}),
    ...(present(body["healthPath"]) ? { healthPath: validateHealthPath(body["healthPath"]) } : {}),
    buildCommand: body["buildCommand"] === undefined ? "npm run build" : validateCommand(body["buildCommand"], "buildCommand"),
    startCommand: body["startCommand"] === undefined ? "npm start" : validateCommand(body["startCommand"], "startCommand"),
    volumePath: body["volumePath"] === undefined ? "/workspace" : validateVolume(body["volumePath"]),
    port: body["port"] === undefined ? 3000 : validatePort(body["port"]),
  };
}

export type ProjectPatch = {
  source?: Source;
  buildContext?: string;
  dockerfile?: string | null;
  healthPath?: string | null;
  buildCommand?: string;
  startCommand?: string;
  volumePath?: string;
  port?: number;
  memoryLimit?: string | null;
  cpus?: string | null;
  notifyUrl?: string | null;
  autoDeployEnabled?: boolean;
};

const cleared = (v: unknown) => v === null || v === "";

/**
 * Only the fields present in the body are changed. `null` or "" clears an
 * optional field. A token is replaced when one is sent and removed with
 * `token: null`; otherwise the stored one is kept.
 */
export function parsePatch(
  raw: unknown,
  current: Pick<Project, "repo" | "branch" | "source">,
  allowLocalRepos: boolean,
): ProjectPatch {
  const body = asBody(raw);
  const patch: ProjectPatch = {};
  const existing = sourceOf(current);

  if (body["source"] !== undefined) {
    patch.source = parseSource(body, allowLocalRepos);
  } else if (existing.type === "git" && ["repo", "branch", "token"].some((k) => body[k] !== undefined)) {
    const next = gitSource(
      { repo: body["repo"] ?? existing.repo, branch: body["branch"] ?? existing.branch },
      allowLocalRepos,
    ) as Extract<Source, { type: "git" }>;
    if (body["token"] === undefined) {
      if (existing.token) next.token = existing.token;
    } else if (!cleared(body["token"])) {
      next.token = validateToken(body["token"]);
    }
    patch.source = next;
  } else if (existing.type === "image" && body["image"] !== undefined) {
    patch.source = { type: "image", image: validateImageRef(body["image"]) };
  }

  if (body["buildContext"] !== undefined) patch.buildContext = validateBuildContext(body["buildContext"]);
  if (body["dockerfile"] !== undefined) {
    patch.dockerfile = cleared(body["dockerfile"]) ? null : validateDockerfile(body["dockerfile"], ".");
  }
  if (body["healthPath"] !== undefined) {
    patch.healthPath = cleared(body["healthPath"]) ? null : validateHealthPath(body["healthPath"]);
  }
  if (body["buildCommand"] !== undefined) patch.buildCommand = validateCommand(body["buildCommand"], "buildCommand");
  if (body["startCommand"] !== undefined) patch.startCommand = validateCommand(body["startCommand"], "startCommand");
  if (body["volumePath"] !== undefined) patch.volumePath = validateVolume(body["volumePath"]);
  if (body["port"] !== undefined) patch.port = validatePort(body["port"]);
  if (body["memoryLimit"] !== undefined) {
    patch.memoryLimit = cleared(body["memoryLimit"]) ? null : validateMemory(body["memoryLimit"]);
  }
  if (body["cpus"] !== undefined) patch.cpus = cleared(body["cpus"]) ? null : validateCpus(body["cpus"]);
  if (body["notifyUrl"] !== undefined) {
    patch.notifyUrl = cleared(body["notifyUrl"]) ? null : validateNotifyUrl(body["notifyUrl"]);
  }
  if (body["autoDeployEnabled"] !== undefined) patch.autoDeployEnabled = Boolean(body["autoDeployEnabled"]);
  return patch;
}

/** Apply a validated patch to a stored project, keeping the display copy of the source in step. */
export function applyPatch(project: Project, patch: ProjectPatch, now: number): void {
  if (patch.source) {
    project.source = patch.source;
    project.repo = patch.source.type === "git" ? patch.source.repo : patch.source.image;
    project.branch = patch.source.type === "git" ? patch.source.branch : "";
  }
  if (patch.buildContext !== undefined) project.buildContext = patch.buildContext;
  if (patch.dockerfile === null) delete project.dockerfile;
  else if (patch.dockerfile !== undefined) project.dockerfile = patch.dockerfile;
  if (patch.healthPath === null) delete project.healthPath;
  else if (patch.healthPath !== undefined) project.healthPath = patch.healthPath;
  if (patch.buildCommand !== undefined) project.buildCommand = patch.buildCommand;
  if (patch.startCommand !== undefined) project.startCommand = patch.startCommand;
  if (patch.volumePath !== undefined) project.volumePath = patch.volumePath;
  if (patch.port !== undefined) project.port = patch.port;
  if (patch.memoryLimit === null) delete project.memoryLimit;
  else if (patch.memoryLimit !== undefined) project.memoryLimit = patch.memoryLimit;
  if (patch.cpus === null) delete project.cpus;
  else if (patch.cpus !== undefined) project.cpus = patch.cpus;
  if (patch.notifyUrl === null) delete project.notifyUrl;
  else if (patch.notifyUrl !== undefined) project.notifyUrl = patch.notifyUrl;
  if (patch.autoDeployEnabled !== undefined) project.autoDeployEnabled = patch.autoDeployEnabled;
  project.updatedAt = now;
}
````

**Create `src/lib/urls.ts`:**

````typescript
/**
 * A project can ask Deploy to POST a notification to a URL when a deploy
 * finishes. That makes this server send a request to an address a member chose,
 * which is a request-forgery risk: pointed at loopback or the LAN it would reach
 * services only this machine can see. So the URL must be https and must not name
 * such a host.
 *
 * This checks the URL as written. It cannot see through DNS (a public name that
 * resolves to a private address), which is a known limit of the check.
 */
const PRIVATE_V4: Array<[number, number]> = [
  [0x7f000000, 0xff000000], // 127.0.0.0/8
  [0x0a000000, 0xff000000], // 10.0.0.0/8
  [0xac100000, 0xfff00000], // 172.16.0.0/12
  [0xc0a80000, 0xffff0000], // 192.168.0.0/16
  [0xa9fe0000, 0xffff0000], // 169.254.0.0/16 (link-local, cloud metadata)
  [0x64400000, 0xffc00000], // 100.64.0.0/10 (carrier-grade NAT)
  [0x00000000, 0xff000000], // 0.0.0.0/8
];

function ipv4ToInt(host: string): number | null {
  const parts = host.split(".");
  if (parts.length !== 4) return null;
  let value = 0;
  for (const part of parts) {
    if (!/^\d{1,3}$/.test(part) || Number(part) > 255) return null;
    value = value * 256 + Number(part);
  }
  return value;
}

export function isPrivateHost(hostname: string): boolean {
  const host = hostname.toLowerCase().replace(/^\[|\]$/g, "");
  if (host === "localhost" || host.endsWith(".localhost") || host.endsWith(".local") || host.endsWith(".internal")) {
    return true;
  }
  const v4 = ipv4ToInt(host);
  if (v4 !== null) return PRIVATE_V4.some(([net, mask]) => (v4 & mask) >>> 0 === net);
  if (host.includes(":")) {
    // IPv6 literal: loopback, unspecified, unique-local (fc00::/7), link-local (fe80::/10),
    // and IPv4-mapped addresses, which could smuggle a private IPv4.
    return host === "::1" || host === "::" || /^f[cd]/.test(host) || /^fe[89ab]/.test(host) || host.startsWith("::ffff:");
  }
  return false;
}

export function validateNotifyUrl(value: unknown): string {
  if (typeof value !== "string" || value.length > 2048) throw new Error("notifyUrl must be an https URL");
  let url: URL;
  try {
    url = new URL(value);
  } catch {
    throw new Error("notifyUrl must be an https URL");
  }
  if (url.protocol !== "https:" || url.username || url.password) {
    throw new Error("notifyUrl must be an https URL without credentials");
  }
  if (isPrivateHost(url.hostname)) throw new Error("notifyUrl must not point at a private or local address");
  return url.toString();
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **136 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/project-input.ts src/lib/urls.ts tests/project-input.test.ts
git commit \
  -m "feat(input): validate every member-supplied project field, and guard notify URLs against request forgery" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 12: An injectable HTTP app with membership, ownership, the name registry and project routes

Splits the server into `createApp(deps)` and a thin `index.ts` so every route can be tested with fake identity, Docker and build dependencies. Adds the membership gate, per-project ownership (another member's project answers 404, exactly like a missing one), the project quota, the name registry, and member administration for operators. The existing server-level tests are adapted: their fake identity is a founder.

**Files:**

- Create: `src/app.ts`
- Create: `src/lib/wiring.ts`
- Create: `src/routes/ai.ts`
- Create: `src/routes/basic.ts`
- Create: `src/routes/context.ts`
- Create: `src/routes/projects.ts`
- Create: `src/routes/types.ts`
- Create: `tests/routes-projects.test.ts`
- Create: `tests/support/app-harness.ts`
- Modify: `src/index.ts`
- Modify: `src/lib/project-input.ts`
- Modify: `src/middleware/auth.ts`
- Modify: `tests/build-context.test.ts`
- Modify: `tests/project-input.test.ts`

**Interfaces:**

- Consumes: Tasks 1-11.
- Produces: `createApp(deps: AppDeps)`, `AppDeps`, `Pipeline`, `ProjectOps`, `RouteContext`; `makeRequireAuth`, `Authenticator`, `AuthResult`, `authenticateWithNexusAuth`, `verifyWithNexusAuth(token, authenticate?)`; `makeContext`, `ownedProject`, `accessOf`, `audit`, `paramOf`; `basicRoutes`, `aiRoutes`, `projectRoutes`; `realDeps`, `dockerOps`, `packageVersion`; test harness `startHarness`, `resetDb`, `FakePipeline`, `FakeOps`.

- [ ] **Step 1: Write the tests**

**Modify `tests/build-context.test.ts`:**

````diff
--- a/tests/build-context.test.ts
+++ b/tests/build-context.test.ts
@@ -13,7 +13,8 @@ process.env.APP_SECRET = "disposable-context-test-secret";  // pragma: allowlist
 const { loadDb, saveDb } = await import("../src/lib/store.js");
 const auth = createServer((_req, res) => {
   res.setHeader("Content-Type", "application/json");
-  res.end(JSON.stringify({ authorized: true, userId: "test-user" }));
+  // A founder, so the first request seeds the owner (members are otherwise invite-only).
+  res.end(JSON.stringify({ authorized: true, userId: "test-user", user: { id: "test-user", role: "founder" } }));
 });
 let child: ReturnType<typeof spawn>;
 let base: string;
````

**Modify `tests/project-input.test.ts`:**

````diff
--- a/tests/project-input.test.ts
+++ b/tests/project-input.test.ts
@@ -136,3 +136,10 @@ test("applying a patch updates the project and keeps the display copy of the sou
   assert.equal("cpus" in project, false);
   assert.equal(project.updatedAt, 99);
 });
+
+test("an explicit empty or null build context is refused, while a missing one defaults to the root", () => {
+  assert.equal(parseNewProject({ name: "x", repo: "o/r" }, false).buildContext, ".");
+  for (const bad of [null, "", " ", 42, {}, []]) {
+    assert.throws(() => parseNewProject({ name: "x", repo: "o/r", buildContext: bad }, false), /buildContext/, String(bad));
+  }
+});
````

**Create `tests/routes-projects.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { after, afterEach, before, beforeEach, test } from "node:test";
import { DEFAULT_LIMITS } from "../src/lib/safeguards.js";
import { resetDb, startHarness, store } from "./support/app-harness.js";

let h: Awaited<ReturnType<typeof startHarness>>;
before(async () => {
  h = await startHarness();
});
after(async () => {
  await h.close();
});
beforeEach(() => resetDb());
afterEach(() => {
  h.deps.takenElsewhere = async () => false;
});

/** The founder arrives first, becoming owner, then lets Ada and Bob in. */
async function seedMembers() {
  await h.call("GET", "/api/projects", { token: "t-boss" });
  await h.call("POST", "/api/admin/members", { token: "t-boss", body: { authUserId: "ada", label: "Ada" } });
  await h.call("POST", "/api/admin/members", { token: "t-boss", body: { authUserId: "bob" } });
}

async function create(token: string, body: object) {
  return h.call("POST", "/api/projects", { token, body });
}

test("health is public and says nothing about anyone", async () => {
  const r = await h.call("GET", "/api/health");
  assert.equal(r.status, 200);
  assert.deepEqual(Object.keys(r.body).sort(), ["name", "ok", "time", "version"]);
});

test("no credentials is a 401 for an API client and a trip to sign-in for a browser", async () => {
  assert.equal((await h.call("GET", "/api/projects")).status, 401);
  const nav = await fetch(`${h.url}/api/projects`, { headers: { Accept: "text/html" }, redirect: "manual" });
  assert.equal(nav.status, 302);
  assert.match(nav.headers.get("location") ?? "", /\/login\?redirect=/);
});

test("a bad token is refused, and an unreachable identity service is a 503, not a 401", async () => {
  assert.equal((await h.call("GET", "/api/projects", { token: "nope" })).status, 401);
  const down = await h.call("GET", "/api/projects", { token: "t-down" });
  assert.equal(down.status, 503);
});

test("an account nobody let in cannot use Deploy, and is told who to ask", async () => {
  const r = await h.call("GET", "/api/projects", { token: "t-ada" });
  assert.equal(r.status, 403);
  assert.equal(r.body.error, "not_a_member");
  assert.match(r.body.hint, /operator/);
  const me = await h.call("GET", "/api/me", { token: "t-ada" });
  assert.equal(me.status, 200);
  assert.deepEqual(me.body.access, { member: false, reason: "not_a_member" });
});

test("the first founder to arrive becomes the owner and adopts projects from before ownership", async () => {
  store.saveDb({
    users: [],
    projects: [{ id: "prj_old", name: "legacy", repo: "o/r", branch: "main" } as never],
    deployments: [],
    sessions: [],
    members: [],
    audit: [],
  });
  assert.equal((await h.call("GET", "/api/projects", { token: "t-boss" })).status, 200);
  const db = store.loadDb();
  assert.deepEqual(db.members.map((m) => [m.authUserId, m.role]), [["boss", "owner"]]);
  assert.equal(db.projects[0]?.ownerId, "boss");
  await h.call("GET", "/api/projects", { token: "t-boss" });
  assert.equal(store.loadDb().members.length, 1, "arriving again seeds nothing");
});

test("an admin can use Deploy as an operator but cannot claim an empty node", async () => {
  const r = await h.call("GET", "/api/projects", { token: "t-admin" });
  assert.equal(r.status, 200);
  assert.equal(store.loadDb().members.length, 0);
});

test("an operator lets an account in, and only then can it create projects", async () => {
  await seedMembers();
  const r = await create("t-ada", { name: "Ada Blog", repo: "ada/blog" });
  assert.equal(r.status, 201);
  assert.equal(r.body.project.name, "ada-blog");
  assert.equal(r.body.project.ownerId, "ada");
  assert.equal(r.body.project.status, "idle");
  assert.ok(r.body.project.webhookSecret);
});

test("only operators manage members", async () => {
  await seedMembers();
  assert.equal((await h.call("GET", "/api/admin/members", { token: "t-ada" })).status, 403);
  assert.equal((await h.call("POST", "/api/admin/members", { token: "t-ada", body: { authUserId: "cy" } })).status, 403);
  assert.equal((await h.call("GET", "/api/admin/members", { token: "t-boss" })).body.members.length, 3);
});

test("member input is validated: ids and labels", async () => {
  await seedMembers();
  for (const body of [{}, { authUserId: "" }, { authUserId: "a b" }, { authUserId: "x".repeat(129) }, { authUserId: "ok", label: 5 }]) {
    assert.equal((await h.call("POST", "/api/admin/members", { token: "t-boss", body })).status, 400, JSON.stringify(body));
  }
});

test("a member sees and changes only their own projects; to anyone else they do not exist", async () => {
  await seedMembers();
  const mine = (await create("t-ada", { name: "mine", repo: "ada/mine" })).body.project;
  const theirs = (await create("t-bob", { name: "theirs", repo: "bob/theirs" })).body.project;

  const list = (await h.call("GET", "/api/projects", { token: "t-ada" })).body.projects;
  assert.deepEqual(list.map((p: { id: string }) => p.id), [mine.id]);

  for (const [method, route, body] of [
    ["GET", `/api/projects/${theirs.id}`, undefined],
    ["PUT", `/api/projects/${theirs.id}`, { branch: "x" }],
    ["PUT", `/api/projects/${theirs.id}/env`, { env: { A: "1" } }],
    ["POST", `/api/projects/${theirs.id}/regen-webhook-secret`, undefined],
  ] as const) {
    const r = await h.call(method, route, { token: "t-ada", ...(body ? { body } : {}) });
    assert.equal(r.status, 404, `${method} ${route}`);
  }
  assert.equal(store.loadDb().projects.find((p) => p.id === theirs.id)?.branch, "main", "nothing changed");
});

test("operators see every project", async () => {
  await seedMembers();
  await create("t-ada", { name: "one", repo: "ada/one" });
  await create("t-bob", { name: "two", repo: "bob/two" });
  const list = (await h.call("GET", "/api/projects", { token: "t-boss" })).body.projects;
  assert.equal(list.length, 2);
  const id = list[0].id;
  assert.equal((await h.call("GET", `/api/projects/${id}`, { token: "t-admin" })).status, 200);
});

test("a suspended member is shut out, and an owner cannot be suspended", async () => {
  await seedMembers();
  const suspend = await h.call("PATCH", "/api/admin/members/ada", { token: "t-boss", body: { status: "suspended" } });
  assert.equal(suspend.status, 200);
  const r = await h.call("GET", "/api/projects", { token: "t-ada" });
  assert.equal(r.status, 403);
  assert.equal(r.body.error, "suspended");
  await h.call("PATCH", "/api/admin/members/ada", { token: "t-boss", body: { status: "active" } });
  assert.equal((await h.call("GET", "/api/projects", { token: "t-ada" })).status, 200);

  assert.equal((await h.call("PATCH", "/api/admin/members/boss", { token: "t-admin", body: { status: "suspended" } })).status, 409);
  assert.equal((await h.call("PATCH", "/api/admin/members/ghost", { token: "t-boss", body: { status: "active" } })).status, 404);
  assert.equal((await h.call("PATCH", "/api/admin/members/ada", { token: "t-boss", body: { status: "banned" } })).status, 400);
});

test("names: reserved, malformed, duplicate and unverifiable ones are refused with the right status", async () => {
  await seedMembers();
  assert.equal((await create("t-ada", { name: "deploy", repo: "o/r" })).status, 400);
  assert.equal((await create("t-ada", { name: "??", repo: "o/r" })).status, 400);
  assert.equal((await create("t-ada", { name: "web", repo: "o/r" })).status, 201);
  const dup = await create("t-bob", { name: "web", repo: "o/r" });
  assert.equal(dup.status, 409);
  assert.equal(dup.body.code, "taken");

  h.deps.takenElsewhere = async (slug) => slug === "hostedsite";
  assert.equal((await create("t-ada", { name: "hostedsite", repo: "o/r" })).status, 409);

  h.deps.takenElsewhere = async () => {
    throw new Error("hosting is unreachable");
  };
  const unverified = await create("t-ada", { name: "anything", repo: "o/r" });
  assert.equal(unverified.status, 503);
  assert.equal(unverified.body.code, "unverified");
});

test("the project quota stops the sixth project, not the fifth", async () => {
  await seedMembers();
  for (let i = 1; i <= DEFAULT_LIMITS.maxProjects; i++) {
    assert.equal((await create("t-ada", { name: `app${i}x`, repo: "o/r" })).status, 201, `project ${i}`);
  }
  const sixth = await create("t-ada", { name: "onetoomany", repo: "o/r" });
  assert.equal(sixth.status, 403);
  assert.equal(sixth.body.code, "quota");
  assert.equal((await create("t-bob", { name: "bobsfirst", repo: "o/r" })).status, 201, "another member is unaffected");
});

test("bad input is a 400 that names the problem and stores nothing", async () => {
  await seedMembers();
  const before = store.loadDb().projects.length;
  for (const [body, pattern] of [
    [{ name: "x1x", source: { type: "image", image: "nginx; rm -rf /" } }, /image must look like/],
    [{ name: "x1x", repo: "o/r", port: 99999 }, /port/],
    [{ name: "x1x", repo: "file:///etc" }, /not allowed/],
    [{ name: "x1x", repo: "o/r", branch: "--upload-pack=sh" }, /branch/],
    [{ name: "x1x", repo: "o/r", volumePath: "/a:/b" }, /volumePath/],
    [{ repo: "o/r" }, /Missing name/],
  ] as const) {
    const r = await create("t-ada", body);
    assert.equal(r.status, 400, JSON.stringify(body));
    assert.match(r.body.error, pattern);
  }
  assert.equal(store.loadDb().projects.length, before);
});

test("malformed JSON is a 400, not a stack trace", async () => {
  const r = await h.call("POST", "/api/projects", {
    token: "t-boss",
    rawBody: "{not json",
    headers: { "Content-Type": "application/json" },
  });
  assert.equal(r.status, 400);
  assert.equal(r.body.error, "Request body is not valid JSON");
});

test("an image project is created from an image reference", async () => {
  await seedMembers();
  const r = await create("t-ada", { name: "webimage", source: { type: "image", image: "nginx:1.27" }, port: 80 });
  assert.equal(r.status, 201);
  assert.deepEqual(r.body.project.source, { type: "image", image: "nginx:1.27" });
  assert.equal(r.body.project.repo, "nginx:1.27");
});

test("a private-repository token is accepted, stored, and never shown again", async () => {
  await seedMembers();
  const token = "ghp_neverShowThis123";
  const created = await create("t-ada", { name: "private", source: { type: "git", repo: "ada/private", token } });
  assert.equal(created.status, 201);
  assert.ok(!JSON.stringify(created.body).includes(token));
  assert.equal(created.body.project.source.hasToken, true);
  const id = created.body.project.id;

  for (const [method, route] of [["GET", `/api/projects/${id}`], ["GET", "/api/projects"]] as const) {
    const r = await h.call(method, route, { token: "t-ada" });
    assert.ok(!JSON.stringify(r.body).includes(token), `${method} ${route}`);
  }
  const updated = await h.call("PUT", `/api/projects/${id}`, { token: "t-ada", body: { branch: "dev" } });
  assert.ok(!JSON.stringify(updated.body).includes(token));
  const stored = store.loadDb().projects[0]?.source as { token?: string } | undefined;
  assert.equal(stored?.token, token, "still stored, because cloning needs it");

  const cleared = await h.call("PUT", `/api/projects/${id}`, { token: "t-ada", body: { token: null } });
  assert.equal(cleared.body.project.source.hasToken, false);
});

test("environment variables are validated and stored for the owner only", async () => {
  await seedMembers();
  const id = (await create("t-ada", { name: "envapp", repo: "o/r" })).body.project.id;
  assert.equal((await h.call("PUT", `/api/projects/${id}/env`, { token: "t-ada", body: { env: { A: "1" } } })).status, 200);
  assert.deepEqual(store.loadDb().projects[0]?.env, { A: "1" });
  assert.equal((await h.call("PUT", `/api/projects/${id}/env`, { token: "t-ada", body: { env: { "bad name": "1" } } })).status, 400);
  assert.equal((await h.call("PUT", `/api/projects/${id}/env`, { token: "t-ada", body: { env: { A: "x\ny" } } })).status, 400);
  assert.equal((await h.call("PUT", `/api/projects/${id}/env`, { token: "t-bob", body: { env: { A: "1" } } })).status, 404);
});

test("regenerating the webhook secret changes it for the owner only", async () => {
  await seedMembers();
  const created = (await create("t-ada", { name: "hooked", repo: "o/r" })).body.project;
  const r = await h.call("POST", `/api/projects/${created.id}/regen-webhook-secret`, { token: "t-ada" });
  assert.equal(r.status, 200);
  assert.notEqual(r.body.webhookSecret, created.webhookSecret);
  assert.equal(store.loadDb().projects[0]?.webhookSecret, r.body.webhookSecret);
});

test("the node's safeguards are visible to members", async () => {
  await seedMembers();
  const r = await h.call("GET", "/api/limits", { token: "t-ada" });
  assert.equal(r.status, 200);
  assert.equal(r.body.limits.maxProjects, DEFAULT_LIMITS.maxProjects);
  assert.equal(r.body.availability, "best-effort");
  assert.equal(r.body.controlHost, "deploy.example.test");
});

test("who did what is recorded for operators, and carries no address", async () => {
  await seedMembers();
  await create("t-ada", { name: "audited", repo: "o/r" });
  assert.equal((await h.call("GET", "/api/admin/audit", { token: "t-ada" })).status, 403);
  const audit = (await h.call("GET", "/api/admin/audit", { token: "t-boss" })).body.audit as Array<Record<string, unknown>>;
  assert.deepEqual(audit.map((e) => e["action"]).sort(), ["member.grant", "member.grant", "member.grant", "project.create"]);
  for (const entry of audit) assert.deepEqual(Object.keys(entry).sort().filter((k) => !["subject"].includes(k)), ["action", "actor", "at", "id"]);
});

test("an unknown API path is a JSON 404", async () => {
  const r = await h.call("GET", "/api/nothing-here", { token: "t-boss" });
  assert.equal(r.status, 404);
  assert.equal(r.body.error, "Not found");
});
````

**Create `tests/support/app-harness.ts`:**

````typescript
/**
 * Starts the real HTTP app on an ephemeral port with every outside dependency
 * faked: identity, Docker, the build pipeline, the ingress. Importing this file
 * points the data directory at a throwaway folder first, so it must be imported
 * before anything that reads the config (node --test gives each test file its
 * own process, so nothing leaks between files).
 */
import { once } from "node:events";
import fs from "node:fs";
import type { AddressInfo } from "node:net";
import os from "node:os";
import path from "node:path";
import type { Server } from "node:http";
import { DEFAULT_LIMITS, type Verdict } from "../../src/lib/safeguards.js";
import type { Authenticator, NexusIdentity } from "../../src/middleware/auth.js";
import type { AppDeps, Pipeline, ProjectOps } from "../../src/routes/types.js";
import type { Deployment, DeployTrigger, Project } from "../../src/types.js";
import { RecordingPublisher } from "./fakes.js";

const dataDir = fs.mkdtempSync(path.join(os.tmpdir(), "deploy-routes-"));
process.env["DATA_DIR"] = dataDir;
process.env["APP_SECRET"] = "test-secret-test-secret-1234";
process.env["BASE_DOMAIN"] = "example.test";
process.env["SANDBOX_DOCKER_HOST"] = "unix:///does-not-exist.sock";

export const store = await import("../../src/lib/store.js");
const { createApp } = await import("../../src/app.js");

export const IDENTITIES: Record<string, NexusIdentity> = {
  "t-boss": { id: "boss", role: "founder", username: "boss" },
  "t-admin": { id: "adm", role: "admin", username: "adm" },
  "t-ada": { id: "ada", role: "user", username: "ada" },
  "t-bob": { id: "bob", role: "user", username: "bob" },
  "t-cy": { id: "cy", role: "user", username: "cy" },
};

export const fakeAuth: Authenticator = async (token) => {
  if (token === "t-down") return { kind: "unavailable" };
  const identity = IDENTITIES[token];
  return identity ? { kind: "ok", identity } : { kind: "denied" };
};

export function resetDb(): void {
  store.saveDb({ users: [], projects: [], deployments: [], sessions: [], members: [], audit: [] });
}

export class FakePipeline implements Pipeline {
  readonly deploys: Array<{ projectId: string; deploymentId: string }> = [];
  readonly rollbacks: Array<{ projectId: string; deploymentId: string; imageRef: string }> = [];
  readonly cancelled: string[] = [];
  verdict: Verdict = { ok: true };
  building = new Set<string>();
  cancellable = true;

  createDeployment(project: Project, commitSha: string, trigger: DeployTrigger): Deployment {
    const dep: Deployment = {
      id: `dep_${store.loadDb().deployments.length + 1}`.padEnd(20, "0"),
      projectId: project.id,
      commitSha,
      triggeredBy: trigger,
      status: "queued",
      imageTag: "",
      logs: [`[nexus] Deploy queued → ${project.name}`],
      createdAt: Date.now(),
    };
    const db = store.loadDb();
    db.deployments.unshift(dep);
    store.saveDb(db);
    return dep;
  }
  async runDeploy(project: Project, deploymentId: string): Promise<void> {
    this.deploys.push({ projectId: project.id, deploymentId });
  }
  async runRollback(project: Project, deploymentId: string, imageRef: string): Promise<void> {
    this.rollbacks.push({ projectId: project.id, deploymentId, imageRef });
  }
  isBuilding(projectId: string): boolean {
    return this.building.has(projectId);
  }
  async cancelBuild(deploymentId: string): Promise<boolean> {
    this.cancelled.push(deploymentId);
    return this.cancellable;
  }
  guardBuild(): Verdict {
    return this.verdict;
  }
}

export class FakeOps implements ProjectOps {
  readonly calls: string[] = [];
  statusValue = "running";
  logLines: string[] = [];
  async stop(name: string) {
    this.calls.push(`stop:${name}`);
  }
  async start(name: string) {
    this.calls.push(`start:${name}`);
  }
  async restart(name: string) {
    this.calls.push(`restart:${name}`);
  }
  async remove(name: string) {
    this.calls.push(`remove:${name}`);
  }
  async status(name: string) {
    this.calls.push(`status:${name}`);
    return this.statusValue;
  }
  async stats(name: string) {
    this.calls.push(`stats:${name}`);
    return { cpu: "1%", memUsage: "10MiB", memLimit: "512MiB", memPercent: "2%", netIn: "1kB", netOut: "1kB", pids: "3" };
  }
  logs(name: string, onLine: (line: string) => void, onEnd?: (err?: Error) => void): () => void {
    this.calls.push(`logs:${name}`);
    for (const line of this.logLines) onLine(line);
    onEnd?.();
    return () => {};
  }
  async removeVolume(name: string) {
    this.calls.push(`removeVolume:${name}`);
  }
  async removeImage(ref: string) {
    this.calls.push(`removeImage:${ref}`);
    return true;
  }
}

export type CallOptions = { token?: string; body?: unknown; rawBody?: string; headers?: Record<string, string> };
export type CallResult = { status: number; body: any; headers: Headers };

export async function startHarness(patch: Partial<AppDeps> = {}) {
  const pipeline = new FakePipeline();
  const ops = new FakeOps();
  const publisher = new RecordingPublisher();
  let clock = 1_800_000_000_000;
  const deps: AppDeps = {
    authenticate: fakeAuth,
    pipeline,
    ops,
    publisher,
    takenElsewhere: async () => false,
    limits: DEFAULT_LIMITS,
    allowLocalRepos: false,
    now: () => clock,
    node: { baseDomain: "example.test", controlHost: "deploy.example.test", hostPattern: "{slug}.{BASE_DOMAIN}" },
    version: "test",
    staticDir: null,
    ...patch,
  };
  const server: Server = createApp(deps).listen(0, "127.0.0.1");
  await once(server, "listening");
  const url = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;

  async function call(method: string, route: string, options: CallOptions = {}): Promise<CallResult> {
    const headers: Record<string, string> = { ...(options.headers ?? {}) };
    if (options.token) headers["Authorization"] = `Bearer ${options.token}`;
    let body: string | undefined;
    if (options.rawBody !== undefined) body = options.rawBody;
    else if (options.body !== undefined) {
      body = JSON.stringify(options.body);
      headers["Content-Type"] ??= "application/json";
    }
    const response = await fetch(`${url}${route}`, { method, headers, ...(body !== undefined ? { body } : {}) });
    const text = await response.text();
    let parsed: any = text;
    try {
      parsed = text ? JSON.parse(text) : null;
    } catch {
      /* not JSON: leave as text */
    }
    return { status: response.status, body: parsed, headers: response.headers };
  }

  return {
    url,
    call,
    deps,
    pipeline,
    ops,
    publisher,
    advance: (ms: number) => {
      clock += ms;
    },
    close: () => new Promise<void>((resolve) => server.close(() => resolve())),
  };
}
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/build-context.test.ts tests/project-input.test.ts tests/routes-projects.test.ts
```

Expected: FAIL — `Cannot find module './support/app-harness.js'` for `routes-projects.test.ts`; after the harness exists, every route test fails until the app is written.

- [ ] **Step 3: Implement**

**Create `src/app.ts`:**

````typescript
import path from "node:path";
import cors from "cors";
import express, { type ErrorRequestHandler } from "express";
import helmet from "helmet";
import { aiRoutes } from "./routes/ai.js";
import { basicRoutes } from "./routes/basic.js";
import { makeContext } from "./routes/context.js";
import { projectRoutes } from "./routes/projects.js";
import type { AppDeps } from "./routes/types.js";

export type { AppDeps } from "./routes/types.js";

/**
 * The HTTP application, built from injected dependencies so that every route can
 * be exercised in tests without Nexus-Auth, Docker or Git. src/index.ts wires in
 * the real ones and starts listening.
 */
export function createApp(deps: AppDeps): express.Express {
  const app = express();
  app.use(helmet({ contentSecurityPolicy: false, frameguard: false }));
  app.use(cors());

  const ctx = makeContext(deps);

  app.use(express.json({ limit: "1mb" }));

  const api = express.Router();
  basicRoutes(api, ctx);
  aiRoutes(api, ctx);
  projectRoutes(api, ctx);
  app.use(api);

  // An unknown API path is a JSON 404, never the dashboard's HTML.
  app.use("/api", (_req, res) => {
    res.status(404).json({ error: "Not found" });
  });

  if (deps.staticDir) {
    const dir = path.resolve(deps.staticDir);
    app.use(express.static(dir));
    app.use((_req, res) => res.sendFile(path.join(dir, "index.html")));
  }

  const onError: ErrorRequestHandler = (err, _req, res, _next) => {
    // A malformed JSON body is the caller's mistake; anything else is ours and
    // must not leak a stack trace.
    if (err && typeof err === "object" && "type" in err && err.type === "entity.parse.failed") {
      res.status(400).json({ error: "Request body is not valid JSON" });
      return;
    }
    console.error("[nexus] request error:", err instanceof Error ? err.message : err);
    res.status(500).json({ error: "Internal error" });
  };
  app.use(onError);

  return app;
}
````

**Replace `src/index.ts` entirely with:**

````typescript
import { createServer } from "node:http";
import { createApp } from "./app.js";
import { config, ensureDataDir } from "./lib/config.js";
import { startStatusSync } from "./lib/status-sync.js";
import { realDeps } from "./lib/wiring.js";

ensureDataDir();
const deps = realDeps();
const server = createServer(createApp(deps));

startStatusSync(30_000);

server.listen(config.port, () => {
  console.log(`
╔════════════════════════════════════════╗
║        NEXUS DEPLOY  v${deps.version.padEnd(16)}║
║   The No Hands Company · Free forever ║
╠════════════════════════════════════════╣
║  ${config.baseUrl.padEnd(38)}║
║  *.${config.baseDomain.padEnd(36)}║
╚════════════════════════════════════════╝
  `);
});
````

**Modify `src/lib/project-input.ts`:**

````diff
--- a/src/lib/project-input.ts
+++ b/src/lib/project-input.ts
@@ -107,7 +107,9 @@ export function parseNewProject(raw: unknown, allowLocalRepos: boolean): NewProj
   return {
     name: body["name"],
     source,
-    buildContext: source.type === "git" ? validateBuildContext(body["buildContext"] ?? ".") : ".",
+    // Only a missing value defaults; an explicit null or empty one is a mistake to refuse.
+    buildContext:
+      source.type === "git" && body["buildContext"] !== undefined ? validateBuildContext(body["buildContext"]) : ".",
     ...(source.type === "git" && present(body["dockerfile"])
       ? { dockerfile: validateDockerfile(body["dockerfile"], ".") }
       : {}),
````

**Create `src/lib/wiring.ts`:**

````typescript
/**
 * The real dependencies: Nexus-Auth for identity, the sandbox daemon for
 * containers, the build pipeline for deployments. Tests build an AppDeps of
 * their own instead of calling this.
 */
import fs from "node:fs";
import { authenticateWithNexusAuth } from "../middleware/auth.js";
import type { AppDeps, ProjectOps } from "../routes/types.js";
import {
  cancelBuild,
  createDeployment,
  getRoutePublisher,
  guardBuild,
  isBuilding,
  runDeploy,
  runRollback,
} from "./build.js";
import { config } from "./config.js";
import {
  dockerLogs,
  dockerRemove,
  dockerRemoveImage,
  dockerRemoveVolume,
  dockerRestart,
  dockerStart,
  dockerStats,
  dockerStatus,
  dockerStop,
} from "./docker.js";

export const dockerOps: ProjectOps = {
  stop: dockerStop,
  start: dockerStart,
  restart: dockerRestart,
  remove: dockerRemove,
  status: dockerStatus,
  stats: dockerStats,
  logs: dockerLogs,
  removeVolume: dockerRemoveVolume,
  removeImage: dockerRemoveImage,
};

export function packageVersion(): string {
  try {
    const raw = fs.readFileSync(new URL("../../package.json", import.meta.url), "utf8");
    return (JSON.parse(raw) as { version?: string }).version ?? "unknown";
  } catch {
    return "unknown";
  }
}

export function realDeps(): AppDeps {
  return {
    authenticate: authenticateWithNexusAuth,
    pipeline: {
      createDeployment,
      runDeploy: (project, deploymentId) => runDeploy(project, deploymentId),
      runRollback: (project, deploymentId, imageRef) => runRollback(project, deploymentId, imageRef),
      isBuilding,
      cancelBuild,
      guardBuild: (project) => guardBuild(project),
    },
    ops: dockerOps,
    publisher: getRoutePublisher(),
    // Until the edge is wired in, only Deploy's own projects are checked for a taken name.
    takenElsewhere: async () => false,
    limits: config.limits,
    allowLocalRepos: config.allowLocalRepos,
    now: Date.now,
    node: { baseDomain: config.baseDomain, controlHost: config.controlHost, hostPattern: config.appHostPattern },
    version: packageVersion(),
    staticDir: "web/dist",
  };
}
````

**Replace `src/middleware/auth.ts` entirely with:**

````typescript
import type { NextFunction, Request, Response } from "express";
import { config } from "../lib/config.js";
import type { GrantedAccess } from "../lib/members.js";

export type NexusIdentity = {
  id: string;
  username?: string;
  email?: string;
  /** "founder" | "admin" | "user" … as Nexus-Auth reports it. */
  role?: string;
  [key: string]: unknown;
};

export type AuthedRequest = Request & { userId?: string; user?: NexusIdentity; access?: GrantedAccess };

/** What asking the identity service about a credential can come to. */
export type AuthResult =
  | { kind: "ok"; identity: NexusIdentity }
  | { kind: "denied" }
  | { kind: "unavailable" };

/** A function from a raw credential to who it belongs to. Injected so tests need no Nexus-Auth. */
export type Authenticator = (token: string) => Promise<AuthResult>;

const SESSION_COOKIE = "nexus_session";

/**
 * Read the session cookie without pulling in a cookie-parser dependency.
 * Browsers cannot set an Authorization header on a plain navigation, so this is
 * the credential that actually arrives when a user clicks through from another
 * app in the ecosystem.
 */
function readSessionCookie(req: Request): string | undefined {
  const header = req.headers.cookie;
  if (!header) return undefined;
  for (const part of header.split(";")) {
    const eq = part.indexOf("=");
    if (eq === -1) continue;
    if (part.slice(0, eq).trim() === SESSION_COOKIE) {
      return decodeURIComponent(part.slice(eq + 1).trim());
    }
  }
  return undefined;
}

/**
 * Authenticate against Nexus-Auth — the ecosystem's identity service — rather
 * than against a user table owned by this app.
 *
 * Nexus-Auth exposes GET /api/v1/auth/check → 401, or 200 with { userId, user }.
 * A session token is opaque, not a JWT, so it cannot be verified locally; this
 * asks the issuer, once per authenticated call.
 */
export const authenticateWithNexusAuth: Authenticator = async (token) => {
  const url = `${config.nexusAuthUrl.replace(/\/+$/, "")}/api/v1/auth/check`;
  let response: globalThis.Response;
  try {
    response = await fetch(url, {
      headers: { Authorization: `Bearer ${token}`, Accept: "application/json" },
      signal: AbortSignal.timeout(config.nexusAuthTimeoutMs),
    });
  } catch {
    // Unreachable is not "wrong credentials": telling a signed-in person to sign
    // in again because the identity service is down would be a lie.
    return { kind: "unavailable" };
  }
  if (response.status >= 500) return { kind: "unavailable" };
  if (!response.ok) return { kind: "denied" };
  const body = (await response.json().catch(() => null)) as
    | { authorized?: boolean; userId?: string; user?: NexusIdentity }
    | null;
  if (!body?.authorized || !body.userId) return { kind: "denied" };
  return { kind: "ok", identity: { ...(body.user ?? {}), id: body.userId } };
};

/** Identity for the WebSocket upgrade paths, which never pass through Express middleware. */
export async function verifyWithNexusAuth(
  token: string,
  authenticate: Authenticator = authenticateWithNexusAuth,
): Promise<NexusIdentity | null> {
  const result = await authenticate(token);
  return result.kind === "ok" ? result.identity : null;
}

/**
 * Send a browser to the ecosystem sign-in page instead of answering 401.
 * API clients still get the 401 they can act on; the two are told apart by
 * whether the request looks like a navigation (a GET accepting text/html).
 * Nexus-Auth refuses any redirect target outside the parent domain, so this
 * cannot become an open redirect.
 */
function wantsHtml(req: Request): boolean {
  return req.method === "GET" && (req.headers.accept ?? "").includes("text/html");
}

function loginUrlFor(req: Request): string {
  const proto = (req.headers["x-forwarded-proto"] as string | undefined) ?? req.protocol;
  const host = (req.headers["x-forwarded-host"] as string | undefined) ?? req.get("host") ?? "";
  const here = `${proto}://${host}${req.originalUrl}`;
  const base = config.nexusAuthPublicUrl.replace(/\/+$/, "");
  return `${base}/login?redirect=${encodeURIComponent(here)}`;
}

function denyUnauthenticated(req: Request, res: Response): void {
  if (wantsHtml(req)) {
    res.redirect(302, loginUrlFor(req));
    return;
  }
  res.status(401).json({ error: "Unauthorized" });
}

/**
 * Credentials are accepted as `Authorization: Bearer`, the shared session
 * cookie, or `?token=` (for SSE, which cannot set headers).
 */
export function makeRequireAuth(authenticate: Authenticator) {
  return async (req: AuthedRequest, res: Response, next: NextFunction): Promise<void> => {
    const header = req.headers.authorization;
    const bearer = header?.startsWith("Bearer ") ? header.slice(7) : undefined;
    const raw = bearer ?? readSessionCookie(req) ?? (req.query["token"] as string | undefined);
    if (!raw) {
      denyUnauthenticated(req, res);
      return;
    }
    const result = await authenticate(raw);
    if (result.kind === "unavailable") {
      res.status(503).json({
        error: "Identity service unavailable",
        hint: "Nexus-Auth could not be reached. Try again shortly.",
      });
      return;
    }
    if (result.kind === "denied") {
      denyUnauthenticated(req, res);
      return;
    }
    req.userId = result.identity.id;
    req.user = result.identity;
    next();
  };
}
````

**Create `src/routes/ai.ts`:**

````typescript
import type { Router } from "express";
import { describeNexusAiIntegration, requestDeployAssistantSuggestion } from "../lib/ai.js";
import type { RouteContext } from "./types.js";

/** The optional Nexus AI deploy assistant. Unchanged in behaviour; now behind the member gate. */
export function aiRoutes(router: Router, ctx: RouteContext): void {
  router.get("/api/ai/integration", ...ctx.member, (_req, res) => {
    res.json({ integration: describeNexusAiIntegration() });
  });

  router.post("/api/ai/deploy-assist", ...ctx.member, async (req, res) => {
    const prompt = String(req.body?.prompt ?? "").trim();
    if (!prompt) return void res.status(400).json({ error: "Missing prompt" });

    try {
      const suggestion = await requestDeployAssistantSuggestion({
        prompt,
        model: typeof req.body?.model === "string" ? req.body.model : undefined,
        temperature: typeof req.body?.temperature === "number" ? req.body.temperature : undefined,
        context: {
          projectId: req.body?.projectId,
          repo: req.body?.repo,
          branch: req.body?.branch,
          buildCommand: req.body?.buildCommand,
          startCommand: req.body?.startCommand,
        },
      });
      res.json(suggestion);
    } catch (error) {
      const message = (error as Error).message;
      res.status(message.includes("not configured") ? 503 : 502).json({ error: message });
    }
  });
}
````

**Create `src/routes/basic.ts`:**

````typescript
import type { Request, Response, Router } from "express";
import { grantMember, resolveAccess, setMemberStatus } from "../lib/members.js";
import { loadDb, writeDb } from "../lib/store.js";
import type { AuthedRequest } from "../middleware/auth.js";
import type { MemberStatus } from "../types.js";
import { audit, paramOf } from "./context.js";
import type { RouteContext } from "./types.js";

const ID_RE = /^[A-Za-z0-9_.:@-]{1,128}$/;

export function basicRoutes(router: Router, ctx: RouteContext): void {
  const { deps } = ctx;

  router.get("/api/health", (_req, res) => {
    res.json({ ok: true, name: "nexus-deploy", version: deps.version, time: new Date(deps.now()).toISOString() });
  });

  // Who am I, and am I let in? Answered even for an account that is not a member,
  // so the dashboard can show "ask an operator" instead of a bare error.
  router.get("/api/me", ctx.auth, (req: Request, res: Response) => {
    const identity = (req as AuthedRequest).user;
    if (!identity) return void res.status(401).json({ error: "Unauthorized" });
    const access = resolveAccess(loadDb().members, identity);
    res.json({
      user: identity,
      access: access.ok ? { member: true, operator: access.operator } : { member: false, reason: access.reason },
    });
  });

  // What this node's safeguards are, shown so nobody is surprised by them.
  router.get("/api/limits", ...ctx.member, (_req, res) => {
    res.json({
      limits: deps.limits,
      baseDomain: deps.node.baseDomain,
      controlHost: deps.node.controlHost,
      appHostPattern: deps.node.hostPattern,
      availability: "best-effort",
    });
  });

  // ── Members (operators only) ─────────────────────────────────────────────
  router.get("/api/admin/members", ...ctx.operator, (_req, res) => {
    res.json({ members: loadDb().members });
  });

  router.post("/api/admin/members", ...ctx.operator, async (req, res) => {
    const body = req.body as { authUserId?: unknown; label?: unknown } | null;
    if (typeof body?.authUserId !== "string" || !ID_RE.test(body.authUserId)) {
      return void res.status(400).json({ error: "authUserId must be the account's Nexus-Auth id" });
    }
    if (body.label !== undefined && (typeof body.label !== "string" || body.label.length > 80)) {
      return void res.status(400).json({ error: "label must be text of at most 80 characters" });
    }
    const actor = (req as AuthedRequest).user;
    if (!actor) return void res.status(401).json({ error: "Unauthorized" });
    let granted = null as ReturnType<typeof grantMember> | null;
    const target: { authUserId: string; label?: string } = { authUserId: body.authUserId };
    if (typeof body.label === "string") target.label = body.label;
    await writeDb((db) => {
      granted = grantMember(db.members, actor, target, deps.now());
    });
    await audit(deps, req, "member.grant", body.authUserId);
    res.status(201).json({ member: granted });
  });

  router.patch("/api/admin/members/:authUserId", ...ctx.operator, async (req, res) => {
    const status = (req.body as { status?: unknown } | null)?.status;
    if (status !== "active" && status !== "suspended") {
      return void res.status(400).json({ error: 'status must be "active" or "suspended"' });
    }
    const authUserId = paramOf(req, "authUserId");
    let result: ReturnType<typeof setMemberStatus> = "not_found";
    await writeDb((db) => {
      result = setMemberStatus(db.members, authUserId, status as MemberStatus);
    });
    if (result === "not_found") return void res.status(404).json({ error: "Not found" });
    if (result === "is_owner") return void res.status(409).json({ error: "An owner cannot be suspended" });
    await audit(deps, req, status === "suspended" ? "member.suspend" : "member.reactivate", authUserId);
    res.json({ ok: true });
  });

  router.get("/api/admin/audit", ...ctx.operator, (_req, res) => {
    res.json({ audit: loadDb().audit.slice(0, 200) });
  });
}
````

**Create `src/routes/context.ts`:**

````typescript
import type { NextFunction, Request, RequestHandler, Response } from "express";
import { type AuditAction, recordAudit } from "../lib/audit.js";
import { adoptOrphanProjects, bootstrapOwner, canAccessProject, type GrantedAccess, resolveAccess } from "../lib/members.js";
import { loadDb, writeDb } from "../lib/store.js";
import { type AuthedRequest, makeRequireAuth } from "../middleware/auth.js";
import type { Project } from "../types.js";
import type { AppDeps, RouteContext } from "./types.js";

export const paramOf = (req: Request, name: string): string => String(req.params[name] ?? "");

/** The access a member route can rely on: the member middleware always sets it first. */
export function accessOf(req: Request): GrantedAccess {
  const access = (req as AuthedRequest).access;
  if (!access) throw new Error("accessOf used on a route without the member middleware");
  return access;
}

/**
 * The project, if it exists and the caller may touch it. A project that exists
 * but belongs to someone else answers exactly like one that does not exist, so
 * names and ids cannot be probed.
 */
export function ownedProject(req: Request, res: Response, id: string): Project | null {
  const project = loadDb().projects.find((p) => p.id === id);
  if (!project || !canAccessProject(project, accessOf(req))) {
    res.status(404).json({ error: "Not found" });
    return null;
  }
  return project;
}

export function makeContext(deps: AppDeps): RouteContext {
  const auth = makeRequireAuth(deps.authenticate) as unknown as RequestHandler;

  const memberCheck: RequestHandler = async (req: Request, res: Response, next: NextFunction) => {
    const authed = req as AuthedRequest;
    const identity = authed.user;
    if (!identity) {
      res.status(401).json({ error: "Unauthorized" });
      return;
    }
    // A fresh node has no members. The first founder to arrive becomes the owner
    // and takes over any projects from before ownership existed.
    if (loadDb().members.length === 0 && identity.role === "founder") {
      await writeDb((db) => {
        const owner = bootstrapOwner(db.members, identity, deps.now());
        if (owner) {
          adoptOrphanProjects(db.projects, owner.authUserId);
          recordAudit(db.audit, { actor: owner.authUserId, action: "member.grant", subject: owner.authUserId }, deps.now());
        }
      });
    }
    const access = resolveAccess(loadDb().members, identity);
    if (!access.ok) {
      res.status(403).json({
        error: access.reason,
        hint:
          access.reason === "suspended"
            ? "Your access to Deploy on this node has been suspended."
            : "Ask an operator of this node to give your account access to Deploy.",
      });
      return;
    }
    authed.access = access;
    next();
  };

  const operatorOnly: RequestHandler = (req: Request, res: Response, next: NextFunction) => {
    if (!accessOf(req).operator) {
      res.status(403).json({ error: "operators_only" });
      return;
    }
    next();
  };

  return { deps, auth, member: [auth, memberCheck], operator: [auth, memberCheck, operatorOnly] };
}

/** Record who did what. Never records where they were. */
export async function audit(
  deps: AppDeps,
  req: Request,
  action: AuditAction,
  subject?: string,
): Promise<void> {
  const actor = (req as AuthedRequest).userId ?? "unknown";
  await writeDb((db) => {
    recordAudit(db.audit, { actor, action, ...(subject !== undefined ? { subject } : {}) }, deps.now());
  });
}
````

**Create `src/routes/projects.ts`:**

````typescript
import crypto from "node:crypto";
import type { Router } from "express";
import { newId } from "../lib/auth.js";
import { validateEnv } from "../lib/env.js";
import { canAccessProject } from "../lib/members.js";
import { checkSlug, slugify } from "../lib/names.js";
import { applyPatch, parseNewProject, parsePatch } from "../lib/project-input.js";
import { recordAudit } from "../lib/audit.js";
import { checkProjectQuota } from "../lib/safeguards.js";
import { publicProject } from "../lib/source.js";
import { loadDb, writeDb } from "../lib/store.js";
import type { Project } from "../types.js";
import { accessOf, audit, ownedProject, paramOf } from "./context.js";
import type { RouteContext } from "./types.js";

const REASON_STATUS = { invalid: 400, reserved: 400, taken: 409, unverified: 503 } as const;

export function projectRoutes(router: Router, ctx: RouteContext): void {
  const { deps } = ctx;

  router.get("/api/projects", ...ctx.member, (req, res) => {
    const access = accessOf(req);
    const db = loadDb();
    res.json({
      projects: db.projects
        .filter((p) => canAccessProject(p, access))
        .map((p) => ({
          ...publicProject(p),
          latestDeployment: db.deployments.find((d) => d.projectId === p.id) ?? null,
        })),
    });
  });

  router.post("/api/projects", ...ctx.member, async (req, res) => {
    const access = accessOf(req);
    let input: ReturnType<typeof parseNewProject>;
    try {
      input = parseNewProject(req.body, deps.allowLocalRepos);
    } catch (error) {
      return void res.status(400).json({ error: (error as Error).message });
    }

    const owned = loadDb().projects.filter((p) => p.ownerId === access.memberId).length;
    const quota = checkProjectQuota(owned, deps.limits);
    if (!quota.ok) return void res.status(403).json({ error: quota.message, code: quota.code });

    const slug = slugify(input.name);
    const name = await checkSlug(slug, {
      takenLocally: (s) => loadDb().projects.some((p) => p.name === s),
      takenElsewhere: deps.takenElsewhere,
    });
    if (!name.ok) return void res.status(REASON_STATUS[name.reason]).json({ error: name.message, code: name.reason });

    const now = deps.now();
    const project: Project = {
      id: newId("prj"),
      ownerId: access.memberId,
      name: slug,
      repo: input.source.type === "git" ? input.source.repo : input.source.image,
      branch: input.source.type === "git" ? input.source.branch : "",
      source: input.source,
      buildContext: input.buildContext,
      ...(input.dockerfile ? { dockerfile: input.dockerfile } : {}),
      ...(input.healthPath ? { healthPath: input.healthPath } : {}),
      buildCommand: input.buildCommand,
      startCommand: input.startCommand,
      volumePath: input.volumePath,
      port: input.port,
      env: {},
      status: "idle",
      webhookSecret: crypto.randomBytes(24).toString("hex"),
      autoDeployEnabled: true,
      createdAt: now,
      updatedAt: now,
    };

    // Checked again inside the write, so two simultaneous creates cannot both win the name.
    let taken = false;
    await writeDb((db) => {
      if (db.projects.some((p) => p.name === slug)) {
        taken = true;
        return;
      }
      db.projects.unshift(project);
      recordAudit(db.audit, { actor: access.memberId, action: "project.create", subject: project.id }, now);
    });
    if (taken) return void res.status(409).json({ error: `"${slug}" is already taken.`, code: "taken" });
    res.status(201).json({ project: publicProject(project) });
  });

  router.get("/api/projects/:id", ...ctx.member, (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    res.json({
      project: publicProject(project),
      deployments: loadDb().deployments.filter((d) => d.projectId === project.id),
    });
  });

  router.put("/api/projects/:id", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    let patch: ReturnType<typeof parsePatch>;
    try {
      patch = parsePatch(req.body, project, deps.allowLocalRepos);
    } catch (error) {
      return void res.status(400).json({ error: (error as Error).message });
    }
    await writeDb((db) => {
      const stored = db.projects.find((p) => p.id === project.id);
      if (stored) applyPatch(stored, patch, deps.now());
    });
    await audit(deps, req, "project.update", project.id);
    const updated = loadDb().projects.find((p) => p.id === project.id);
    if (!updated) return void res.status(404).json({ error: "Not found" });
    res.json({ project: publicProject(updated) });
  });

  router.put("/api/projects/:id/env", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    let env: Record<string, string>;
    try {
      env = validateEnv((req.body as { env?: unknown } | null)?.env);
    } catch (error) {
      return void res.status(400).json({ error: (error as Error).message });
    }
    await writeDb((db) => {
      const stored = db.projects.find((p) => p.id === project.id);
      if (stored) {
        stored.env = env;
        stored.updatedAt = deps.now();
      }
    });
    await audit(deps, req, "project.update", project.id);
    res.json({ ok: true });
  });

  router.post("/api/projects/:id/regen-webhook-secret", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    const secret = crypto.randomBytes(24).toString("hex");
    await writeDb((db) => {
      const stored = db.projects.find((p) => p.id === project.id);
      if (stored) {
        stored.webhookSecret = secret;
        stored.updatedAt = deps.now();
      }
    });
    res.json({ webhookSecret: secret });
  });
}
````

**Create `src/routes/types.ts`:**

````typescript
import type { RequestHandler } from "express";
import type { ContainerStats } from "../lib/docker.js";
import type { RoutePublisher } from "../lib/publisher.js";
import type { Limits, Verdict } from "../lib/safeguards.js";
import type { Authenticator } from "../middleware/auth.js";
import type { Deployment, DeployTrigger, Project } from "../types.js";

/** Starting, stopping and cancelling builds. Injected so the routes are tested without Docker or Git. */
export type Pipeline = {
  createDeployment(project: Project, commitSha: string, trigger: DeployTrigger): Deployment;
  runDeploy(project: Project, deploymentId: string): Promise<void>;
  runRollback(project: Project, deploymentId: string, imageRef: string): Promise<void>;
  isBuilding(projectId: string): boolean;
  cancelBuild(deploymentId: string): Promise<boolean>;
  guardBuild(project: Project): Verdict;
};

/** What the routes do to containers, images and volumes. All of it targets the sandbox daemon. */
export type ProjectOps = {
  stop(name: string): Promise<void>;
  start(name: string): Promise<void>;
  restart(name: string): Promise<void>;
  remove(name: string): Promise<void>;
  status(name: string): Promise<string>;
  stats(name: string): Promise<ContainerStats | null>;
  logs(name: string, onLine: (line: string) => void, onEnd?: (err?: Error) => void): () => void;
  removeVolume(name: string): Promise<void>;
  removeImage(ref: string): Promise<boolean>;
};

export type AppDeps = {
  authenticate: Authenticator;
  pipeline: Pipeline;
  ops: ProjectOps;
  publisher: RoutePublisher;
  /** True if Cloud's registry or a Hosting site already uses the name. Throwing means "could not check". */
  takenElsewhere: (slug: string) => Promise<boolean>;
  limits: Limits;
  allowLocalRepos: boolean;
  now: () => number;
  node: { baseDomain: string; controlHost: string; hostPattern: string };
  version: string;
  /** Built dashboard to serve, or null for an API-only server (tests). */
  staticDir: string | null;
};

export type RouteContext = {
  deps: AppDeps;
  /** Signed in with Nexus-Auth. */
  auth: RequestHandler;
  /** Signed in, and an operator has let the account in (operators always are). */
  member: RequestHandler[];
  /** A member who is also an operator. */
  operator: RequestHandler[];
};
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **160 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/app.ts src/index.ts src/lib/project-input.ts src/lib/wiring.ts src/middleware/auth.ts src/routes/ai.ts src/routes/basic.ts src/routes/context.ts src/routes/projects.ts src/routes/types.ts tests/build-context.test.ts tests/project-input.test.ts tests/routes-projects.test.ts tests/support/app-harness.ts
git commit \
  -m "feat(app): injectable HTTP app with membership, ownership, name registry and project routes" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 13: Lifecycle routes, a webhook that always verifies, and authorised WebSocket streams

Adds deploy, rollback, cancel, stop/start/restart, stats, logs, status events, activity, export and delete, every one scoped to the caller's projects. Fixes three defects that exist in the original code (see 'Defects found'): the webhook failed on real GitHub requests and skipped signature verification when the header was absent, and the two live-log WebSocket endpoints corrupted each other.

**Files:**

- Create: `src/lib/ws.ts`
- Create: `src/routes/lifecycle.ts`
- Create: `src/routes/webhook.ts`
- Create: `tests/routes-lifecycle.test.ts`
- Create: `tests/webhook.test.ts`
- Create: `tests/ws.test.ts`
- Modify: `src/app.ts`
- Modify: `src/index.ts`
- Modify: `tests/support/app-harness.ts`

**Interfaces:**

- Consumes: Task 12.
- Produces: `lifecycleRoutes`, `cleanCommit`, `refuse`; `webhookRoutes`, `signatureMatches`; `attachSockets(server, deps)`; harness helpers `addProject`, `addDeployment`.

- [ ] **Step 1: Write the tests**

**Create `tests/routes-lifecycle.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { after, before, beforeEach, test } from "node:test";
import { emitStatusChange } from "../src/lib/events.js";
import { addDeployment, addProject, resetDb, startHarness, store } from "./support/app-harness.js";

let h: Awaited<ReturnType<typeof startHarness>>;
before(async () => {
  h = await startHarness();
});
after(async () => {
  await h.close();
});
beforeEach(async () => {
  resetDb();
  h.ops.calls.length = 0;
  h.pipeline.deploys.length = 0;
  h.pipeline.rollbacks.length = 0;
  h.pipeline.cancelled.length = 0;
  h.pipeline.building.clear();
  h.pipeline.verdict = { ok: true };
  h.publisher.unpublished.length = 0;
  // The founder arrives first (becoming owner) and lets Ada and Bob in.
  await h.call("GET", "/api/projects", { token: "t-boss" });
  await h.call("POST", "/api/admin/members", { token: "t-boss", body: { authUserId: "ada" } });
  await h.call("POST", "/api/admin/members", { token: "t-boss", body: { authUserId: "bob" } });
});

test("deploy starts a build for the owner, records who did it, and returns the deployment", async () => {
  const p = addProject({ ownerId: "ada" });
  const r = await h.call("POST", `/api/projects/${p.id}/deploy`, { token: "t-ada", body: { commitSha: "feedface" } });
  assert.equal(r.status, 200);
  assert.equal(r.body.deployment.commitSha, "feedface");
  assert.deepEqual(h.pipeline.deploys.map((d) => d.projectId), [p.id]);
  assert.ok(store.loadDb().audit.some((e) => e.action === "deploy.start" && e.actor === "ada" && e.subject === p.id));
});

test("a commit label that is not plain text is replaced, so it cannot reach logs or webhooks", async () => {
  const p = addProject({ ownerId: "ada" });
  const r = await h.call("POST", `/api/projects/${p.id}/deploy`, { token: "t-ada", body: { commitSha: "x; rm -rf /" } });
  assert.equal(r.body.deployment.commitSha, "manual");
});

test("someone else's project cannot be deployed, and nothing starts", async () => {
  const p = addProject({ ownerId: "bob" });
  const r = await h.call("POST", `/api/projects/${p.id}/deploy`, { token: "t-ada" });
  assert.equal(r.status, 404);
  assert.equal(h.pipeline.deploys.length, 0);
});

test("a build already running is a 409, and each safeguard refusal has its own status", async () => {
  const p = addProject({ ownerId: "ada" });
  h.pipeline.building.add(p.id);
  assert.equal((await h.call("POST", `/api/projects/${p.id}/deploy`, { token: "t-ada" })).status, 409);
  h.pipeline.building.clear();

  const cases = [
    ["disk", 503],
    ["busy", 409],
    ["rate", 429],
    ["quota", 403],
  ] as const;
  for (const [code, status] of cases) {
    h.pipeline.verdict = { ok: false, code, message: `refused: ${code}` };
    const r = await h.call("POST", `/api/projects/${p.id}/deploy`, { token: "t-ada" });
    assert.equal(r.status, status, code);
    assert.equal(r.body.code, code);
    assert.equal(r.body.error, `refused: ${code}`);
  }
  assert.equal(h.pipeline.deploys.length, 0, "no refused request started a build");
});

test("rollback re-runs the recorded image as a new deployment", async () => {
  const p = addProject({ ownerId: "ada", status: "live", containerName: "nexus-app-demo-new" });
  const old = addDeployment({ projectId: p.id, imageTag: "nexus/demo:old", commitSha: "0123456789abcdef" });
  const r = await h.call("POST", `/api/deployments/${old.id}/rollback`, { token: "t-ada" });
  assert.equal(r.status, 200);
  assert.deepEqual(h.pipeline.rollbacks.map((x) => x.imageRef), ["nexus/demo:old"]);
  assert.equal(r.body.deployment.triggeredBy, "rollback");
  assert.equal(store.loadDb().deployments.find((d) => d.id === r.body.deployment.id)?.imageTag, "nexus/demo:old");
});

test("rollback refuses a deployment with no image, a busy project, and another member's", async () => {
  const p = addProject({ ownerId: "ada" });
  const noImage = addDeployment({ projectId: p.id, imageTag: "" });
  assert.equal((await h.call("POST", `/api/deployments/${noImage.id}/rollback`, { token: "t-ada" })).status, 400);
  const withImage = addDeployment({ projectId: p.id, imageTag: "nexus/demo:old" });
  h.pipeline.building.add(p.id);
  assert.equal((await h.call("POST", `/api/deployments/${withImage.id}/rollback`, { token: "t-ada" })).status, 409);
  h.pipeline.building.clear();
  assert.equal((await h.call("POST", `/api/deployments/${withImage.id}/rollback`, { token: "t-bob" })).status, 404);
  assert.equal(h.pipeline.rollbacks.length, 0);
});

test("cancel stops a running build; one that is not running cannot be cancelled", async () => {
  const p = addProject({ ownerId: "ada" });
  const running = addDeployment({ projectId: p.id, status: "building" });
  const done = addDeployment({ projectId: p.id, status: "live" });
  const ok = await h.call("POST", `/api/deployments/${running.id}/cancel`, { token: "t-ada" });
  assert.deepEqual(ok.body, { ok: true, cancelled: true });
  assert.deepEqual(h.pipeline.cancelled, [running.id]);
  assert.equal((await h.call("POST", `/api/deployments/${done.id}/cancel`, { token: "t-ada" })).status, 400);
  assert.equal((await h.call("POST", `/api/deployments/${running.id}/cancel`, { token: "t-bob" })).status, 404);
});

test("if the build can no longer be killed, cancel still marks it cancelled", async () => {
  const p = addProject({ ownerId: "ada" });
  const running = addDeployment({ projectId: p.id, status: "building" });
  h.pipeline.cancellable = false;
  const r = await h.call("POST", `/api/deployments/${running.id}/cancel`, { token: "t-ada" });
  h.pipeline.cancellable = true;
  assert.equal(r.body.cancelled, false);
  assert.equal(store.loadDb().deployments.find((d) => d.id === running.id)?.status, "cancelled");
});

test("stop, start and restart act on the container serving now, falling back to the old fixed name", async () => {
  const modern = addProject({ ownerId: "ada", name: "modern", containerName: "nexus-app-modern-ab12cd34", status: "live" });
  const legacy = addProject({ ownerId: "ada", name: "legacy", containerId: "abc", status: "live" });
  await h.call("POST", `/api/projects/${modern.id}/stop`, { token: "t-ada" });
  await h.call("POST", `/api/projects/${legacy.id}/restart`, { token: "t-ada" });
  await h.call("POST", `/api/projects/${modern.id}/start`, { token: "t-ada" });
  assert.deepEqual(h.ops.calls, ["stop:nexus-app-modern-ab12cd34", "restart:nexus-app-legacy", "start:nexus-app-modern-ab12cd34"]);
  assert.equal(store.loadDb().projects.find((p) => p.id === modern.id)?.status, "live");
});

test("restart with nothing to restart is a 400, and other members' containers are untouchable", async () => {
  const never = addProject({ ownerId: "ada" });
  assert.equal((await h.call("POST", `/api/projects/${never.id}/restart`, { token: "t-ada" })).status, 400);
  const bobs = addProject({ ownerId: "bob", containerName: "nexus-app-bobs-x" });
  for (const action of ["stop", "start", "restart"]) {
    assert.equal((await h.call("POST", `/api/projects/${bobs.id}/${action}`, { token: "t-ada" })).status, 404, action);
  }
  assert.deepEqual(h.ops.calls, []);
});

test("health reports the container's state and corrects a project that says live but is not", async () => {
  const p = addProject({ ownerId: "ada", status: "live", containerName: "nexus-app-demo-x" });
  h.ops.statusValue = "exited";
  const r = await h.call("GET", `/api/projects/${p.id}/health`, { token: "t-ada" });
  h.ops.statusValue = "running";
  assert.deepEqual(r.body, { healthy: false, dockerStatus: "exited" });
  assert.equal(store.loadDb().projects.find((x) => x.id === p.id)?.status, "stopped");
});

test("stats only for a live project, and only for its owner", async () => {
  const live = addProject({ ownerId: "ada", status: "live", containerName: "nexus-app-demo-x" });
  const idle = addProject({ ownerId: "ada", name: "idle", status: "stopped" });
  assert.equal((await h.call("GET", `/api/projects/${live.id}/stats`, { token: "t-ada" })).body.stats.cpu, "1%");
  assert.equal((await h.call("GET", `/api/projects/${idle.id}/stats`, { token: "t-ada" })).body.stats, null);
  assert.equal((await h.call("GET", `/api/projects/${live.id}/stats`, { token: "t-bob" })).status, 404);
});

test("a build's log is readable by its owner, deletable by its owner, and nobody else", async () => {
  const p = addProject({ ownerId: "ada" });
  const dep = addDeployment({ projectId: p.id, logs: ["line one", "line two"], status: "live" });
  assert.deepEqual((await h.call("GET", `/api/deployments/${dep.id}/logs`, { token: "t-ada" })).body.logs, ["line one", "line two"]);
  assert.equal((await h.call("GET", `/api/deployments/${dep.id}/logs`, { token: "t-bob" })).status, 404);
  assert.equal((await h.call("DELETE", `/api/deployments/${dep.id}/logs`, { token: "t-bob" })).status, 404);
  assert.equal((await h.call("DELETE", `/api/deployments/${dep.id}/logs`, { token: "t-ada" })).status, 200);
  assert.deepEqual(store.loadDb().deployments.find((d) => d.id === dep.id)?.logs, ["[nexus] Log deleted by its owner"]);
});

test("a build in progress cannot have its log deleted", async () => {
  const p = addProject({ ownerId: "ada" });
  const dep = addDeployment({ projectId: p.id, status: "building" });
  assert.equal((await h.call("DELETE", `/api/deployments/${dep.id}/logs`, { token: "t-ada" })).status, 409);
});

test("activity shows only the caller's own deployments, operators' everything", async () => {
  const mine = addProject({ ownerId: "ada", name: "mine" });
  const theirs = addProject({ ownerId: "bob", name: "theirs" });
  addDeployment({ projectId: mine.id });
  addDeployment({ projectId: theirs.id });
  const ada = (await h.call("GET", "/api/activity", { token: "t-ada" })).body.activity;
  assert.deepEqual(ada.map((a: { projectName: string }) => a.projectName), ["mine"]);
  const boss = (await h.call("GET", "/api/activity", { token: "t-boss" })).body.activity;
  assert.equal(boss.length, 2);
});

test("the live status stream carries a member's own projects and nobody else's", async () => {
  const mine = addProject({ ownerId: "ada", name: "mine" });
  const theirs = addProject({ ownerId: "bob", name: "theirs" });
  const abort = new AbortController();
  const response = await fetch(`${h.url}/api/events?token=t-ada`, { signal: abort.signal });
  assert.equal(response.headers.get("content-type"), "text/event-stream");
  const reader = (response.body as ReadableStream<Uint8Array>).getReader();
  const decoder = new TextDecoder();
  let seen = "";
  const read = async () => {
    const { value } = await reader.read();
    seen += decoder.decode(value);
  };
  await read();
  assert.match(seen, /connected/);
  emitStatusChange(theirs.id, "building");
  emitStatusChange(mine.id, "live");
  while (!seen.includes(mine.id)) await read();
  abort.abort();
  assert.ok(seen.includes(mine.id));
  assert.ok(!seen.includes(theirs.id), "another member's project never appears");
});

test("an export has everything needed to recreate a project, and no credentials", async () => {
  const p = addProject({
    ownerId: "ada",
    name: "exported",
    source: { type: "git", repo: "ada/app", branch: "main", token: "ghp_exportsecret1" },
    env: { A: "1" },
    healthPath: "/up",
    port: 8080,
    webhookSecret: "whsec_secret_value",
  });
  const r = await h.call("GET", `/api/projects/${p.id}/export`, { token: "t-ada" });
  assert.equal(r.status, 200);
  assert.equal(r.body.format, "nexus-deploy-project/1");
  assert.deepEqual(r.body.project.source, { type: "git", repo: "ada/app", branch: "main" });
  assert.deepEqual(r.body.project.env, { A: "1" });
  assert.equal(r.body.project.healthPath, "/up");
  assert.equal(r.body.project.port, 8080);
  const text = JSON.stringify(r.body);
  assert.ok(!text.includes("ghp_exportsecret1") && !text.includes("whsec_secret_value"));
  assert.equal((await h.call("GET", `/api/projects/${p.id}/export`, { token: "t-bob" })).status, 404);
});

test("deleting a project removes its containers, route, volume, images, records and name", async () => {
  const p = addProject({
    ownerId: "ada",
    name: "doomed",
    containerName: "nexus-app-doomed-ab12cd34",
    imageTag: "nexus/doomed:ab12cd34",
    previousImageTag: "nexus/doomed:00000000",
  });
  addDeployment({ projectId: p.id, imageTag: "nexus/doomed:11111111" });
  const r = await h.call("DELETE", `/api/projects/${p.id}`, { token: "t-ada" });
  assert.equal(r.status, 200);

  assert.deepEqual(h.ops.calls.filter((c) => c.startsWith("remove:")).sort(), [
    "remove:nexus-app-doomed",
    "remove:nexus-app-doomed-ab12cd34",
  ]);
  assert.ok(h.ops.calls.includes("removeVolume:nexus-vol-doomed"));
  assert.deepEqual(h.ops.calls.filter((c) => c.startsWith("removeImage:")).sort(), [
    "removeImage:nexus/doomed:00000000",
    "removeImage:nexus/doomed:11111111",
    "removeImage:nexus/doomed:ab12cd34",
  ]);
  const containersFirst = h.ops.calls.findIndex((c) => c.startsWith("removeImage:"));
  assert.ok(h.ops.calls.findIndex((c) => c.startsWith("remove:")) < containersFirst, "containers go before their images");
  assert.deepEqual(h.publisher.unpublished, ["doomed"]);
  const db = store.loadDb();
  assert.equal(db.projects.length, 0);
  assert.equal(db.deployments.length, 0);
  assert.ok(db.audit.some((e) => e.action === "project.delete"));
});

test("a project cannot be deleted mid-build, or by someone else", async () => {
  const p = addProject({ ownerId: "ada" });
  h.pipeline.building.add(p.id);
  assert.equal((await h.call("DELETE", `/api/projects/${p.id}`, { token: "t-ada" })).status, 409);
  h.pipeline.building.clear();
  assert.equal((await h.call("DELETE", `/api/projects/${p.id}`, { token: "t-bob" })).status, 404);
  assert.equal(store.loadDb().projects.length, 1);
  assert.deepEqual(h.ops.calls, []);
});
````

**Modify `tests/support/app-harness.ts`:**

````diff
--- a/tests/support/app-harness.ts
+++ b/tests/support/app-harness.ts
@@ -25,6 +25,7 @@ process.env["SANDBOX_DOCKER_HOST"] = "unix:///does-not-exist.sock";
 
 export const store = await import("../../src/lib/store.js");
 const { createApp } = await import("../../src/app.js");
+const { attachSockets } = await import("../../src/lib/ws.js");
 
 export const IDENTITIES: Record<string, NexusIdentity> = {
   "t-boss": { id: "boss", role: "founder", username: "boss" },
@@ -40,6 +41,50 @@ export const fakeAuth: Authenticator = async (token) => {
   return identity ? { kind: "ok", identity } : { kind: "denied" };
 };
 
+/** Insert a project directly, for tests that start from a project in a given state. */
+export function addProject(patch: Partial<import("../../src/types.js").Project> = {}) {
+  const project = {
+    id: `prj_${Math.random().toString(36).slice(2, 10)}`,
+    ownerId: "ada",
+    name: "demo",
+    repo: "o/r",
+    branch: "main",
+    buildContext: ".",
+    buildCommand: "",
+    startCommand: "",
+    volumePath: "/workspace",
+    port: 3000,
+    env: {},
+    status: "idle",
+    autoDeployEnabled: true,
+    webhookSecret: "whsec_test_secret",
+    createdAt: 1,
+    updatedAt: 1,
+    ...patch,
+  } as import("../../src/types.js").Project;
+  const db = store.loadDb();
+  db.projects.unshift(project);
+  store.saveDb(db);
+  return project;
+}
+
+export function addDeployment(patch: Partial<import("../../src/types.js").Deployment> & { projectId: string }) {
+  const dep = {
+    id: `dep_${Math.random().toString(36).slice(2, 12)}`,
+    commitSha: "abc12345",
+    triggeredBy: "manual",
+    status: "live",
+    imageTag: "nexus/demo:old",
+    logs: ["[nexus] hello"],
+    createdAt: 1,
+    ...patch,
+  } as import("../../src/types.js").Deployment;
+  const db = store.loadDb();
+  db.deployments.unshift(dep);
+  store.saveDb(db);
+  return dep;
+}
+
 export function resetDb(): void {
   store.saveDb({ users: [], projects: [], deployments: [], sessions: [], members: [], audit: [] });
 }
@@ -148,6 +193,7 @@ export async function startHarness(patch: Partial<AppDeps> = {}) {
     ...patch,
   };
   const server: Server = createApp(deps).listen(0, "127.0.0.1");
+  attachSockets(server, deps);
   await once(server, "listening");
   const url = `http://127.0.0.1:${(server.address() as AddressInfo).port}`;
````

**Create `tests/webhook.test.ts`:**

````typescript
import assert from "node:assert/strict";
import crypto from "node:crypto";
import { after, before, beforeEach, test } from "node:test";
import { addProject, resetDb, startHarness, store } from "./support/app-harness.js";

let h: Awaited<ReturnType<typeof startHarness>>;
before(async () => {
  h = await startHarness();
});
after(async () => {
  await h.close();
});

const SECRET = "whsec_test_secret";
const sign = (body: string, secret = SECRET) => `sha256=${crypto.createHmac("sha256", secret).update(body).digest("hex")}`;
const push = (project: { id: string }, body: object, headers: Record<string, string> = {}) => {
  const raw = JSON.stringify(body);
  return h.call("POST", `/api/webhooks/github/${project.id}`, {
    rawBody: raw,
    headers: { "Content-Type": "application/json", "X-GitHub-Event": "push", "X-Hub-Signature-256": sign(raw), ...headers },
  });
};

beforeEach(() => {
  resetDb();
  h.pipeline.deploys.length = 0;
  h.pipeline.building.clear();
  h.pipeline.verdict = { ok: true };
});

test("a correctly signed push to the tracked branch starts a build", async () => {
  const p = addProject({});
  const r = await push(p, { ref: "refs/heads/main", after: "deadbeefcafe" });
  assert.equal(r.status, 200);
  assert.ok(r.body.deploymentId);
  assert.deepEqual(h.pipeline.deploys.map((d) => d.projectId), [p.id]);
  assert.equal(store.loadDb().deployments[0]?.triggeredBy, "webhook");
  assert.equal(store.loadDb().deployments[0]?.commitSha, "deadbeefcafe");
});

test("a request with no signature is refused, however it is dressed up", async () => {
  const p = addProject({});
  const raw = JSON.stringify({ ref: "refs/heads/main" });
  const r = await h.call("POST", `/api/webhooks/github/${p.id}`, {
    rawBody: raw,
    headers: { "Content-Type": "application/json", "X-GitHub-Event": "push" },
  });
  assert.equal(r.status, 401);
  assert.equal(h.pipeline.deploys.length, 0);
});

test("a wrong, truncated or differently-keyed signature is refused", async () => {
  const p = addProject({});
  const raw = JSON.stringify({ ref: "refs/heads/main" });
  for (const bad of [sign(raw, "another-secret"), sign(raw).slice(0, -4), "sha256=", "nonsense", sign("different body")]) {
    const r = await push(p, { ref: "refs/heads/main" }, { "X-Hub-Signature-256": bad });
    assert.equal(r.status, 401, bad);
  }
  assert.equal(h.pipeline.deploys.length, 0);
});

test("a project with no webhook secret refuses all webhooks", async () => {
  const p = addProject({ webhookSecret: "" });
  assert.equal((await push(p, { ref: "refs/heads/main" })).status, 403);
});

test("an unknown project is a 404", async () => {
  const r = await push({ id: "prj_nope" }, { ref: "refs/heads/main" });
  assert.equal(r.status, 404);
});

test("GitHub's ping is answered and builds nothing", async () => {
  const p = addProject({});
  const r = await push(p, { zen: "hello" }, { "X-GitHub-Event": "ping" });
  assert.deepEqual(r.body, { ok: true, pong: true });
  assert.equal(h.pipeline.deploys.length, 0);
});

test("events other than push are skipped", async () => {
  const p = addProject({});
  const r = await push(p, { action: "opened" }, { "X-GitHub-Event": "issues" });
  assert.match(r.body.skipped, /event issues/);
  assert.equal(h.pipeline.deploys.length, 0);
});

test("a push to another branch, or with auto-deploy off, or mid-build, is skipped", async () => {
  const other = await push(addProject({}), { ref: "refs/heads/feature" });
  assert.match(other.body.skipped, /branch feature/);

  const off = await push(addProject({ autoDeployEnabled: false }), { ref: "refs/heads/main" });
  assert.match(off.body.skipped, /auto-deploy disabled/);

  const busy = addProject({});
  h.pipeline.building.add(busy.id);
  assert.match((await push(busy, { ref: "refs/heads/main" })).body.skipped, /build in progress/);
  assert.equal(h.pipeline.deploys.length, 0);
});

test("a push counts against the safeguards, so a push loop cannot bypass them", async () => {
  const p = addProject({});
  h.pipeline.verdict = { ok: false, code: "rate", message: "too many builds" };
  const r = await push(p, { ref: "refs/heads/main" });
  assert.equal(r.status, 200);
  assert.equal(r.body.skipped, "too many builds");
  assert.equal(h.pipeline.deploys.length, 0);
});

test("a suspended member's projects stop building on push", async () => {
  const p = addProject({ ownerId: "ada" });
  const db = store.loadDb();
  db.members.push({ authUserId: "ada", role: "member", status: "suspended", grantedBy: "boss", grantedAt: 1 });
  store.saveDb(db);
  const r = await push(p, { ref: "refs/heads/main" });
  assert.equal(r.body.skipped, "owner suspended");
  assert.equal(h.pipeline.deploys.length, 0);
});

test("an image project has no use for a push webhook", async () => {
  const p = addProject({ repo: "nginx", branch: "", source: { type: "image", image: "nginx" } });
  assert.equal((await push(p, { ref: "refs/heads/main" })).status, 400);
});

test("a signed body that is not JSON is a 400", async () => {
  const p = addProject({});
  const raw = "this is not json";
  const r = await h.call("POST", `/api/webhooks/github/${p.id}`, {
    rawBody: raw,
    headers: { "X-GitHub-Event": "push", "X-Hub-Signature-256": sign(raw), "Content-Type": "application/json" },
  });
  assert.equal(r.status, 400);
});
````

**Create `tests/ws.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { after, before, beforeEach, test } from "node:test";
import { WebSocket } from "ws";
import { addDeployment, addProject, resetDb, startHarness } from "./support/app-harness.js";

let h: Awaited<ReturnType<typeof startHarness>>;
before(async () => {
  h = await startHarness();
});
after(async () => {
  await h.close();
});
beforeEach(async () => {
  resetDb();
  await h.call("GET", "/api/projects", { token: "t-boss" });
  await h.call("POST", "/api/admin/members", { token: "t-boss", body: { authUserId: "ada" } });
  await h.call("POST", "/api/admin/members", { token: "t-boss", body: { authUserId: "bob" } });
});

type Outcome = { messages: Array<{ type: string; line?: string; status?: string }>; code: number };

function open(pathAndQuery: string): Promise<Outcome> {
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(`${h.url.replace("http", "ws")}${pathAndQuery}`);
    const messages: Outcome["messages"] = [];
    socket.on("message", (data) => messages.push(JSON.parse(data.toString())));
    socket.on("close", (code) => resolve({ messages, code }));
    socket.on("error", reject);
    setTimeout(() => socket.close(), 1500);
  });
}

test("no token, a bad token and an account that was never let in are all turned away", async () => {
  const p = addProject({ ownerId: "ada" });
  const dep = addDeployment({ projectId: p.id });
  for (const query of ["", "&token=nope", "&token=t-cy"]) {
    const r = await open(`/api/log-stream?deploymentId=${dep.id}${query}`);
    assert.equal(r.code, 1008, query);
    assert.deepEqual(r.messages, []);
  }
});

test("an owner receives their finished build's log and a done marker", async () => {
  const p = addProject({ ownerId: "ada" });
  const dep = addDeployment({ projectId: p.id, logs: ["one", "two"], status: "live" });
  const r = await open(`/api/log-stream?deploymentId=${dep.id}&token=t-ada`);
  assert.deepEqual(r.messages, [
    { type: "log", line: "one" },
    { type: "log", line: "two" },
    { type: "done", status: "live" },
  ]);
});

test("another member's build log is refused exactly like one that does not exist", async () => {
  const p = addProject({ ownerId: "bob" });
  const dep = addDeployment({ projectId: p.id, logs: ["secret line"] });
  const theirs = await open(`/api/log-stream?deploymentId=${dep.id}&token=t-ada`);
  const missing = await open("/api/log-stream?deploymentId=dep_missing&token=t-ada");
  assert.equal(theirs.code, 1008);
  assert.deepEqual(theirs.messages, []);
  assert.equal(missing.code, 1008);
});

test("an operator can watch any build", async () => {
  const p = addProject({ ownerId: "bob" });
  const dep = addDeployment({ projectId: p.id, logs: ["visible"], status: "failed" });
  const r = await open(`/api/log-stream?deploymentId=${dep.id}&token=t-boss`);
  assert.deepEqual(r.messages.at(0), { type: "log", line: "visible" });
});

test("a container's output streams to its owner and to nobody else", async () => {
  const p = addProject({ ownerId: "ada", name: "streamy", containerName: "nexus-app-streamy-x" });
  h.ops.logLines = ["hello from the app"];
  const mine = await open(`/api/container-stream?projectId=${p.id}&token=t-ada`);
  assert.ok(mine.messages.some((m) => m.type === "log" && m.line === "hello from the app"));
  assert.ok(h.ops.calls.includes("logs:nexus-app-streamy-x"));
  h.ops.calls.length = 0;
  const theirs = await open(`/api/container-stream?projectId=${p.id}&token=t-bob`);
  assert.equal(theirs.code, 1008);
  assert.deepEqual(h.ops.calls, [], "no log process was started for the wrong member");
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/routes-lifecycle.test.ts tests/webhook.test.ts tests/ws.test.ts
```

Expected: FAIL — `Cannot find module` / assertion failures in `routes-lifecycle.test.ts`, `webhook.test.ts` and `ws.test.ts`.

- [ ] **Step 3: Implement**

**Modify `src/app.ts`:**

````diff
--- a/src/app.ts
+++ b/src/app.ts
@@ -5,8 +5,10 @@ import helmet from "helmet";
 import { aiRoutes } from "./routes/ai.js";
 import { basicRoutes } from "./routes/basic.js";
 import { makeContext } from "./routes/context.js";
+import { lifecycleRoutes } from "./routes/lifecycle.js";
 import { projectRoutes } from "./routes/projects.js";
 import type { AppDeps } from "./routes/types.js";
+import { webhookRoutes } from "./routes/webhook.js";
 
 export type { AppDeps } from "./routes/types.js";
 
@@ -22,12 +24,17 @@ export function createApp(deps: AppDeps): express.Express {
 
   const ctx = makeContext(deps);
 
+  // The webhook verifies a signature over the raw bytes, so it is registered
+  // before the JSON parser, which would consume them.
+  webhookRoutes(app, ctx);
+
   app.use(express.json({ limit: "1mb" }));
 
   const api = express.Router();
   basicRoutes(api, ctx);
   aiRoutes(api, ctx);
   projectRoutes(api, ctx);
+  lifecycleRoutes(api, ctx);
   app.use(api);
 
   // An unknown API path is a JSON 404, never the dashboard's HTML.
````

**Modify `src/index.ts`:**

````diff
--- a/src/index.ts
+++ b/src/index.ts
@@ -2,11 +2,13 @@ import { createServer } from "node:http";
 import { createApp } from "./app.js";
 import { config, ensureDataDir } from "./lib/config.js";
 import { startStatusSync } from "./lib/status-sync.js";
+import { attachSockets } from "./lib/ws.js";
 import { realDeps } from "./lib/wiring.js";
 
 ensureDataDir();
 const deps = realDeps();
 const server = createServer(createApp(deps));
+attachSockets(server, deps);
 
 startStatusSync(30_000);
````

**Create `src/lib/ws.ts`:**

````typescript
/**
 * Live streams over WebSocket: a build's log, and a running container's output.
 *
 * A browser cannot put an Authorization header on a WebSocket, so the credential
 * arrives as `?token=`. It goes through exactly the same checks as an HTTP
 * request — identity, membership, ownership — because an upgrade must not be a
 * cheaper way in.
 */
import type { Server } from "node:http";
import { type WebSocket, WebSocketServer } from "ws";
import { verifyWithNexusAuth } from "../middleware/auth.js";
import type { AppDeps } from "../routes/types.js";
import { subscribeToDeployment } from "./build.js";
import { containerNameOf } from "./containers.js";
import { canAccessProject, resolveAccess } from "./members.js";
import { loadDb } from "./store.js";

const HEARTBEAT_MS = 15_000;

async function accessFor(token: string | null, deps: AppDeps) {
  if (!token) return null;
  const identity = await verifyWithNexusAuth(token, deps.authenticate);
  if (!identity) return null;
  const access = resolveAccess(loadDb().members, identity);
  return access.ok ? access : null;
}

function keepAlive(socket: WebSocket): () => void {
  const timer = setInterval(() => {
    if (socket.readyState === socket.OPEN) socket.send(JSON.stringify({ type: "ping" }));
  }, HEARTBEAT_MS);
  return () => clearInterval(timer);
}

export function attachSockets(server: Server, deps: AppDeps): void {
  // Two WebSocketServers attached directly to one HTTP server both answer every
  // upgrade request, so the one whose path does not match rejects the connection
  // the other has just accepted. Each is therefore detached (`noServer`) and one
  // handler below routes upgrades by path.
  const buildLogs = new WebSocketServer({ noServer: true });
  const containerLogs = new WebSocketServer({ noServer: true });

  server.on("upgrade", (req, socket, head) => {
    const { pathname } = new URL(req.url ?? "", "http://localhost");
    const target =
      pathname === "/api/log-stream" ? buildLogs : pathname === "/api/container-stream" ? containerLogs : null;
    if (!target) {
      socket.destroy();
      return;
    }
    target.handleUpgrade(req, socket, head, (ws) => target.emit("connection", ws, req));
  });

  // ── A build's log ────────────────────────────────────────────────────────
  buildLogs.on("connection", async (socket, req) => {
    const url = new URL(req.url ?? "", "http://localhost");
    const access = await accessFor(url.searchParams.get("token"), deps);
    if (!access) return void socket.close(1008, "Unauthorized");

    const deploymentId = url.searchParams.get("deploymentId");
    if (!deploymentId) return void socket.close(1008, "Missing deploymentId");
    const dep = loadDb().deployments.find((d) => d.id === deploymentId);
    const project = dep && loadDb().projects.find((p) => p.id === dep.projectId);
    // Someone else's build answers exactly like one that does not exist.
    if (!dep || !project || !canAccessProject(project, access)) return void socket.close(1008, "Not found");

    for (const line of dep.logs) socket.send(JSON.stringify({ type: "log", line }));
    if (dep.status === "live" || dep.status === "failed" || dep.status === "cancelled") {
      socket.send(JSON.stringify({ type: "done", status: dep.status }));
      return void socket.close();
    }

    const unsubscribe = subscribeToDeployment(deploymentId, (line) => {
      if (socket.readyState !== socket.OPEN) return;
      socket.send(JSON.stringify({ type: "log", line }));
      const current = loadDb().deployments.find((d) => d.id === deploymentId);
      if (current && (current.status === "live" || current.status === "failed" || current.status === "cancelled")) {
        socket.send(JSON.stringify({ type: "done", status: current.status }));
        socket.close();
      }
    });
    const stopBeat = keepAlive(socket);
    socket.on("close", () => {
      unsubscribe();
      stopBeat();
    });
  });

  // ── A running container's output ─────────────────────────────────────────
  containerLogs.on("connection", async (socket, req) => {
    const url = new URL(req.url ?? "", "http://localhost");
    const access = await accessFor(url.searchParams.get("token"), deps);
    if (!access) return void socket.close(1008, "Unauthorized");

    const projectId = url.searchParams.get("projectId");
    const project = projectId ? loadDb().projects.find((p) => p.id === projectId) : undefined;
    if (!project || !canAccessProject(project, access)) return void socket.close(1008, "Not found");

    const name = containerNameOf(project);
    let closed = false;
    socket.send(JSON.stringify({ type: "info", line: `[nexus] Streaming logs from ${name}…` }));

    const stopLogs = deps.ops.logs(
      name,
      (line) => {
        if (!closed && socket.readyState === socket.OPEN) socket.send(JSON.stringify({ type: "log", line }));
      },
      (err) => {
        if (!closed && socket.readyState === socket.OPEN) {
          if (err) socket.send(JSON.stringify({ type: "error", line: `[nexus] ${err.message}` }));
          socket.close();
        }
      },
    );
    const stopBeat = keepAlive(socket);
    socket.on("close", () => {
      closed = true;
      stopBeat();
      stopLogs();
    });
  });
}
````

**Create `src/routes/lifecycle.ts`:**

````typescript
import type { Request, Response, Router } from "express";
import { containerNameOf } from "../lib/containers.js";
import { canAccessProject } from "../lib/members.js";
import type { Verdict } from "../lib/safeguards.js";
import { publicProject } from "../lib/source.js";
import { loadDb, writeDb } from "../lib/store.js";
import { onStatusChange } from "../lib/events.js";
import type { Deployment } from "../types.js";
import { accessOf, audit, ownedProject, paramOf } from "./context.js";
import type { RouteContext } from "./types.js";

const VERDICT_STATUS = { quota: 403, busy: 409, rate: 429, disk: 503 } as const;

/** A commit label ends up in logs and notifications, so only a plain one is kept. */
export function cleanCommit(value: unknown): string {
  return typeof value === "string" && /^[A-Za-z0-9._:-]{1,64}$/.test(value) ? value : "manual";
}

export function refuse(res: Response, verdict: Extract<Verdict, { ok: false }>): void {
  res.status(VERDICT_STATUS[verdict.code]).json({ error: verdict.message, code: verdict.code });
}

/** The deployment, if it exists and belongs to a project the caller may touch. */
function ownedDeployment(req: Request, res: Response, id: string): Deployment | null {
  const dep = loadDb().deployments.find((d) => d.id === id);
  const project = dep && loadDb().projects.find((p) => p.id === dep.projectId);
  if (!dep || !project || !canAccessProject(project, accessOf(req))) {
    res.status(404).json({ error: "Not found" });
    return null;
  }
  return dep;
}

export function lifecycleRoutes(router: Router, ctx: RouteContext): void {
  const { deps } = ctx;
  const { pipeline, ops } = deps;

  // ── Deploy ───────────────────────────────────────────────────────────────
  router.post("/api/projects/:id/deploy", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    if (pipeline.isBuilding(project.id)) return void res.status(409).json({ error: "Build already in progress" });
    const verdict = pipeline.guardBuild(project);
    if (!verdict.ok) return void refuse(res, verdict);

    const dep = pipeline.createDeployment(project, cleanCommit(req.body?.commitSha), "manual");
    pipeline.runDeploy(project, dep.id).catch((err) => console.error("[nexus] deploy error:", (err as Error).message));
    await audit(deps, req, "deploy.start", project.id);
    res.json({ deployment: dep });
  });

  router.post("/api/deployments/:id/rollback", ...ctx.member, async (req, res) => {
    const source = ownedDeployment(req, res, paramOf(req, "id"));
    if (!source) return;
    if (!source.imageTag) return void res.status(400).json({ error: "No image — cannot roll back" });
    const project = loadDb().projects.find((p) => p.id === source.projectId);
    if (!project) return void res.status(404).json({ error: "Not found" });
    if (pipeline.isBuilding(project.id)) return void res.status(409).json({ error: "Build in progress" });

    const dep = pipeline.createDeployment(project, `rollback:${source.commitSha.slice(0, 8)}`, "rollback");
    await writeDb((db) => {
      const found = db.deployments.find((d) => d.id === dep.id);
      if (found) found.imageTag = source.imageTag;
    });
    pipeline.runRollback(project, dep.id, source.imageTag).catch(() => {});
    await audit(deps, req, "deploy.rollback", project.id);
    res.json({ deployment: dep });
  });

  router.post("/api/deployments/:id/cancel", ...ctx.member, async (req, res) => {
    const dep = ownedDeployment(req, res, paramOf(req, "id"));
    if (!dep) return;
    if (dep.status !== "building" && dep.status !== "queued") {
      return void res.status(400).json({ error: "Deployment is not in progress" });
    }
    const cancelled = await pipeline.cancelBuild(dep.id);
    if (!cancelled) {
      await writeDb((db) => {
        const d = db.deployments.find((x) => x.id === dep.id);
        if (d) {
          d.status = "cancelled";
          d.finishedAt = deps.now();
        }
      });
    }
    await audit(deps, req, "deploy.cancel", dep.projectId);
    res.json({ ok: true, cancelled });
  });

  // ── Build logs ───────────────────────────────────────────────────────────
  router.get("/api/deployments/:id/logs", ...ctx.member, (req, res) => {
    const dep = ownedDeployment(req, res, paramOf(req, "id"));
    if (!dep) return;
    res.json({ logs: dep.logs, status: dep.status });
  });

  // The owner can erase a build's log. Nothing else keeps it for them.
  router.delete("/api/deployments/:id/logs", ...ctx.member, async (req, res) => {
    const dep = ownedDeployment(req, res, paramOf(req, "id"));
    if (!dep) return;
    if (dep.status === "building" || dep.status === "queued") {
      return void res.status(409).json({ error: "This build is still running" });
    }
    await writeDb((db) => {
      const d = db.deployments.find((x) => x.id === dep.id);
      if (d) d.logs = ["[nexus] Log deleted by its owner"];
    });
    await audit(deps, req, "logs.delete", dep.projectId);
    res.json({ ok: true });
  });

  // ── Running container ────────────────────────────────────────────────────
  const setStatus = (id: string, status: "stopped" | "live") =>
    writeDb((db) => {
      const p = db.projects.find((x) => x.id === id);
      if (p) {
        p.status = status;
        p.updatedAt = deps.now();
      }
    });

  router.post("/api/projects/:id/stop", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    await ops.stop(containerNameOf(project));
    await setStatus(project.id, "stopped");
    await audit(deps, req, "project.stop", project.id);
    res.json({ ok: true });
  });

  router.post("/api/projects/:id/start", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    await ops.start(containerNameOf(project));
    await setStatus(project.id, "live");
    await audit(deps, req, "project.start", project.id);
    res.json({ ok: true });
  });

  router.post("/api/projects/:id/restart", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    if (!project.containerName && !project.containerId) {
      return void res.status(400).json({ error: "No container to restart" });
    }
    await ops.restart(containerNameOf(project));
    await setStatus(project.id, "live");
    await audit(deps, req, "project.restart", project.id);
    res.json({ ok: true });
  });

  router.get("/api/projects/:id/health", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    const status = await ops.status(containerNameOf(project));
    const healthy = status === "running";
    if (!healthy && project.status === "live") await setStatus(project.id, "stopped");
    res.json({ healthy, dockerStatus: status });
  });

  router.get("/api/projects/:id/stats", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    if (project.status !== "live") return void res.json({ stats: null, reason: "not running" });
    res.json({ stats: await ops.stats(containerNameOf(project)) });
  });

  // ── Activity and live status ─────────────────────────────────────────────
  router.get("/api/activity", ...ctx.member, (req, res) => {
    const access = accessOf(req);
    const db = loadDb();
    const visible = new Map(db.projects.filter((p) => canAccessProject(p, access)).map((p) => [p.id, p.name]));
    res.json({
      activity: db.deployments
        .filter((d) => visible.has(d.projectId))
        .slice(0, 25)
        .map((d) => ({
          id: d.id,
          projectId: d.projectId,
          projectName: visible.get(d.projectId) ?? "deleted",
          commitSha: d.commitSha,
          triggeredBy: d.triggeredBy,
          status: d.status,
          createdAt: d.createdAt,
          finishedAt: d.finishedAt,
        })),
    });
  });

  // Status changes for the caller's own projects only.
  router.get("/api/events", ...ctx.member, (req, res) => {
    const access = accessOf(req);
    res.setHeader("Content-Type", "text/event-stream");
    res.setHeader("Cache-Control", "no-cache");
    res.setHeader("Connection", "keep-alive");
    res.flushHeaders();
    const send = (data: object) => res.write(`data: ${JSON.stringify(data)}\n\n`);
    send({ type: "connected" });

    const unsubscribe = onStatusChange((projectId, status) => {
      const project = loadDb().projects.find((p) => p.id === projectId);
      if (project && canAccessProject(project, access)) send({ type: "status", projectId, status });
    });
    const heartbeat = setInterval(() => send({ type: "ping" }), 20_000);
    req.on("close", () => {
      unsubscribe();
      clearInterval(heartbeat);
    });
  });

  // ── Your data is yours ───────────────────────────────────────────────────
  // Everything needed to recreate the project elsewhere. The repository token
  // and webhook secret are credentials, not settings, and are left out.
  router.get("/api/projects/:id/export", ...ctx.member, (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    const source = publicProject(project).source;
    res.json({
      exportedAt: new Date(deps.now()).toISOString(),
      format: "nexus-deploy-project/1",
      project: {
        name: project.name,
        source: source.type === "git" ? { type: "git", repo: source.repo, branch: source.branch } : source,
        buildContext: project.buildContext,
        dockerfile: project.dockerfile ?? null,
        healthPath: project.healthPath ?? null,
        volumePath: project.volumePath,
        port: project.port,
        env: project.env,
        memoryLimit: project.memoryLimit ?? null,
        cpus: project.cpus ?? null,
        notifyUrl: project.notifyUrl ?? null,
        autoDeployEnabled: project.autoDeployEnabled,
      },
    });
  });

  // Deleting a project removes everything it left behind: containers, the route,
  // the data volume, its images, its logs and its name.
  router.delete("/api/projects/:id", ...ctx.member, async (req, res) => {
    const project = ownedProject(req, res, paramOf(req, "id"));
    if (!project) return;
    if (pipeline.isBuilding(project.id)) {
      return void res.status(409).json({ error: "A build is running; cancel it first" });
    }
    const db = loadDb();
    const images = new Set(
      [
        project.imageTag,
        project.previousImageTag,
        ...db.deployments.filter((d) => d.projectId === project.id).map((d) => d.imageTag),
      ].filter((ref): ref is string => Boolean(ref)),
    );
    for (const name of new Set([containerNameOf(project), `nexus-app-${project.name}`])) {
      await ops.remove(name).catch(() => {});
    }
    await deps.publisher.unpublish(project.name).catch(() => {});
    await ops.removeVolume(`nexus-vol-${project.name}`).catch(() => {});
    for (const ref of images) await ops.removeImage(ref).catch(() => false);
    await writeDb((d) => {
      d.projects = d.projects.filter((p) => p.id !== project.id);
      d.deployments = d.deployments.filter((x) => x.projectId !== project.id);
    });
    await audit(deps, req, "project.delete", project.id);
    res.json({ ok: true });
  });
}
````

**Create `src/routes/webhook.ts`:**

````typescript
import crypto from "node:crypto";
import express, { type Express } from "express";
import { sourceOf } from "../lib/source.js";
import { loadDb } from "../lib/store.js";
import { cleanCommit } from "./lifecycle.js";
import type { RouteContext } from "./types.js";

/** `sha256=<hex>` of the exact bytes received, compared without leaking timing. */
export function signatureMatches(secret: string, body: Buffer, header: string | undefined): boolean {
  if (!header) return false;
  const expected = Buffer.from(`sha256=${crypto.createHmac("sha256", secret).update(body).digest("hex")}`);
  const given = Buffer.from(header);
  return given.length === expected.length && crypto.timingSafeEqual(given, expected);
}

/**
 * GitHub push webhook. Registered on the app itself, ahead of the JSON parser:
 * the signature is over the raw bytes, which a parsed body no longer has.
 *
 * A valid signature is required every time. (An earlier version skipped the check
 * when the header was absent, which let anyone who knew a project id start builds.)
 */
export function webhookRoutes(app: Express, ctx: RouteContext): void {
  const { deps } = ctx;

  app.post(
    "/api/webhooks/github/:projectId",
    express.raw({ type: "*/*", limit: "1mb" }),
    (req, res) => {
      const project = loadDb().projects.find((p) => p.id === req.params["projectId"]);
      if (!project) return void res.status(404).json({ error: "Project not found" });

      const secret = project.webhookSecret;
      const body = Buffer.isBuffer(req.body) ? req.body : Buffer.alloc(0);
      if (!secret) return void res.status(403).json({ error: "Webhook not configured" });
      if (!signatureMatches(secret, body, req.get("x-hub-signature-256"))) {
        return void res.status(401).json({ error: "Invalid signature" });
      }

      const event = req.get("x-github-event");
      if (event === "ping") return void res.json({ ok: true, pong: true });
      if (event !== undefined && event !== "push") return void res.json({ ok: true, skipped: `event ${event}` });

      let payload: { ref?: unknown; after?: unknown };
      try {
        payload = JSON.parse(body.toString("utf8"));
      } catch {
        return void res.status(400).json({ error: "Bad payload" });
      }

      const source = sourceOf(project);
      if (source.type !== "git") {
        return void res.status(400).json({ error: "Image projects do not use push webhooks" });
      }
      const branch = typeof payload.ref === "string" ? payload.ref.replace("refs/heads/", "") : undefined;
      if (branch && branch !== source.branch) {
        return void res.json({ ok: true, skipped: `branch ${branch} ≠ ${source.branch}` });
      }
      if (!project.autoDeployEnabled) return void res.json({ ok: true, skipped: "auto-deploy disabled" });
      if (deps.pipeline.isBuilding(project.id)) return void res.json({ ok: true, skipped: "build in progress" });

      // A suspended member's projects stop building on push too.
      const owner = loadDb().members.find((m) => m.authUserId === project.ownerId);
      if (owner?.status === "suspended") return void res.json({ ok: true, skipped: "owner suspended" });

      // Pushes count against the same safeguards as a manual deploy, or a push loop would bypass them.
      const verdict = deps.pipeline.guardBuild(project);
      if (!verdict.ok) return void res.json({ ok: true, skipped: verdict.message });

      const dep = deps.pipeline.createDeployment(project, cleanCommit(payload.after), "webhook");
      deps.pipeline.runDeploy(project, dep.id).catch(() => {});
      res.json({ ok: true, deploymentId: dep.id });
    },
  );
}
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **196 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/app.ts src/index.ts src/lib/ws.ts src/routes/lifecycle.ts src/routes/webhook.ts tests/routes-lifecycle.test.ts tests/support/app-harness.ts tests/webhook.test.ts tests/ws.test.ts
git commit \
  -m "feat(routes): lifecycle routes, a webhook that always verifies, and authorised log streams" \
  -m "Also fixes the original webhook (500 on signed JSON, signature skipped when absent,
ping triggered a build) and the two WebSocket servers that rejected each other." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 14: Image pruning that keeps rollback targets, and status sync on per-release names

The sandbox daemon belongs to Deploy alone, so every image in it is Deploy's. The hourly sweep removes any image no project, rollback target or recent deployment references, recognising a pulled image by its digest. The status sync follows the per-release container name, and the same sweep forgets audit entries past thirty days.

**Files:**

- Create: `src/lib/prune.ts`
- Create: `tests/prune.test.ts`
- Modify: `src/lib/status-sync.ts`

**Interfaces:**

- Consumes: `ListedImage`, `dockerListImages`, `dockerRemoveImage` (Task 7), `containerNameOf` (Task 9), `pruneAudit` (Task 10).
- Produces: `keepRefs`, `imagesToPrune`, `KEEP_RECENT_DEPLOYMENTS`.

- [ ] **Step 1: Write the tests**

**Create `tests/prune.test.ts`:**

````typescript
import assert from "node:assert/strict";
import { test } from "node:test";
import { imagesToPrune, keepRefs } from "../src/lib/prune.js";

const img = (repository: string, tag: string, digest = "<none>", id = "id0000000000") => ({ repository, tag, digest, id });

test("the running image, the rollback image and the few most recent deployments are kept", () => {
  const keep = keepRefs({
    projects: [{ id: "p", imageTag: "nexus/a:cur", previousImageTag: "nexus/a:prev" }],
    deployments: [
      { projectId: "p", imageTag: "nexus/a:d1", createdAt: 1 },
      { projectId: "p", imageTag: "nexus/a:d2", createdAt: 2 },
      { projectId: "p", imageTag: "nexus/a:d3", createdAt: 3 },
      { projectId: "p", imageTag: "nexus/a:d4", createdAt: 4 },
      { projectId: "p", imageTag: "", createdAt: 5 },
    ],
  });
  assert.deepEqual([...keep].sort(), ["nexus/a:cur", "nexus/a:d2", "nexus/a:d3", "nexus/a:d4", "nexus/a:prev"]);
});

test("another project's deployments do not keep this one's images alive, nor the reverse", () => {
  const keep = keepRefs({
    projects: [{ id: "a" }, { id: "b" }],
    deployments: [
      { projectId: "a", imageTag: "nexus/a:1", createdAt: 1 },
      { projectId: "b", imageTag: "nexus/b:1", createdAt: 2 },
    ],
  }, 1);
  assert.deepEqual([...keep].sort(), ["nexus/a:1", "nexus/b:1"]);
});

test("unreferenced images are pruned and referenced ones are not", () => {
  const listed = [img("nexus/a", "cur"), img("nexus/a", "old"), img("nginx", "1.27")];
  assert.deepEqual(imagesToPrune(listed, new Set(["nexus/a:cur"])), ["nexus/a:old", "nginx:1.27"]);
});

test("a pulled image recorded by digest is kept even though it is listed by tag", () => {
  const digest = `sha256:${"d".repeat(64)}`;
  const listed = [img("nginx", "1.27", digest), img("nginx", "1.26", `sha256:${"e".repeat(64)}`)];
  assert.deepEqual(imagesToPrune(listed, new Set([`nginx@${digest}`])), ["nginx:1.26"]);
});

test("untagged images are pruned by id", () => {
  assert.deepEqual(imagesToPrune([img("<none>", "<none>", "<none>", "abc123def456")], new Set()), ["abc123def456"]);
});

test("an empty node prunes nothing", () => {
  assert.deepEqual(imagesToPrune([], new Set()), []);
});
````

- [ ] **Step 2: Run them and watch them fail**

```bash
cd apps/Nexus-Deploy
node --import tsx --test tests/prune.test.ts
```

Expected: FAIL — `Cannot find module '../src/lib/prune.js'`.

- [ ] **Step 3: Implement**

**Create `src/lib/prune.ts`:**

````typescript
/**
 * Which images the hourly sweep may delete.
 *
 * The sandbox daemon belongs to Deploy alone, so every image in it is ours and
 * "unused" is the only question. An image is kept if a project is running it, if
 * it is the release a rollback would restore, or if it is among a project's few
 * most recent deployments. A pulled image is listed by tag but recorded by
 * digest, so both spellings are checked.
 */
import type { ListedImage } from "./docker.js";

/** How many recent deployments per project keep their image, for rollback. */
export const KEEP_RECENT_DEPLOYMENTS = 3;

export function keepRefs(
  data: {
    projects: ReadonlyArray<{ id: string; imageTag?: string | undefined; previousImageTag?: string | undefined }>;
    deployments: ReadonlyArray<{ projectId: string; imageTag: string; createdAt: number }>;
  },
  recent: number = KEEP_RECENT_DEPLOYMENTS,
): Set<string> {
  const keep = new Set<string>();
  for (const project of data.projects) {
    if (project.imageTag) keep.add(project.imageTag);
    if (project.previousImageTag) keep.add(project.previousImageTag);
    data.deployments
      .filter((d) => d.projectId === project.id && d.imageTag)
      .sort((a, b) => b.createdAt - a.createdAt)
      .slice(0, recent)
      .forEach((d) => keep.add(d.imageTag));
  }
  return keep;
}

/** References to pass to `docker rmi`: `repo:tag` for tagged images, the id for untagged ones. */
export function imagesToPrune(listed: readonly ListedImage[], keep: ReadonlySet<string>): string[] {
  const doomed: string[] = [];
  for (const image of listed) {
    const tagged = image.repository !== "<none>" && image.tag !== "<none>";
    const ref = tagged ? `${image.repository}:${image.tag}` : image.id;
    const byDigest = image.digest !== "<none>" ? `${image.repository}@${image.digest}` : null;
    if (keep.has(ref) || (byDigest !== null && keep.has(byDigest))) continue;
    doomed.push(ref);
  }
  return doomed;
}
````

**Modify `src/lib/status-sync.ts`:**

````diff
--- a/src/lib/status-sync.ts
+++ b/src/lib/status-sync.ts
@@ -3,10 +3,13 @@
  * status in the DB when reality diverges from what we have stored.
  * Also prunes old nexus Docker images once per hour.
  */
-import { writeDb, loadDb } from "./store.js";
-import { dockerStatus, spawnStream } from "./docker.js";
+import { pruneAudit } from "./audit.js";
 import { isBuilding } from "./build.js";
+import { containerNameOf } from "./containers.js";
+import { dockerListImages, dockerRemoveImage, dockerStatus } from "./docker.js";
 import { emitStatusChange } from "./events.js";
+import { imagesToPrune, keepRefs } from "./prune.js";
+import { loadDb, writeDb } from "./store.js";
 
 const DOCKER_TO_PROJECT: Record<string, string> = {
   running: "live",
@@ -22,7 +25,7 @@ async function syncOnce() {
   const toSync = db.projects.filter(p => p.containerId && !isBuilding(p.id));
 
   for (const project of toSync) {
-    const dockerState = await dockerStatus(`nexus-app-${project.name}`);
+    const dockerState = await dockerStatus(containerNameOf(project));
     const expected = DOCKER_TO_PROJECT[dockerState] ?? "stopped";
 
     if (project.status !== expected) {
@@ -36,27 +39,18 @@ async function syncOnce() {
   }
 }
 
-// ── Image pruning ──────────────────────────────────────────────────────────
-async function pruneImages() {
+// ── Housekeeping ───────────────────────────────────────────────────────────
+// Images nothing needs, and audit entries past their thirty days. Docker itself
+// refuses to remove an image a container still uses, so this never forces.
+async function housekeeping() {
   const db = loadDb();
-  const activeImages = new Set(db.projects.map(p => p.imageTag).filter(Boolean));
-
-  const images: string[] = [];
-  await spawnStream(
-    "docker",
-    ["images", "--format", "{{.Repository}}:{{.Tag}}", "--filter", "reference=nexus/*"],
-    line => images.push(line.trim())
-  ).catch(() => {});
-
+  const doomed = imagesToPrune(await dockerListImages(), keepRefs(db));
   let pruned = 0;
-  for (const image of images) {
-    if (!activeImages.has(image)) {
-      await spawnStream("docker", ["rmi", "-f", image], () => {}).catch(() => {});
-      pruned++;
-    }
-  }
-
+  for (const ref of doomed) if (await dockerRemoveImage(ref)) pruned++;
   if (pruned > 0) console.log(`[image-prune] Removed ${pruned} unused image(s)`);
+  await writeDb((d) => {
+    pruneAudit(d.audit, Date.now());
+  });
 }
 
 // ── Boot ───────────────────────────────────────────────────────────────────
@@ -65,8 +59,8 @@ export function startStatusSync(intervalMs = 30_000) {
   const syncInterval = setInterval(() => syncOnce().catch(console.error), intervalMs);
 
   // Prune once after 5 min, then every hour
-  const firstPrune = setTimeout(() => pruneImages().catch(console.error), 5 * 60_000);
-  const pruneInterval = setInterval(() => pruneImages().catch(console.error), 60 * 60_000);
+  const firstPrune = setTimeout(() => housekeeping().catch(console.error), 5 * 60_000);
+  const pruneInterval = setInterval(() => housekeeping().catch(console.error), 60 * 60_000);
 
   return () => {
     clearTimeout(firstSync); clearInterval(syncInterval);
````

- [ ] **Step 4: Run the checks**

```bash
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **202 tests, all passing**.

- [ ] **Step 5: Commit**

```bash
cd apps/Nexus-Deploy
git add src/lib/prune.ts src/lib/status-sync.ts tests/prune.test.ts
git commit \
  -m "feat(prune): keep rollback targets, prune by reference, and follow per-release container names" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 15: New code is lint-clean

The repository already carries a lint backlog (41 errors before this plan), which is not part of any slice. The rule for this plan is that **everything it adds or rewrites passes Biome**. Biome's formatter does most of the work; four files need small hand fixes afterwards, shown as an exact diff against the formatter's output so the result is the same on every machine.

**Files:**

- Format: `src/app.ts`
- Format: `src/lib/audit.ts`
- Format: `src/lib/build-steps.ts`
- Format: `src/lib/containers.ts`
- Format: `src/lib/docker-runtime.ts`
- Format: `src/lib/env.ts`
- Format: `src/lib/guard.ts`
- Format: `src/lib/hosts.ts`
- Format: `src/lib/members.ts`
- Format: `src/lib/names.ts`
- Format: `src/lib/ports.ts`
- Format: `src/lib/project-input.ts`
- Format: `src/lib/prune.ts`
- Format: `src/lib/publisher.ts`
- Format: `src/lib/release.ts`
- Format: `src/lib/run-args.ts`
- Format: `src/lib/safeguards.ts`
- Format: `src/lib/source.ts`
- Format: `src/lib/urls.ts`
- Format: `src/lib/wiring.ts`
- Format: `src/lib/ws.ts`
- Format: `src/routes/ai.ts`
- Format: `src/routes/basic.ts`
- Format: `src/routes/context.ts`
- Format: `src/routes/lifecycle.ts`
- Format: `src/routes/projects.ts`
- Format: `src/routes/types.ts`
- Format: `src/routes/webhook.ts`
- Format: `tests/audit.test.ts`
- Format: `tests/deploy-flow.test.ts`
- Format: `tests/guard.test.ts`
- Format: `tests/hosts.test.ts`
- Format: `tests/members.test.ts`
- Format: `tests/names.test.ts`
- Format: `tests/no-hardcoded-domain.test.ts`
- Format: `tests/ports.test.ts`
- Format: `tests/project-input.test.ts`
- Format: `tests/prune.test.ts`
- Format: `tests/release.test.ts`
- Format: `tests/routes-lifecycle.test.ts`
- Format: `tests/routes-projects.test.ts`
- Format: `tests/run-args.test.ts`
- Format: `tests/safeguards.test.ts`
- Format: `tests/sandbox.test.ts`
- Format: `tests/source.test.ts`
- Format: `tests/support/app-harness.ts`
- Format: `tests/support/fakes.ts`
- Format: `tests/webhook.test.ts`
- Format: `tests/ws.test.ts`
- Format: `src/lib/build.ts` (rewritten by this plan)
- Format: `src/index.ts` (rewritten by this plan)
- Format: `src/middleware/auth.ts` (rewritten by this plan)

- [ ] **Step 1: Format the new and rewritten files**

```bash
cd apps/Nexus-Deploy
mkdir -p /tmp/nexus-deploy-tools && cp node_modules/@biomejs/cli-linux-x64/biome /tmp/nexus-deploy-tools/biome && chmod +x /tmp/nexus-deploy-tools/biome
/tmp/nexus-deploy-tools/biome check --write \
  src/app.ts src/lib/audit.ts src/lib/build-steps.ts src/lib/containers.ts src/lib/docker-runtime.ts src/lib/env.ts src/lib/guard.ts src/lib/hosts.ts src/lib/members.ts src/lib/names.ts src/lib/ports.ts src/lib/project-input.ts src/lib/prune.ts src/lib/publisher.ts src/lib/release.ts src/lib/run-args.ts src/lib/safeguards.ts src/lib/source.ts src/lib/urls.ts src/lib/wiring.ts src/lib/ws.ts src/routes/ai.ts src/routes/basic.ts src/routes/context.ts src/routes/lifecycle.ts src/routes/projects.ts src/routes/types.ts src/routes/webhook.ts tests/audit.test.ts tests/deploy-flow.test.ts tests/guard.test.ts tests/hosts.test.ts tests/members.test.ts tests/names.test.ts tests/no-hardcoded-domain.test.ts tests/ports.test.ts tests/project-input.test.ts tests/prune.test.ts tests/release.test.ts tests/routes-lifecycle.test.ts tests/routes-projects.test.ts tests/run-args.test.ts tests/safeguards.test.ts tests/sandbox.test.ts tests/source.test.ts tests/support/app-harness.ts tests/support/fakes.ts tests/webhook.test.ts tests/ws.test.ts \
  src/lib/build.ts src/index.ts src/middleware/auth.ts
```

Expected: it reports a few remaining errors it cannot fix automatically. That is expected; Step 2 fixes them.

- [ ] **Step 2: Apply the hand fixes**

`forEach` becomes a `for…of` loop; the `delete`s become one documented `unset` helper (an absent field is what "cleared" means, and assigning `undefined` would leave an own key behind); the test helper's untyped JSON gets documented exceptions; `pendingPort` is reset by assignment.

**Modify `src/lib/build.ts`:**

````diff
--- a/src/lib/build.ts
+++ b/src/lib/build.ts
@@ -88,7 +88,7 @@ export function subscribeToDeployment(id: string, fn: Subscriber): () => void {
 }
 
 function broadcast(id: string, line: string) {
-  subscribers.get(id)?.forEach((fn) => fn(line));
+  for (const fn of subscribers.get(id) ?? []) fn(line);
 }
 
 // ── Per-project build lock ─────────────────────────────────────────────────
@@ -306,7 +306,7 @@ async function release(
   } finally {
     await writeDb((db) => {
       const p = db.projects.find((p) => p.id === project.id);
-      if (p) delete p.pendingPort;
+      if (p) p.pendingPort = undefined;
     });
   }
 }
````

**Modify `src/lib/project-input.ts`:**

````diff
--- a/src/lib/project-input.ts
+++ b/src/lib/project-input.ts
@@ -216,6 +216,14 @@ export function parsePatch(
   return patch;
 }
 
+type ClearableField = "dockerfile" | "healthPath" | "memoryLimit" | "cpus" | "notifyUrl";
+
+function unset(project: Project, field: ClearableField): void {
+  // An absent field is what "cleared" means. Assigning undefined would leave an
+  // own key behind, which later code (and the tests) would read as "present".
+  delete project[field];
+}
+
 /** Apply a validated patch to a stored project, keeping the display copy of the source in step. */
 export function applyPatch(project: Project, patch: ProjectPatch, now: number): void {
   if (patch.source) {
@@ -224,19 +232,19 @@ export function applyPatch(project: Project, patch: ProjectPatch, now: number):
     project.branch = patch.source.type === "git" ? patch.source.branch : "";
   }
   if (patch.buildContext !== undefined) project.buildContext = patch.buildContext;
-  if (patch.dockerfile === null) delete project.dockerfile;
+  if (patch.dockerfile === null) unset(project, "dockerfile");
   else if (patch.dockerfile !== undefined) project.dockerfile = patch.dockerfile;
-  if (patch.healthPath === null) delete project.healthPath;
+  if (patch.healthPath === null) unset(project, "healthPath");
   else if (patch.healthPath !== undefined) project.healthPath = patch.healthPath;
   if (patch.buildCommand !== undefined) project.buildCommand = patch.buildCommand;
   if (patch.startCommand !== undefined) project.startCommand = patch.startCommand;
   if (patch.volumePath !== undefined) project.volumePath = patch.volumePath;
   if (patch.port !== undefined) project.port = patch.port;
-  if (patch.memoryLimit === null) delete project.memoryLimit;
+  if (patch.memoryLimit === null) unset(project, "memoryLimit");
   else if (patch.memoryLimit !== undefined) project.memoryLimit = patch.memoryLimit;
-  if (patch.cpus === null) delete project.cpus;
+  if (patch.cpus === null) unset(project, "cpus");
   else if (patch.cpus !== undefined) project.cpus = patch.cpus;
-  if (patch.notifyUrl === null) delete project.notifyUrl;
+  if (patch.notifyUrl === null) unset(project, "notifyUrl");
   else if (patch.notifyUrl !== undefined) project.notifyUrl = patch.notifyUrl;
   if (patch.autoDeployEnabled !== undefined) project.autoDeployEnabled = patch.autoDeployEnabled;
   project.updatedAt = now;
````

**Modify `src/lib/prune.ts`:**

````diff
--- a/src/lib/prune.ts
+++ b/src/lib/prune.ts
@@ -27,11 +27,11 @@ export function keepRefs(
   for (const project of data.projects) {
     if (project.imageTag) keep.add(project.imageTag);
     if (project.previousImageTag) keep.add(project.previousImageTag);
-    data.deployments
+    const recentDeployments = data.deployments
       .filter((d) => d.projectId === project.id && d.imageTag)
       .sort((a, b) => b.createdAt - a.createdAt)
-      .slice(0, recent)
-      .forEach((d) => keep.add(d.imageTag));
+      .slice(0, recent);
+    for (const deployment of recentDeployments) keep.add(deployment.imageTag);
   }
   return keep;
 }
````

**Modify `tests/support/app-harness.ts`:**

````diff
--- a/tests/support/app-harness.ts
+++ b/tests/support/app-harness.ts
@@ -186,6 +186,7 @@ export type CallOptions = {
   rawBody?: string;
   headers?: Record<string, string>;
 };
+// biome-ignore lint/suspicious/noExplicitAny: a test helper that returns parsed JSON of any shape
 export type CallResult = { status: number; body: any; headers: Headers };
 
 export async function startHarness(patch: Partial<AppDeps> = {}) {
@@ -235,6 +236,7 @@ export async function startHarness(patch: Partial<AppDeps> = {}) {
       ...(body !== undefined ? { body } : {}),
     });
     const text = await response.text();
+    // biome-ignore lint/suspicious/noExplicitAny: parsed JSON of any shape
     let parsed: any = text;
     try {
       parsed = text ? JSON.parse(text) : null;
````

- [ ] **Step 3: Verify**

```bash
/tmp/nexus-deploy-tools/biome check \
  src/app.ts src/lib/audit.ts src/lib/build-steps.ts src/lib/containers.ts src/lib/docker-runtime.ts src/lib/env.ts src/lib/guard.ts src/lib/hosts.ts src/lib/members.ts src/lib/names.ts src/lib/ports.ts src/lib/project-input.ts src/lib/prune.ts src/lib/publisher.ts src/lib/release.ts src/lib/run-args.ts src/lib/safeguards.ts src/lib/source.ts src/lib/urls.ts src/lib/wiring.ts src/lib/ws.ts src/routes/ai.ts src/routes/basic.ts src/routes/context.ts src/routes/lifecycle.ts src/routes/projects.ts src/routes/types.ts src/routes/webhook.ts tests/audit.test.ts tests/deploy-flow.test.ts tests/guard.test.ts tests/hosts.test.ts tests/members.test.ts tests/names.test.ts tests/no-hardcoded-domain.test.ts tests/ports.test.ts tests/project-input.test.ts tests/prune.test.ts tests/release.test.ts tests/routes-lifecycle.test.ts tests/routes-projects.test.ts tests/run-args.test.ts tests/safeguards.test.ts tests/sandbox.test.ts tests/source.test.ts tests/support/app-harness.ts tests/support/fakes.ts tests/webhook.test.ts tests/ws.test.ts \
  src/lib/build.ts src/index.ts src/middleware/auth.ts
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: Biome reports no errors for these files; no type errors; **202 tests, all passing**.

- [ ] **Step 4: Commit**

```bash
cd apps/Nexus-Deploy
git add -A src tests
git commit \
  -m "style: new code is lint-clean" \
  -m "Everything added or rewritten by the shared-node work passes Biome." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
```

### Task 16: Correct the licence and document the shared-node design

`README.md` says MIT while `LICENSE` is the GNU AGPL-3.0, which is what the Charter requires for applications and services. The README is wrong and is corrected. The docs, example configuration and compose file also still describe the Traefik-labelled design this plan replaces; they are updated so they are true of this release, and the compose file is marked unsupported rather than silently misleading. This task has no new behaviour, so it has no new tests.

**Files:**

- Modify: `.env.example`
- Modify: `AGENTS.md`
- Modify: `README.md`
- Modify: `docker-compose.yml`
- Modify: `package.json`
- Modify: `src/lib/config.ts`

- [ ] **Step 1: Apply the documentation and configuration changes**

**Modify `.env.example`:**

````diff
--- a/.env.example
+++ b/.env.example
@@ -22,11 +22,38 @@ APP_SECRET=change_me_32_chars_minimum
 ADMIN_EMAIL=admin@yourdomain.com
 ADMIN_PASSWORD=change_me_on_first_login
 
-# Docker network name (must match docker-compose.yml)
-DOCKER_NETWORK=nexus-net
-
 # Allow new user registration after admin account is created
 ALLOW_REGISTRATION=false
 
 # Port for the API server (Traefik handles external routing)
 PORT=3000
+
+# === Node-specific values (nothing in the code names a domain) ===
+
+# Where the dashboard, the API and the short links (<CONTROL_HOST>/<app>) live
+CONTROL_HOST=deploy.yourdomain.com
+
+# Where one app is reachable. {slug} and {BASE_DOMAIN} are substituted.
+APP_HOST_PATTERN={slug}.{BASE_DOMAIN}
+
+# Loopback ports apps are published on (the ingress reaches an app only here)
+APP_PORT_RANGE=20000-29999
+
+# The rootless sandbox Docker daemon. REQUIRED: with this empty, builds and
+# containers refuse to run rather than fall back to the host's Docker.
+SANDBOX_DOCKER_HOST=unix:///run/user/1001/docker.sock
+
+# Mount point of the sandbox's size-capped storage (builds pause when it fills)
+SANDBOX_STORAGE_PATH=/var/lib/nexus-deploy-sandbox
+
+# === Safeguards (the same for everyone; all optional) ===
+# CAP_MEMORY_MB=512
+# CAP_CPUS=0.5
+# CAP_PIDS=256
+# CAP_MAX_PROJECTS=5
+# CAP_MAX_CONCURRENT_BUILDS=1
+# CAP_MAX_BUILDS_PER_HOUR=6
+# CAP_BUILD_TIMEOUT_MS=600000
+# CAP_PULL_TIMEOUT_MS=600000
+# CAP_MIN_HOST_FREE_PERCENT=15
+# CAP_MAX_SANDBOX_USED_PERCENT=85
````

**Modify `AGENTS.md`:**

````diff
--- a/AGENTS.md
+++ b/AGENTS.md
@@ -1,8 +1,7 @@
 # Nexus Deploy — Agent Guide
 
 ## Purpose
-Self-hosted Railway-style deployment platform by The No Hands Company.
-Push to GitHub → auto-build → live HTTPS app. Free forever, no billing, ever.
+A free, open-source alternative to paid deployment platforms (Railway, Render) by The No Hands Company. Deploy a Git repo or a container image and get a live HTTPS app. A general service for anyone: no Nexus app is special to it.
 
 ## Architecture
 ```
@@ -68,6 +67,19 @@ Token accepted as:
 - `Authorization: Bearer <token>` — for all API calls
 - `?token=<token>` — for SSE (EventSource) and WebSocket (browser limitation)
 
+## How it fits together (v0.6)
+- `src/app.ts` builds the HTTP app from injected dependencies (`AppDeps`); `src/index.ts` only wires the real ones and listens. Tests inject fakes, so nothing needs Nexus-Auth, Docker or Git.
+- **Members and ownership** (`lib/members.ts`): Nexus-Auth proves identity; an operator (Auth founder/admin) lets accounts in. Every project has an `ownerId`; another member's project answers 404, exactly like a missing one. Operators see all.
+- **Sources** (`lib/source.ts`): a project has one source, Git or image. A private-repo token is handed to git by an askpass helper, never put in a URL, argument, log or API response.
+- **Pipeline** (`lib/build.ts`): clone/build or pull, then `lib/release.ts` starts the new container on its own loopback port, proves it healthy, tells the ingress through a `RoutePublisher`, then stops the old one (blue/green). A project with a data volume releases stop-then-start instead.
+- **Sandbox**: every `docker`/`nixpacks` call is forced to `SANDBOX_DOCKER_HOST` and refuses to run without it. Never fall back to the host daemon. App containers are started only through `lib/run-args.ts`, which drops all capabilities and caps CPU, memory, processes and logs.
+- **Privacy**: no visitor address is read or stored anywhere (scripts/check-privacy.sh in the monorepo enforces it). The audit log records account, action and time only, for thirty days.
+- Node-specific values (`BASE_DOMAIN`, `APP_HOST_PATTERN`, `CONTROL_HOST`, `SANDBOX_*`, `APP_PORT_RANGE`, `CAP_*`) are configuration; no source file may name a domain (a test enforces it).
+
+## Testing
+- `node --import tsx --test tests/*.test.ts` runs everything; `npm run typecheck` and `npm run typecheck:tests` must be clean. New code must also pass `biome check` (the older files still have a lint backlog, which is not part of any slice).
+- On a filesystem that cannot execute binaries, point `ESBUILD_BINARY_PATH` at an executable copy of esbuild.
+
 ## Conventions
 - Keep it lean. No ORM, no message queue, no microservices.
 - All DB writes go through `writeDb(mutate)` — never call `saveDb` during builds.
@@ -77,7 +89,7 @@ Token accepted as:
 - Image naming: `nexus/{project-name}:{8-char-deployment-id}`
 - Container naming: `nexus-app-{project-name}`
 - Volume naming: `nexus-vol-{project-name}` (mounted at project.volumePath)
-- Never add billing, limits, or feature gates. Free forever.
+- Never add billing, plans or paywalls. Free forever. Safeguards that protect a shared node (container caps, build limits, the disk guard) are allowed and apply equally to everyone; an operator tunes them for the whole node with CAP_* variables.
 
 ## Standards Enforcement
 - Follow ../docs/ENGINEERING_STANDARDS.md as the baseline for TypeScript quality, API behavior, observability, and security.
````

**Modify `README.md`:**

````diff
--- a/README.md
+++ b/README.md
@@ -2,13 +2,13 @@
 
 **Self-hosted deployment platform by [The No Hands Company](https://github.com/The-No-Hands-company)**
 
-Push to GitHub → auto-build → live HTTPS app. Your own Railway, on your own server.
+Deploy a Git repository or a container image and get a live HTTPS app. A free, open-source alternative to paid deployment platforms such as Railway and Render, for anyone to run or use.
 
 ---
 
 ## 💸 Pricing
 
-**Free. Always. No exceptions.**
+**Free. Always. No exceptions.** Nexus Deploy is part of [The No Hands Company](https://github.com/The-No-Hands-company)'s promise that nothing it makes is sold or put behind a paywall. The software contains no billing or plans. Safeguards that protect a shared node (CPU, memory and build limits) apply equally to everyone and are shown in the dashboard. Anyone running their own node may charge for it; the licence allows that.
 
 Nexus Deploy is and will always be 100% free. The No Hands Company does not bill users for any of its products. [Donations welcome](https://github.com/The-No-Hands-company) — never required.
 
@@ -30,6 +30,10 @@ Nexus Deploy is and will always be 100% free. The No Hands Company does not bill
 
 ---
 
+## 📌 Status of this release (v0.6)
+
+This release turns Nexus Deploy into something a node can share between invited members, safely. It adds members and per-user ownership, two kinds of source (Git and Docker image), private repositories, blue/green releases, safeguards, a sandboxed Docker daemon requirement and a route-publishing interface. Three things come in following releases: the sandbox **setup script**, the **ingress integration** that makes apps reachable (the reference node uses the Nexus ecosystem proxy and Cloud's registry), and the **conformance check** (`doctor` / `verify`). Until then the Traefik-and-Let's-Encrypt compose file below is **not supported**: app containers are no longer labelled for Traefik.
+
 ## 🏗️ Architecture
 
 ```
@@ -168,7 +172,7 @@ The app stores data in `DATA_DIR` (default `/workspace`). In dev it creates `./w
 
 ## 📄 License
 
-MIT — fork it, own it, run it.
+[GNU AGPL-3.0](LICENSE). Fork it, change it, run it; if you offer a changed version to others as a service, share your changes too. (This file used to say MIT, which was wrong: the repository has always carried the AGPL.)
 
 ---
````

**Modify `docker-compose.yml`:**

````diff
--- a/docker-compose.yml
+++ b/docker-compose.yml
@@ -1,3 +1,9 @@
+# NOTE (v0.6): this Traefik-and-Let's-Encrypt stack is no longer supported as shipped.
+# App containers are no longer labelled for Traefik; apps are published on loopback
+# ports and announced through a route publisher instead, and builds run in a
+# separate rootless sandbox daemon (SANDBOX_DOCKER_HOST). A maintained self-host
+# guide arrives with the sandbox setup script. Kept for reference only.
+#
 # Nexus Deploy — full production stack
 # Traefik handles HTTPS + routing. nexus-deploy handles builds + dashboard.
 #
````

**Modify `package.json`:**

````diff
--- a/package.json
+++ b/package.json
@@ -1,7 +1,7 @@
 {
   "name": "nexus-deploy",
   "private": true,
-  "version": "0.5.0",
+  "version": "0.6.0",
   "type": "module",
   "scripts": {
     "dev": "concurrently -k \"npm run dev:server\" \"npm run dev:web\"",
````

**Modify `src/lib/config.ts`:**

````diff
--- a/src/lib/config.ts
+++ b/src/lib/config.ts
@@ -56,7 +56,6 @@ export const config = {
   nexusAuthPublicUrl:
     process.env.NEXUS_AUTH_PUBLIC_URL ?? process.env.NEXUS_AUTH_URL ?? "http://localhost:4310",
   webhookSecret: process.env.WEBHOOK_SECRET ?? "",
-  dockerNetwork: process.env.DOCKER_NETWORK ?? "nexus-net",
   // file: repositories clone off this machine's disk; off unless a test harness
   // asks for them. See src/lib/repo.ts.
   allowLocalRepos: (process.env.DEPLOY_ALLOW_LOCAL_REPOS ?? "false") === "true",
````

- [ ] **Step 2: Verify nothing broke**

```bash
cd apps/Nexus-Deploy
node node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node node_modules/typescript/bin/tsc -p tsconfig.test.json
node --import tsx --test tests/*.test.ts
```

Expected: no type errors; **202 tests, all passing**.

- [ ] **Step 3: Run the repository's privacy guard**

The Charter promises that no visitor address is stored. The monorepo has a guard that fails if any non-test source reads an address header or a socket's remote address. Deploy must pass it; the new code never touches either.

```bash
cd ../..
bash scripts/check-privacy.sh 2>&1 | tee /tmp/guard.out
grep -c Nexus-Deploy /tmp/guard.out
```

Expected: the `grep -c` prints `0`, meaning the guard names nothing under `apps/Nexus-Deploy`. The guard may still fail for other apps: at the time this plan was written it reported `apps/Nexus-API/` (excluded but started by `deploy.sh`) and an address read in `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_service.rs`. Those are not Deploy's and are out of scope here; do not edit them in this task.

- [ ] **Step 4: Commit, push, and bump the pointer in the monorepo**

```bash
cd apps/Nexus-Deploy
git add README.md AGENTS.md .env.example docker-compose.yml package.json src/lib/config.ts
git commit \
  -m "docs: correct the licence and describe the shared-node design" \
  -m "README said MIT while LICENSE is AGPL-3.0. The docs and example configuration
now match this release; the Traefik compose file is marked unsupported." \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
git push origin main
cd ../..
git add apps/Nexus-Deploy
git commit \
  -m "chore: bump Nexus-Deploy to the shared-node control plane" \
  -m "Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>"
git push origin main
```
