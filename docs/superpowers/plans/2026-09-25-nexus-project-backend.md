# Nexus Project Backend Implementation Plan (Plan 1 of 2)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the non-working `apps/Nexus-Project` scaffold with a tested backend: a pure critical-path scheduling engine, a SQLite store for workspaces, projects, statuses, a WBS of tasks and dependencies, and an authenticated JSON API enforcing roles.

**Architecture:** Bun + strict TypeScript service on `127.0.0.1:3152`. `src/schedule/` is a pure module (no database, clock or I/O) that Plan 2's frontend will also bundle. `src/store/` holds data access plus domain rules and throws `HttpError`s; `src/routes/` parses and validates requests and calls the store. Identity follows Nexus-Calendar exactly (`@nexus/identity` token or Dashboard hop secret).

**Tech Stack:** Bun (runtime, `bun:sqlite`, `bun:test`), TypeScript 5.9.3, `@nexus/identity` (in-repo package), Biome 1.9.4.

**Spec:** `docs/superpowers/specs/2026-09-25-nexus-project-design.md`

**Plan 2 (not in this plan):** frontend views, Dashboard proxied-app factory, Caddy front door on port **8093**, deploy.sh registration, DNS. Written after this plan lands.

## Global Constraints

- All work on `main` (the user is a solo developer and uses no branches). Commit after every task; end every commit message with `Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>`.
- Touch nothing outside `apps/Nexus-Project/` and `docs/` in this plan.
- Service name `nexus-project`, port `3152`, bind `NEXUS_BIND_HOST || "127.0.0.1"`. Env: `NEXUS_PROJECT_DB` (default `data/project.sqlite`), `NEXUS_PROJECT_DASHBOARD_SECRET`, `NEXUS_PROJECT_JWT_AUDIENCE` (default `project.tnhc.dev`), `NEXUS_PROJECT_BASE_URL`, `NEXUS_PROJECT_ENABLE_CLOUD_INTEGRATION`, `NEXUS_PROJECT_CLOUD_HEARTBEAT_INTERVAL_MS`, `NEXUS_AUTH_INTERNAL_URL`, `NEXUS_CLOUD_URL`, `NEXUS_CLOUD_API_KEY`.
- No runtime dependencies beyond Bun built-ins and `node:*`. Dev dependencies pinned exactly: `typescript` `5.9.3`, `bun-types` `1.3.14`, `@biomejs/biome` `1.9.4`.
- Install with `npm install --no-audit --no-fund` inside `apps/Nexus-Project`. **Never `bun install`** — it hangs in this monorepo. **Never `bunx`** — it tries to download; use `./node_modules/.bin/tsc`.
- API prefix `/api/v1/project`. Error envelope `{ "error": "<code>", "message": "<text>" }` (plus documented extras). Every response sends `cache-control: no-store`, `referrer-policy: no-referrer`, `x-content-type-options: nosniff`.
- Limits: task title ≤ 512 chars, description ≤ 50,000, every name ≤ 200. Unknown body fields → 400.
- Dates are `YYYY-MM-DD`, real calendar dates, years 2000–2199. Scheduling granularity is whole working days.
- A task occupies the half-open working-day interval `[ES, EF)`; its displayed finish is the last working day of that interval. A milestone (duration 0) is shown on the working day at whose start it occurs, so a milestone driven by a task that finishes on a Friday shows the following Monday. (Plan 2's Gantt draws it on the day boundary.)
- Roles, lowest to highest: `viewer`, `member`, `admin`, `owner`. Cannot see → 404; can see but role too low → 403.
- Tests assert defining properties, not output counts. `check.sh` fails if the number of test files bun ran differs from the number on disk.

## File Structure

```
apps/Nexus-Project/
  package.json, package-lock.json, tsconfig.json, biome.json, .gitignore, check.sh, README.md
  src/
    index.ts            process entry: start server, close on SIGTERM/SIGINT
    server.ts           Bun.serve, health/status, auth gate, router dispatch, error mapping
    http.ts             json(), HttpError and helpers, readJson()
    router.ts           tiny method + path-pattern router
    auth.ts             resolveCaller(): identity token or Dashboard hop
    contracts.ts        Cloud registration payload
    cloud.ts            Cloud registration + heartbeat
    access.ts           roles, workspace/project/task visibility checks
    validation.ts       strict body parsing helpers
    rank.ts             fractional ranks for ordering
    schedule/
      types.ts          engine input/output types
      calendar.ts       dates <-> day numbers, working-day calendar
      graph.ts          topological order, cycle finding
      engine.ts         schedule(): forward/backward pass, float, violations, roll-up
    store/
      db.ts             open, migrate, transaction, now()
      rows.ts           row types for every table
      queries.ts        small shared queries (hasChildren, hasLinks, statusInProject)
      workspaces.ts     workspaces and members
      projects.ts       projects, project members, move
      statuses.ts       statuses
      scheduling.ts     bridge between store and engine; auto-mode write-back
      tasks.ts          tasks, WBS, ranks, versions
      dependencies.ts   links
      calendars.ts      project calendar
    routes/
      workspaces.ts projects.ts statuses.ts tasks.ts dependencies.ts calendars.ts schedule.ts
  tests/
    support/server.ts   test server + authenticated client
    support/random.ts   seeded PRNG
    support/schedule.ts engine fixtures
    *.test.ts
```

---

### Task 1: Replace the scaffold with a service skeleton

The existing untracked files (`src/*.ts`, `src/project-engine.js`, `tests/server.test.ts`, `package.json`) are the broken scaffold the spec replaces. Nothing in them is kept.

**Files:**
- Delete: `apps/Nexus-Project/src/cloud.ts`, `src/contracts.ts`, `src/index.ts`, `src/project-engine.js`, `src/project-engine.ts`, `src/server.ts`, `tests/server.test.ts`, `package.json`
- Create: `apps/Nexus-Project/package.json`, `tsconfig.json`, `biome.json`, `.gitignore`, `check.sh`
- Create: `src/http.ts`, `src/router.ts`, `src/contracts.ts`, `src/cloud.ts`, `src/server.ts`, `src/index.ts`
- Test: `tests/support/server.ts`, `tests/server.test.ts`, `tests/router.test.ts`, `tests/contracts.test.ts`

**Interfaces:**
- Produces: `HttpError(status, code, message, extra?)`, `json(body, status?)`, `errorResponse(err)`, `readJson(req)`, `notFound(what?)`, `forbidden()`, `badRequest(message)`, `unprocessable(code, message, extra?)`, `conflict(code, message, extra?)` from `src/http.ts`; `Router<C>` with `add(method, pattern, handler)` / `match(method, path)` and `param(params, name): string` from `src/router.ts`; `createServer()` returning `{ server, close }`; `API_PREFIX = "/api/v1/project"`; `startTestServer()` from `tests/support/server.ts`.

- [ ] **Step 1: Remove the scaffold and write the project files**

```bash
cd apps/Nexus-Project
rm -f src/cloud.ts src/contracts.ts src/index.ts src/project-engine.js src/project-engine.ts src/server.ts tests/server.test.ts package.json
mkdir -p src/schedule src/store src/routes tests/support
```

`apps/Nexus-Project/package.json`:

```json
{
  "name": "nexus-project",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "description": "Nexus project management: workspaces, WBS, dependencies and a critical-path scheduler",
  "scripts": {
    "dev": "bun run src/index.ts",
    "start": "bun run src/index.ts",
    "check": "tsc --noEmit",
    "test": "bun test tests/",
    "lint": "biome check src tests",
    "gate": "bash check.sh"
  },
  "devDependencies": {
    "@biomejs/biome": "1.9.4",
    "bun-types": "1.3.14",
    "typescript": "5.9.3"
  }
}
```

`apps/Nexus-Project/tsconfig.json`:

```json
{
  "compilerOptions": {
    "target": "ESNext",
    "lib": ["ESNext"],
    "module": "ESNext",
    "moduleResolution": "bundler",
    "types": ["bun-types"],
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "exactOptionalPropertyTypes": true,
    "noImplicitReturns": true,
    "noFallthroughCasesInSwitch": true,
    "skipLibCheck": true
  },
  "include": ["src", "tests"]
}
```

`apps/Nexus-Project/biome.json`:

```json
{
  "$schema": "https://biomejs.dev/schemas/1.9.4/schema.json",
  "formatter": { "indentStyle": "space", "lineWidth": 100 },
  "linter": { "enabled": true, "rules": { "recommended": true } }
}
```

`apps/Nexus-Project/.gitignore`:

```
node_modules/
data/
```

`apps/Nexus-Project/check.sh`:

```bash
#!/bin/bash
# Quality gate. Runs from any cwd; invoked identically by a developer and by CI.
set -euo pipefail
cd "$(dirname "$0")"
echo "nexus-project..."
./node_modules/.bin/tsc --noEmit
# A green total only covers files that imported. Compare the number of test
# files bun actually ran with the number on disk, so a file that fails to load
# cannot hide behind a passing count.
expected=$(find tests -name '*.test.ts' | wc -l | tr -d ' ')
output=$(bun test tests/ 2>&1) || { printf '%s\n' "$output"; exit 1; }
printf '%s\n' "$output" | tail -4
ran=$(printf '%s\n' "$output" | sed -n 's/^Ran [0-9]* tests\{0,1\} across \([0-9]*\) files\{0,1\}.*/\1/p')
if [ "$ran" != "$expected" ]; then
  echo "FAIL: bun ran $ran test files but $expected exist" >&2
  exit 1
fi
echo "PASS"
```

Then install:

```bash
cd apps/Nexus-Project && chmod +x check.sh && npm install --no-audit --no-fund
```

Expected: `added N packages`; `node_modules/.bin/tsc` exists.

- [ ] **Step 2: Write the failing tests**

`tests/support/server.ts`:

```ts
import { mock } from "bun:test";

// Cloud registration is fire-and-forget network traffic; tests never want it.
mock.module("../../src/cloud", () => ({ startHeartbeat: () => () => {} }));

export const TEST_SECRET = "project-test-hop-secret"; // pragma: allowlist secret

export interface ApiResponse<T = any> {
  status: number;
  body: T;
  headers: Headers;
}

export interface Client {
  call<T = any>(
    method: string,
    path: string,
    body?: unknown,
    headers?: Record<string, string>,
  ): Promise<ApiResponse<T>>;
}

/** Starts a server on a random port with a fresh in-memory database. */
export async function startTestServer() {
  process.env.NEXUS_PROJECT_DB = ":memory:";
  process.env.PORT = "0";
  process.env.NEXUS_PROJECT_DASHBOARD_SECRET = TEST_SECRET;
  const { createServer } = await import("../../src/server");
  const handle = await createServer();
  const base = `http://127.0.0.1:${handle.server.port}`;

  async function send(
    subject: string | null,
    method: string,
    path: string,
    body?: unknown,
    headers: Record<string, string> = {},
  ): Promise<ApiResponse> {
    const all: Record<string, string> = { ...headers };
    if (subject !== null) {
      all["x-nexus-subject"] = subject;
      all["x-nexus-dashboard-secret"] = TEST_SECRET;
    }
    if (body !== undefined) all["content-type"] = "application/json";
    const response = await fetch(`${base}${path}`, {
      method,
      headers: all,
      body: body === undefined ? null : JSON.stringify(body),
    });
    const text = await response.text();
    return { status: response.status, body: text ? JSON.parse(text) : null, headers: response.headers };
  }

  /** A client authenticated as `subject`; paths are relative to /api/v1/project. */
  const as = (subject: string): Client => ({
    call: (method, path, body, headers) => send(subject, method, `/api/v1/project${path}`, body, headers),
  });

  return { handle, base, as, raw: send, close: () => handle.close() };
}
```

`tests/router.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { Router } from "../src/router";

describe("Router", () => {
  const router = new Router<string>();
  router.add("GET", "/tasks/:id", (ctx, params) => new Response(`${ctx}:${params.id}`));
  router.add("PATCH", "/tasks/:id", () => new Response("patched"));
  router.add("GET", "/projects/:id/tasks", () => new Response("list"));

  it("matches a pattern and decodes its parameters", async () => {
    const match = router.match("GET", "/tasks/a%20b");
    expect(match && "handler" in match).toBe(true);
    if (!match || !("handler" in match)) return;
    expect(await match.handler("ctx", match.params).text()).toBe("ctx:a b");
  });

  it("reports the allowed methods when only the method is wrong", () => {
    expect(router.match("DELETE", "/tasks/1")).toEqual({ allowed: ["GET", "PATCH"] });
  });

  it("does not match a different shape", () => {
    expect(router.match("GET", "/tasks/1/extra")).toBeNull();
    expect(router.match("GET", "/projects/1")).toBeNull();
  });

  it("refuses a malformed percent-encoding instead of throwing", () => {
    expect(router.match("GET", "/tasks/%E0%A4%A")).toBeNull();
  });
});
```

`tests/contracts.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { buildSystemsApiRegistrationPayload } from "../src/contracts";

describe("Cloud registration", () => {
  it("declares proxied delivery and does not self-authorize", () => {
    const payload = buildSystemsApiRegistrationPayload("http://127.0.0.1:3152");
    expect(payload).toMatchObject({
      id: "nexus-project",
      upstreamUrl: "http://127.0.0.1:3152",
      path: "/project",
      publicUrl: "https://project.tnhc.dev",
      delivery: "proxied-app",
    });
    // Whether an app is behind the SSO gate is Cloud's operator-only switch.
    expect("requiresAuth" in payload).toBe(false);
  });
});
```

`tests/server.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { startTestServer } from "./support/server";

describe("nexus-project server", () => {
  let t: Awaited<ReturnType<typeof startTestServer>>;
  beforeAll(async () => {
    t = await startTestServer();
  });
  afterAll(() => t.close());

  it("answers health without identity", async () => {
    const res = await t.raw(null, "GET", "/health");
    expect(res.status).toBe(200);
    expect(res.body).toMatchObject({ service: "nexus-project", status: "ok" });
  });

  it("answers status without identity", async () => {
    const res = await t.raw(null, "GET", "/api/v1/status");
    expect(res.status).toBe(200);
    expect(res.body.capabilities).toEqual(["projects", "tasks", "scheduling"]);
  });

  it("sends the privacy headers on every response", async () => {
    const res = await t.raw(null, "GET", "/health");
    expect(res.headers.get("cache-control")).toBe("no-store");
    expect(res.headers.get("referrer-policy")).toBe("no-referrer");
    expect(res.headers.get("x-content-type-options")).toBe("nosniff");
  });

  it("returns the error envelope for an unknown path", async () => {
    const res = await t.raw(null, "GET", "/nope");
    expect(res.status).toBe(404);
    expect(res.body).toEqual({ error: "not_found", message: "resource not found" });
  });
});
```

- [ ] **Step 3: Run the tests to verify they fail**

Run: `cd apps/Nexus-Project && bun test tests/`
Expected: FAIL — `Cannot find module '../src/router'` (and the other missing modules).

- [ ] **Step 4: Write the implementation**

`src/http.ts`:

```ts
/** The one error shape every route returns. Thrown anywhere, mapped by server.ts. */
export class HttpError extends Error {
  constructor(
    readonly status: number,
    readonly code: string,
    message: string,
    readonly extra: Record<string, unknown> = {},
  ) {
    super(message);
  }
}

const HEADERS = {
  "content-type": "application/json; charset=utf-8",
  "x-content-type-options": "nosniff",
  // Project data is private: no shared cache may keep it, and a URL carrying a
  // task id must not travel to a third party in a Referer.
  "cache-control": "no-store",
  "referrer-policy": "no-referrer",
} as const;

export function json(body: unknown, status = 200, headers: Record<string, string> = {}): Response {
  return new Response(JSON.stringify(body), { status, headers: { ...HEADERS, ...headers } });
}

export function errorResponse(error: HttpError): Response {
  return json({ error: error.code, message: error.message, ...error.extra }, error.status);
}

export const notFound = (what = "resource") => new HttpError(404, "not_found", `${what} not found`);
export const forbidden = () => new HttpError(403, "forbidden", "your role does not allow this");
export const badRequest = (message: string) => new HttpError(400, "invalid_request", message);
export const unprocessable = (code: string, message: string, extra: Record<string, unknown> = {}) =>
  new HttpError(422, code, message, extra);
export const conflict = (code: string, message: string, extra: Record<string, unknown> = {}) =>
  new HttpError(409, code, message, extra);

/** Parses a JSON body. An empty body is `{}`; malformed JSON is a 400, never a crash. */
export async function readJson(req: Request): Promise<unknown> {
  const text = await req.text();
  if (!text) return {};
  try {
    return JSON.parse(text);
  } catch {
    throw badRequest("body is not valid JSON");
  }
}
```

`src/router.ts`:

```ts
export type Params = Record<string, string>;
export type Handler<C> = (ctx: C, params: Params) => Promise<Response> | Response;
export type Match<C> = { handler: Handler<C>; params: Params } | { allowed: string[] } | null;

interface Route<C> {
  method: string;
  parts: string[];
  handler: Handler<C>;
}

/** Method + path-pattern routing. `:name` segments become decoded params. */
export class Router<C> {
  private readonly routes: Route<C>[] = [];

  add(method: string, pattern: string, handler: Handler<C>): void {
    this.routes.push({ method, parts: split(pattern), handler });
  }

  match(method: string, path: string): Match<C> {
    const parts = split(path);
    const allowed: string[] = [];
    for (const route of this.routes) {
      const params = matchParts(route.parts, parts);
      if (!params) continue;
      if (route.method === method) return { handler: route.handler, params };
      allowed.push(route.method);
    }
    return allowed.length > 0 ? { allowed } : null;
  }
}

/** A route parameter the pattern guarantees exists. */
export function param(params: Params, name: string): string {
  const value = params[name];
  if (value === undefined) throw new Error(`route has no :${name} parameter`);
  return value;
}

function split(path: string): string[] {
  return path.split("/").filter(Boolean);
}

function matchParts(pattern: string[], parts: string[]): Params | null {
  if (pattern.length !== parts.length) return null;
  const params: Params = {};
  for (let i = 0; i < pattern.length; i++) {
    const expected = pattern[i] as string;
    const actual = parts[i] as string;
    if (expected.startsWith(":")) {
      try {
        params[expected.slice(1)] = decodeURIComponent(actual);
      } catch {
        return null;
      }
    } else if (expected !== actual) {
      return null;
    }
  }
  return params;
}
```

`src/contracts.ts`:

```ts
export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  /**
   * No `requiresAuth`: whether an app sits behind the SSO gate is Cloud's
   * operator-only switch, and Cloud ignores what an app says about itself.
   */
  /** The canonical in-shell route. */
  path: string;
  /** The direct HTTPS origin for bookmarks and standalone access. */
  publicUrl: string;
  delivery: "proxied-app";
  metadata: Record<string, unknown>;
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-project",
    name: "Nexus-Project",
    description: "Project management for solo users and teams: boards, WBS, dependencies and a critical-path schedule",
    mode: "orchestrated",
    exposed: true,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["projects", "tasks", "scheduling"],
    path: "/project",
    publicUrl: "https://project.tnhc.dev",
    delivery: "proxied-app",
    metadata: { version: "v1", defaultPort: 3152 },
  };
}
```

`src/cloud.ts`:

```ts
import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function heartbeatMs(): number {
  return Math.max(5000, Number(process.env.NEXUS_PROJECT_CLOUD_HEARTBEAT_INTERVAL_MS || "30000"));
}

function enabled(): boolean {
  return (process.env.NEXUS_PROJECT_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

export async function registerWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)),
  });
  if (!response.ok) throw new Error(`Nexus-Project registration failed: ${response.status}`);
}

export async function heartbeatWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/nexus-project/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!response.ok) throw new Error(`Nexus-Project heartbeat failed: ${response.status}`);
}

export function startHeartbeat(baseUrl: string): () => void {
  if (!enabled()) return () => {};
  registerWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-project] Cloud registration failed: ${(error as Error).message}`);
  });
  const timer = setInterval(() => {
    heartbeatWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-project] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatMs());
  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
```

`src/server.ts` (Task 7 replaces this with the authenticated version):

```ts
import { startHeartbeat } from "./cloud";
import { HttpError, errorResponse, json, notFound } from "./http";

export const API_PREFIX = "/api/v1/project";

export async function createServer() {
  const port = Number(process.env.PORT || "3152");
  const baseUrl = process.env.NEXUS_PROJECT_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();

  const server = Bun.serve({
    port,
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const path = new URL(req.url).pathname;
      if (req.method === "GET" && path === "/health") {
        return json({
          service: "nexus-project",
          status: "ok",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
        });
      }
      if (req.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-project", status: "ready", capabilities: ["projects", "tasks", "scheduling"] });
      }
      return errorResponse(notFound());
    },
    error(error) {
      if (error instanceof HttpError) return errorResponse(error);
      console.error("[nexus-project] unhandled error:", error);
      return json({ error: "internal", message: "internal error" }, 500);
    },
  });

  console.log(`[nexus-project] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    close: () => {
      stopHeartbeat();
      server.stop(true);
    },
  };
}
```

`src/index.ts`:

```ts
import { createServer } from "./server";

const { close } = await createServer();
process.on("SIGTERM", () => {
  close();
  process.exit(0);
});
process.on("SIGINT", () => {
  close();
  process.exit(0);
});
```

- [ ] **Step 5: Run the tests and the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: tsc clean, `Ran 9 tests across 3 files`, then `PASS`.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): replace the broken scaffold with a tested service skeleton

The scaffold never ran: 0/2 tests, an undefined PhantomApp, filters never
bound, a critical path that ignored dependencies and a sync that duplicated
data. This keeps its name, port 3152 and Cloud registration and nothing else.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---
### Task 2: Fractional ranks

Tasks are ordered by one `rank` string per project: the board sorts a status column by it and the list sorts siblings by it. Moving a task only rewrites that task's rank.

**Files:**
- Create: `apps/Nexus-Project/src/rank.ts`, `tests/support/random.ts`
- Test: `tests/rank.test.ts`

**Interfaces:**
- Produces: `rankBetween(before: string | null, after: string | null): string` — returns a string strictly between the two (lexicographically); `null` means open-ended. Throws `RangeError` if `before >= after`. `mulberry32(seed: number): () => number` from `tests/support/random.ts`.

- [ ] **Step 1: Write the failing test**

`tests/support/random.ts`:

```ts
/** Seeded PRNG so property tests are reproducible. Returns floats in [0, 1). */
export function mulberry32(seed: number): () => number {
  let state = seed >>> 0;
  return () => {
    state = (state + 0x6d2b79f5) >>> 0;
    let t = state;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function randomInt(next: () => number, min: number, max: number): number {
  return min + Math.floor(next() * (max - min + 1));
}
```

`tests/rank.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { rankBetween } from "../src/rank";
import { mulberry32, randomInt } from "./support/random";

describe("rankBetween", () => {
  it("starts in the middle of the space", () => {
    expect(rankBetween(null, null)).toBe("i");
  });

  it("always lands strictly between its bounds", () => {
    const next = mulberry32(7);
    const ranks: string[] = [rankBetween(null, null)];
    // Insert 2,000 times at random positions, including both ends, and check
    // the defining property every time rather than a final count.
    for (let i = 0; i < 2000; i++) {
      const at = randomInt(next, 0, ranks.length);
      const before = at === 0 ? null : (ranks[at - 1] as string);
      const after = at === ranks.length ? null : (ranks[at] as string);
      const rank = rankBetween(before, after);
      if (before !== null) expect(rank > before).toBe(true);
      if (after !== null) expect(rank < after).toBe(true);
      ranks.splice(at, 0, rank);
    }
    expect(new Set(ranks).size).toBe(ranks.length);
    expect([...ranks].sort()).toEqual(ranks);
  });

  it("never produces a rank ending in the lowest digit, so there is always room before it", () => {
    let rank = rankBetween(null, null);
    for (let i = 0; i < 200; i++) {
      rank = rankBetween(null, rank);
      expect(rank.endsWith("0")).toBe(false);
    }
  });

  it("refuses bounds in the wrong order", () => {
    expect(() => rankBetween("m", "c")).toThrow(RangeError);
    expect(() => rankBetween("m", "m")).toThrow(RangeError);
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/rank.test.ts`
Expected: FAIL — `Cannot find module '../src/rank'`.

- [ ] **Step 3: Implement**

`src/rank.ts`:

```ts
const DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz";
const BASE = DIGITS.length;

/**
 * A string strictly between `before` and `after` in lexicographic order.
 * `null` is an open end. Results never end in "0", so there is always room to
 * insert before any rank this returns.
 */
export function rankBetween(before: string | null, after: string | null): string {
  if (before !== null && after !== null && before >= after) {
    throw new RangeError(`rank bounds out of order: ${before} >= ${after}`);
  }
  let upper = after;
  let result = "";
  for (let i = 0; ; i++) {
    const low = before !== null && i < before.length ? DIGITS.indexOf(before[i] as string) : 0;
    const high = upper !== null && i < upper.length ? DIGITS.indexOf(upper[i] as string) : BASE;
    if (high - low > 1) return result + DIGITS[Math.floor((low + high) / 2)];
    result += DIGITS[low];
    // Once this prefix is below the upper bound's prefix, later digits of the
    // upper bound no longer constrain anything.
    if (low < high) upper = null;
  }
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cd apps/Nexus-Project && bun test tests/rank.test.ts`
Expected: PASS, 4 tests.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Project/src/rank.ts apps/Nexus-Project/tests/rank.test.ts apps/Nexus-Project/tests/support/random.ts
git commit -m "feat(project): fractional ranks for ordering tasks

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Working-day calendar

**Files:**
- Create: `apps/Nexus-Project/src/schedule/types.ts`, `src/schedule/calendar.ts`
- Test: `tests/calendar.test.ts`

**Interfaces:**
- Produces (`src/schedule/types.ts`, the whole engine vocabulary, used by Tasks 5, 10–13):

```ts
export type LinkType = "FS" | "SS" | "FF" | "SF";
export type TaskState = "open" | "completed" | "canceled";
export interface CalendarException { date: string; working: boolean }
export interface CalendarSpec { workingWeekdays: readonly number[]; exceptions: readonly CalendarException[] }
export interface ScheduleTask { id: string; key: string; number: number; parentId: string | null; kind: "task" | "milestone"; state: TaskState; durationDays: number | null; startDate: string | null; constraintType: "asap" | "start_no_earlier_than"; constraintDate: string | null; deadline: string | null; progress: number }
export interface ScheduleLink { id: string; predecessorId: string; successorId: string; type: LinkType; lagDays: number }
export interface ScheduleInput { projectStart: string; mode: "manual" | "auto"; calendar: CalendarSpec; tasks: readonly ScheduleTask[]; links: readonly ScheduleLink[] }
export interface TaskSchedule { id: string; earlyStart: string; earlyFinish: string; lateStart: string; lateFinish: string; totalFloat: number; freeFloat: number; critical: boolean }
export interface Violation { linkId: string; predecessorId: string; successorId: string; type: LinkType; gapDays: number }
export interface ScheduleWarning { code: "link_ignored"; linkId: string; taskId: string; reason: "unestimated" | "canceled" }
export interface SummaryRollup { id: string; start: string | null; finish: string | null; progress: number }
export interface ScheduleResult { projectFinish: string | null; tasks: TaskSchedule[]; summaries: SummaryRollup[]; criticalPath: string[]; violations: Violation[]; warnings: ScheduleWarning[] }
```

- Produces (`src/schedule/calendar.ts`): `DAY_MS`, `toDay(date: string): number` (throws `RangeError` on anything but a real `YYYY-MM-DD`), `fromDay(day: number): string`, `weekday(day: number): number` (0 = Sunday), `class WorkingCalendar { constructor(spec: CalendarSpec); isWorking(day): boolean; indexOf(day): number; dayAt(index): number }`. `indexOf(day)` is the number of working days in `[0, day)` (negative before 1970); `dayAt(i)` is the working day whose `indexOf` is `i`. So `indexOf(day)` of a non-working day equals the index of the next working day.

- [ ] **Step 1: Write the types file**

Write `src/schedule/types.ts` with exactly the declarations in the Interfaces block above, each on its own lines, with this header comment:

```ts
/**
 * The scheduling engine's vocabulary. Pure data: no database, clock or I/O.
 * Dates are YYYY-MM-DD strings; durations and lags are whole working days.
 */
```

- [ ] **Step 2: Write the failing test**

`tests/calendar.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { WorkingCalendar, fromDay, toDay, weekday } from "../src/schedule/calendar";
import type { CalendarSpec } from "../src/schedule/types";
import { mulberry32, randomInt } from "./support/random";

const WEEKDAYS: CalendarSpec = { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] };

describe("day numbers", () => {
  it("anchors day 0 at 1970-01-01, a Thursday", () => {
    expect(toDay("1970-01-01")).toBe(0);
    expect(weekday(0)).toBe(4);
    expect(weekday(toDay("2026-09-07"))).toBe(1); // a Monday
  });

  it("round-trips every day across leap years", () => {
    for (let day = toDay("2023-12-25"); day < toDay("2025-01-10"); day++) {
      expect(toDay(fromDay(day))).toBe(day);
    }
  });

  it("rejects strings that are not real dates", () => {
    for (const bad of ["2026-02-30", "2026-13-01", "2026-9-7", "20260907", "", "2026-09-07T00:00"]) {
      expect(() => toDay(bad)).toThrow(RangeError);
    }
  });
});

describe("WorkingCalendar", () => {
  const cal = new WorkingCalendar(WEEKDAYS);
  const monday = toDay("2026-09-07");

  it("counts only working days", () => {
    expect(cal.indexOf(monday + 1) - cal.indexOf(monday)).toBe(1); // Mon is working
    expect(cal.indexOf(monday + 7) - cal.indexOf(monday)).toBe(5); // one week
  });

  it("gives a weekend day the index of the following Monday", () => {
    expect(cal.indexOf(monday - 1)).toBe(cal.indexOf(monday)); // Sunday
    expect(cal.indexOf(monday - 2)).toBe(cal.indexOf(monday)); // Saturday
  });

  it("maps an index back to its working day", () => {
    expect(fromDay(cal.dayAt(cal.indexOf(monday) + 4))).toBe("2026-09-11"); // Friday
    expect(fromDay(cal.dayAt(cal.indexOf(monday) + 5))).toBe("2026-09-14"); // next Monday
  });

  it("applies holidays and working exceptions", () => {
    const holidays = new WorkingCalendar({
      workingWeekdays: [1, 2, 3, 4, 5],
      exceptions: [
        { date: "2026-09-08", working: false }, // a Tuesday off
        { date: "2026-09-12", working: true }, // a working Saturday
      ],
    });
    expect(holidays.isWorking(toDay("2026-09-08"))).toBe(false);
    expect(holidays.isWorking(toDay("2026-09-12"))).toBe(true);
    const start = holidays.indexOf(monday);
    expect([0, 1, 2, 3, 4, 5].map((i) => fromDay(holidays.dayAt(start + i)))).toEqual([
      "2026-09-07",
      "2026-09-09",
      "2026-09-10",
      "2026-09-11",
      "2026-09-12",
      "2026-09-14",
    ]);
  });

  it("ignores an exception that matches the weekday rule", () => {
    const same = new WorkingCalendar({ workingWeekdays: [1, 2, 3, 4, 5], exceptions: [{ date: "2026-09-13", working: false }] });
    expect(same.indexOf(monday + 14)).toBe(cal.indexOf(monday + 14));
  });

  it("refuses a calendar with no working weekdays", () => {
    expect(() => new WorkingCalendar({ workingWeekdays: [], exceptions: [] })).toThrow(RangeError);
    expect(() => new WorkingCalendar({ workingWeekdays: [7], exceptions: [] })).toThrow(RangeError);
  });

  it("holds its defining properties on random calendars", () => {
    const next = mulberry32(42);
    for (let trial = 0; trial < 50; trial++) {
      const weekdays = [0, 1, 2, 3, 4, 5, 6].filter(() => next() < 0.6);
      if (weekdays.length === 0) weekdays.push(randomInt(next, 0, 6));
      const exceptions = Array.from({ length: randomInt(next, 0, 20) }, () => ({
        date: fromDay(monday + randomInt(next, -60, 60)),
        working: next() < 0.5,
      }));
      const random = new WorkingCalendar({ workingWeekdays: weekdays, exceptions });
      for (let day = monday - 80; day < monday + 80; day++) {
        const step = random.indexOf(day + 1) - random.indexOf(day);
        // indexOf rises by exactly one across a working day and not at all otherwise.
        expect(step).toBe(random.isWorking(day) ? 1 : 0);
        if (random.isWorking(day)) expect(random.dayAt(random.indexOf(day))).toBe(day);
      }
      for (let index = random.indexOf(monday - 50); index < random.indexOf(monday + 50); index++) {
        const day = random.dayAt(index);
        // dayAt never lands on a non-working day, and round-trips.
        expect(random.isWorking(day)).toBe(true);
        expect(random.indexOf(day)).toBe(index);
      }
    }
  });
});
```

- [ ] **Step 3: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/calendar.test.ts`
Expected: FAIL — `Cannot find module '../src/schedule/calendar'`.

- [ ] **Step 4: Implement**

`src/schedule/calendar.ts`:

```ts
import type { CalendarSpec } from "./types";

export const DAY_MS = 86_400_000;
const DATE = /^(\d{4})-(\d{2})-(\d{2})$/;

/** Days since 1970-01-01 (UTC). Only real YYYY-MM-DD dates are accepted. */
export function toDay(date: string): number {
  const match = DATE.exec(date);
  if (!match) throw new RangeError(`not a YYYY-MM-DD date: ${date}`);
  const day = Date.UTC(Number(match[1]), Number(match[2]) - 1, Number(match[3])) / DAY_MS;
  if (fromDay(day) !== date) throw new RangeError(`not a real calendar date: ${date}`);
  return day;
}

export function fromDay(day: number): string {
  return new Date(day * DAY_MS).toISOString().slice(0, 10);
}

/** 0 = Sunday … 6 = Saturday. Day 0 (1970-01-01) was a Thursday. */
export function weekday(day: number): number {
  return (((day + 4) % 7) + 7) % 7;
}

/**
 * Converts between calendar days and working-day indices.
 *
 * `indexOf(day)` counts the working days in [0, day), so it rises by exactly
 * one across each working day. A non-working day therefore shares its index
 * with the next working day, which is what "a task starting on Saturday
 * starts on Monday" means.
 */
export class WorkingCalendar {
  private readonly mask: number;
  private readonly perWeek: number;
  private readonly flipDays: number[];
  private readonly flipPrefix: number[];
  private readonly flips: Map<number, number>;

  constructor(spec: CalendarSpec) {
    let mask = 0;
    for (const day of spec.workingWeekdays) {
      if (!Number.isInteger(day) || day < 0 || day > 6) throw new RangeError(`weekday out of range: ${day}`);
      mask |= 1 << day;
    }
    if (mask === 0) throw new RangeError("a calendar needs at least one working weekday");
    this.mask = mask;
    this.perWeek = [0, 1, 2, 3, 4, 5, 6].filter((d) => (mask >> d) & 1).length;

    // An exception only matters where it disagrees with the weekday rule; it
    // then "flips" that one day. Later entries for the same date win.
    this.flips = new Map();
    for (const exception of spec.exceptions) {
      const day = toDay(exception.date);
      if (exception.working === this.weekdayWorking(day)) this.flips.delete(day);
      else this.flips.set(day, exception.working ? 1 : -1);
    }
    this.flipDays = [...this.flips.keys()].sort((a, b) => a - b);
    this.flipPrefix = [];
    let sum = 0;
    for (const day of this.flipDays) {
      sum += this.flips.get(day) as number;
      this.flipPrefix.push(sum);
    }
  }

  private weekdayWorking(day: number): boolean {
    return ((this.mask >> weekday(day)) & 1) === 1;
  }

  isWorking(day: number): boolean {
    const flip = this.flips.get(day);
    return flip === undefined ? this.weekdayWorking(day) : flip === 1;
  }

  indexOf(day: number): number {
    const weeks = Math.floor(day / 7);
    let count = weeks * this.perWeek;
    for (let d = weeks * 7; d < day; d++) if (this.weekdayWorking(d)) count++;
    return count + this.flipsBefore(day);
  }

  /** The working day whose index is `index`. */
  dayAt(index: number): number {
    // The answer is the smallest day d with indexOf(d + 1) > index. Bracket it
    // from an estimate, widening geometrically, then binary-search.
    const above = (day: number) => this.indexOf(day + 1) > index;
    let lo = Math.floor((index * 7) / this.perWeek) - 7;
    for (let step = 7; above(lo); step *= 2) lo -= step;
    let hi = lo + 7;
    for (let step = 7; !above(hi); step *= 2) hi += step;
    while (hi - lo > 1) {
      const mid = Math.floor((lo + hi) / 2);
      if (above(mid)) hi = mid;
      else lo = mid;
    }
    return hi;
  }

  private flipsBefore(day: number): number {
    let lo = 0;
    let hi = this.flipDays.length;
    while (lo < hi) {
      const mid = (lo + hi) >> 1;
      if ((this.flipDays[mid] as number) < day) lo = mid + 1;
      else hi = mid;
    }
    return lo === 0 ? 0 : (this.flipPrefix[lo - 1] as number);
  }
}
```

- [ ] **Step 5: Run to verify it passes**

Run: `cd apps/Nexus-Project && bun test tests/calendar.test.ts`
Expected: PASS, 10 tests.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project/src/schedule apps/Nexus-Project/tests/calendar.test.ts
git commit -m "feat(project): working-day calendar for the scheduler

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Dependency graph

**Files:**
- Create: `apps/Nexus-Project/src/schedule/graph.ts`
- Test: `tests/graph.test.ts`

**Interfaces:**
- Produces: `interface Edge { from: string; to: string }`, `class CycleError extends Error { readonly cycle: string[] }`, `topologicalOrder(nodes: readonly string[], edges: readonly Edge[]): string[]` (throws `CycleError`), `findCycle(nodes, edges): string[] | null` — the cycle as a closed path `[a, b, …, a]`.

- [ ] **Step 1: Write the failing test**

`tests/graph.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { CycleError, type Edge, findCycle, topologicalOrder } from "../src/schedule/graph";
import { mulberry32, randomInt } from "./support/random";

/** Independent oracle: a graph has a cycle iff some node reaches itself. */
function hasCycleBruteForce(nodes: string[], edges: Edge[]): boolean {
  const index = new Map(nodes.map((n, i) => [n, i]));
  const n = nodes.length;
  const reach = Array.from({ length: n }, () => new Array<boolean>(n).fill(false));
  for (const e of edges) (reach[index.get(e.from) as number] as boolean[])[index.get(e.to) as number] = true;
  for (let k = 0; k < n; k++)
    for (let i = 0; i < n; i++)
      for (let j = 0; j < n; j++)
        if (reach[i]?.[k] && reach[k]?.[j]) (reach[i] as boolean[])[j] = true;
  return nodes.some((_, i) => reach[i]?.[i]);
}

function randomGraph(next: () => number): { nodes: string[]; edges: Edge[] } {
  const nodes = Array.from({ length: randomInt(next, 1, 8) }, (_, i) => `n${i}`);
  const edges: Edge[] = [];
  const count = randomInt(next, 0, 12);
  for (let i = 0; i < count; i++) {
    const from = nodes[randomInt(next, 0, nodes.length - 1)] as string;
    const to = nodes[randomInt(next, 0, nodes.length - 1)] as string;
    if (from !== to && !edges.some((e) => e.from === from && e.to === to)) edges.push({ from, to });
  }
  return { nodes, edges };
}

describe("graph", () => {
  it("orders every edge's source before its target", () => {
    const order = topologicalOrder(["a", "b", "c", "d"], [
      { from: "c", to: "b" },
      { from: "b", to: "a" },
      { from: "d", to: "a" },
    ]);
    const at = (id: string) => order.indexOf(id);
    expect(order).toHaveLength(4);
    expect(at("c") < at("b") && at("b") < at("a") && at("d") < at("a")).toBe(true);
  });

  it("names the cycle it refuses", () => {
    const edges = [
      { from: "a", to: "b" },
      { from: "b", to: "c" },
      { from: "c", to: "a" },
    ];
    expect(() => topologicalOrder(["a", "b", "c"], edges)).toThrow(CycleError);
    try {
      topologicalOrder(["a", "b", "c"], edges);
    } catch (error) {
      expect((error as CycleError).cycle).toEqual(["a", "b", "c", "a"]);
    }
  });

  it("agrees with a brute-force oracle on random graphs", () => {
    const next = mulberry32(99);
    for (let trial = 0; trial < 500; trial++) {
      const { nodes, edges } = randomGraph(next);
      const expected = hasCycleBruteForce(nodes, edges);
      const cycle = findCycle(nodes, edges);
      expect(cycle !== null).toBe(expected);
      if (cycle) {
        // The reported cycle is a real closed path through existing edges.
        expect(cycle[0]).toBe(cycle[cycle.length - 1]);
        for (let i = 0; i + 1 < cycle.length; i++) {
          expect(edges.some((e) => e.from === cycle[i] && e.to === cycle[i + 1])).toBe(true);
        }
      } else {
        const order = topologicalOrder(nodes, edges);
        expect([...order].sort()).toEqual([...nodes].sort());
        for (const e of edges) expect(order.indexOf(e.from) < order.indexOf(e.to)).toBe(true);
      }
    }
  });

  it("rejects an edge to an unknown node", () => {
    expect(() => topologicalOrder(["a"], [{ from: "a", to: "zz" }])).toThrow("unknown node");
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/graph.test.ts`
Expected: FAIL — `Cannot find module '../src/schedule/graph'`.

- [ ] **Step 3: Implement**

`src/schedule/graph.ts`:

```ts
export interface Edge {
  from: string;
  to: string;
}

export class CycleError extends Error {
  constructor(readonly cycle: string[]) {
    super(`dependency cycle: ${cycle.join(" -> ")}`);
  }
}

function adjacency(nodes: readonly string[], edges: readonly Edge[]): Map<string, string[]> {
  const out = new Map<string, string[]>();
  for (const node of nodes) out.set(node, []);
  for (const edge of edges) {
    const targets = out.get(edge.from);
    if (!targets || !out.has(edge.to)) throw new Error(`edge references unknown node: ${edge.from} -> ${edge.to}`);
    targets.push(edge.to);
  }
  return out;
}

/** Kahn's algorithm. Deterministic for a given input order. Throws CycleError. */
export function topologicalOrder(nodes: readonly string[], edges: readonly Edge[]): string[] {
  const out = adjacency(nodes, edges);
  const indegree = new Map<string, number>(nodes.map((n) => [n, 0]));
  for (const edge of edges) indegree.set(edge.to, (indegree.get(edge.to) as number) + 1);
  const queue = nodes.filter((n) => indegree.get(n) === 0);
  for (let head = 0; head < queue.length; head++) {
    for (const target of out.get(queue[head] as string) as string[]) {
      const remaining = (indegree.get(target) as number) - 1;
      indegree.set(target, remaining);
      if (remaining === 0) queue.push(target);
    }
  }
  if (queue.length !== nodes.length) throw new CycleError(findCycle(nodes, edges) ?? []);
  return queue;
}

/** Iterative depth-first search; returns the first cycle found as [a, …, a]. */
export function findCycle(nodes: readonly string[], edges: readonly Edge[]): string[] | null {
  const out = adjacency(nodes, edges);
  const state = new Map<string, 1 | 2>(); // 1 = on the current path, 2 = finished
  for (const start of nodes) {
    if (state.has(start)) continue;
    const stack: { node: string; next: number }[] = [{ node: start, next: 0 }];
    const path: string[] = [start];
    state.set(start, 1);
    while (stack.length > 0) {
      const frame = stack[stack.length - 1] as { node: string; next: number };
      const targets = out.get(frame.node) as string[];
      if (frame.next < targets.length) {
        const target = targets[frame.next++] as string;
        const seen = state.get(target);
        if (seen === 1) return [...path.slice(path.indexOf(target)), target];
        if (seen === undefined) {
          state.set(target, 1);
          stack.push({ node: target, next: 0 });
          path.push(target);
        }
      } else {
        state.set(frame.node, 2);
        stack.pop();
        path.pop();
      }
    }
  }
  return null;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cd apps/Nexus-Project && bun test tests/graph.test.ts`
Expected: PASS, 4 tests.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Project/src/schedule/graph.ts apps/Nexus-Project/tests/graph.test.ts
git commit -m "feat(project): topological order and cycle finding for dependencies

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---
### Task 5: The scheduling engine

**Files:**
- Create: `apps/Nexus-Project/src/schedule/engine.ts`, `tests/support/schedule.ts`
- Test: `tests/engine.test.ts`

**Interfaces:**
- Consumes: `WorkingCalendar`, `toDay`, `fromDay` (Task 3); `topologicalOrder`, `CycleError` (Task 4); all types from `src/schedule/types.ts`.
- Produces: `schedule(input: ScheduleInput): ScheduleResult`. Throws `CycleError` if the links contain a cycle (the store prevents this; the throw is a guard). Output `tasks` contains only scheduled leaves (estimated, not canceled, no children), sorted by task `number`.

Rules the engine implements (spec, "Scheduling Engine"):

| Rule | Behaviour |
|---|---|
| Scheduled leaf | not a summary (no task has it as parent), not `canceled`, and `kind = milestone` (duration 0) or `durationDays !== null` |
| Floor | auto: `projectStart`; manual: the stored `startDate` if set, else `projectStart`; then raised to `constraintDate` for `start_no_earlier_than` |
| Pinned | a `completed` task with a `startDate` starts exactly there in either mode and ignores links |
| Forward | `ES = max(floor, requirement of each incoming link)`; `EF = ES + duration` |
| Requirement | FS `p.EF+lag`; SS `p.ES+lag`; FF `p.EF+lag−d`; SF `p.ES+lag−d` (d = successor duration) |
| Violation | manual mode, stored start `s`, a link requiring `r > s` → `{ gapDays: r − s }` |
| Backward | `LF = min(projectFinish, deadline cap, bound of each outgoing link)`; `LS = LF − d` |
| Bound | FS `s.LS−lag`; SS `s.LS−lag+d`; FF `s.LF−lag`; SF `s.LF−lag+d` (d = predecessor duration) |
| Deadline cap | `indexOf(deadline + 1 day)`: finish by the end of the deadline day |
| Float | total `LS − ES`; free = min(link slacks, `projectFinish − EF`, deadline cap − EF, total); critical when total ≤ 0 |
| Excluded link | a link touching an unestimated or canceled leaf is ignored and reported as a `link_ignored` warning |
| Roll-up | summary start/finish = earliest/latest of its descendants; progress = Σ(weight × progress)/Σweight, weight = duration for scheduled leaves (milestones weigh 0), 1 for unestimated leaves (progress 0, or 100 if completed); canceled leaves are left out; if every weight is 0, the plain mean |

- [ ] **Step 1: Write the fixtures and the failing tests**

`tests/support/schedule.ts`:

```ts
import type { LinkType, ScheduleInput, ScheduleLink, ScheduleResult, ScheduleTask, TaskSchedule } from "../../src/schedule/types";

export const WEEKDAYS = { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] };
/** 2026-09-07 is a Monday. */
export const MONDAY = "2026-09-07";

export function task(id: string, number: number, fields: Partial<ScheduleTask> = {}): ScheduleTask {
  return {
    id,
    key: `T-${number}`,
    number,
    parentId: null,
    kind: "task",
    state: "open",
    durationDays: 1,
    startDate: null,
    constraintType: "asap",
    constraintDate: null,
    deadline: null,
    progress: 0,
    ...fields,
  };
}

export function link(id: string, predecessorId: string, successorId: string, type: LinkType = "FS", lagDays = 0): ScheduleLink {
  return { id, predecessorId, successorId, type, lagDays };
}

export function input(tasks: ScheduleTask[], links: ScheduleLink[] = [], overrides: Partial<ScheduleInput> = {}): ScheduleInput {
  return { projectStart: MONDAY, mode: "auto", calendar: WEEKDAYS, tasks, links, ...overrides };
}

export function byId(result: ScheduleResult): Map<string, TaskSchedule> {
  return new Map(result.tasks.map((t) => [t.id, t]));
}

export function get(result: ScheduleResult, id: string): TaskSchedule {
  const found = byId(result).get(id);
  if (!found) throw new Error(`task ${id} was not scheduled`);
  return found;
}
```

`tests/engine.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { schedule } from "../src/schedule/engine";
import { CycleError } from "../src/schedule/graph";
import { byId, get, input, link, task } from "./support/schedule";

// Dates below: 2026-09-07 Mon … 09-11 Fri, 09-14 Mon … 09-18 Fri.

describe("forward pass and links", () => {
  it("chains finish-to-start and marks the chain critical", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2, { durationDays: 2 })], [link("l", "A", "B")]));
    expect(get(r, "A")).toMatchObject({ earlyStart: "2026-09-07", earlyFinish: "2026-09-09", totalFloat: 0, critical: true });
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-10", earlyFinish: "2026-09-11", totalFloat: 0, critical: true });
    expect(r.projectFinish).toBe("2026-09-11");
    expect(r.criticalPath).toEqual(["A", "B"]);
  });

  it("skips the weekend", () => {
    const r = schedule(input([task("A", 1, { durationDays: 5 }), task("B", 2)], [link("l", "A", "B")]));
    expect(get(r, "A").earlyFinish).toBe("2026-09-11");
    expect(get(r, "B").earlyStart).toBe("2026-09-14");
  });

  it("applies lag and lead in working days", () => {
    const lag = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2)], [link("l", "A", "B", "FS", 2)]));
    expect(get(lag, "B").earlyStart).toBe("2026-09-14");
    const lead = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2)], [link("l", "A", "B", "FS", -1)]));
    expect(get(lead, "B").earlyStart).toBe("2026-09-09");
  });

  it("schedules start-to-start", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2, { durationDays: 2 })], [link("l", "A", "B", "SS", 1)]));
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-08", earlyFinish: "2026-09-09", critical: true });
    expect(get(r, "A").critical).toBe(true);
  });

  it("schedules finish-to-finish", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("B", 2)], [link("l", "A", "B", "FF")]));
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-09", earlyFinish: "2026-09-09" });
  });

  it("schedules start-to-finish", () => {
    const tasks = [
      task("A", 1, { durationDays: 3, constraintType: "start_no_earlier_than", constraintDate: "2026-09-14" }),
      task("B", 2, { durationDays: 2 }),
    ];
    const r = schedule(input(tasks, [link("l", "A", "B", "SF")]));
    expect(get(r, "A").earlyStart).toBe("2026-09-14");
    expect(get(r, "B")).toMatchObject({ earlyStart: "2026-09-10", earlyFinish: "2026-09-11" });
  });

  it("respects a holiday", () => {
    const r = schedule(
      input([task("A", 1, { durationDays: 3 })], [], {
        calendar: { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [{ date: "2026-09-08", working: false }] },
      }),
    );
    expect(get(r, "A").earlyFinish).toBe("2026-09-10");
  });

  it("places a milestone at the start of the day its driver frees", () => {
    const r = schedule(input([task("A", 1, { durationDays: 3 }), task("M", 2, { kind: "milestone", durationDays: null })], [link("l", "A", "M")]));
    expect(get(r, "M")).toMatchObject({ earlyStart: "2026-09-10", earlyFinish: "2026-09-10", critical: true });
  });

  it("refuses a cycle", () => {
    expect(() => schedule(input([task("A", 1), task("B", 2)], [link("x", "A", "B"), link("y", "B", "A")]))).toThrow(CycleError);
  });
});

describe("backward pass and float", () => {
  it("gives the shorter parallel path float", () => {
    const r = schedule(
      input(
        [task("A", 1, { durationDays: 5 }), task("B", 2, { durationDays: 2 }), task("C", 3)],
        [link("l1", "A", "C"), link("l2", "B", "C")],
      ),
    );
    expect(get(r, "A")).toMatchObject({ totalFloat: 0, freeFloat: 0, critical: true });
    expect(get(r, "B")).toMatchObject({ totalFloat: 3, freeFloat: 3, critical: false, lateStart: "2026-09-10" });
    expect(r.criticalPath).toEqual(["A", "C"]);
  });

  it("turns a missed deadline into negative float", () => {
    const r = schedule(input([task("A", 1, { durationDays: 5, deadline: "2026-09-09" })]));
    expect(get(r, "A")).toMatchObject({ totalFloat: -2, critical: true, lateFinish: "2026-09-09", lateStart: "2026-09-03" });
  });
});

describe("modes", () => {
  it("manual mode never moves a stored date earlier and reports the broken link", () => {
    const r = schedule(
      input(
        [task("A", 1, { durationDays: 3, startDate: "2026-09-07" }), task("B", 2, { durationDays: 2, startDate: "2026-09-08" })],
        [link("l", "A", "B")],
        { mode: "manual" },
      ),
    );
    expect(r.violations).toEqual([{ linkId: "l", predecessorId: "A", successorId: "B", type: "FS", gapDays: 2 }]);
    expect(get(r, "B").earlyStart).toBe("2026-09-10");
  });

  it("manual mode keeps a stored date later than the logic needs", () => {
    const r = schedule(
      input(
        [task("A", 1, { durationDays: 3, startDate: "2026-09-07" }), task("B", 2, { durationDays: 2, startDate: "2026-09-21" })],
        [link("l", "A", "B")],
        { mode: "manual" },
      ),
    );
    expect(r.violations).toEqual([]);
    expect(get(r, "B").earlyStart).toBe("2026-09-21");
    expect(get(r, "A").totalFloat).toBe(7);
  });

  it("auto mode ignores stored dates and honours start-no-earlier-than", () => {
    const r = schedule(
      input([
        task("A", 1, { durationDays: 3 }),
        task("B", 2, { durationDays: 2, startDate: "2026-09-21" }),
        task("C", 3, { constraintType: "start_no_earlier_than", constraintDate: "2026-09-09" }),
      ], [link("l", "A", "B")]),
    );
    expect(get(r, "B").earlyStart).toBe("2026-09-10");
    expect(get(r, "C").earlyStart).toBe("2026-09-09");
    expect(r.violations).toEqual([]);
  });

  it("pins a completed task to its recorded start", () => {
    const r = schedule(
      input([task("A", 1, { durationDays: 3 }), task("C", 2, { state: "completed", startDate: "2026-09-07" })], [link("l", "A", "C")]),
    );
    expect(get(r, "C").earlyStart).toBe("2026-09-07");
    expect(r.violations).toEqual([]);
  });
});

describe("tasks outside the schedule", () => {
  it("ignores links to unestimated and canceled tasks and says so", () => {
    const r = schedule(
      input(
        [task("A", 1), task("U", 2, { durationDays: null }), task("X", 3, { state: "canceled", durationDays: 2 })],
        [link("l1", "A", "U"), link("l2", "X", "A")],
      ),
    );
    expect([...byId(r).keys()]).toEqual(["A"]);
    expect(r.warnings).toEqual([
      { code: "link_ignored", linkId: "l1", taskId: "U", reason: "unestimated" },
      { code: "link_ignored", linkId: "l2", taskId: "X", reason: "canceled" },
    ]);
  });

  it("returns an empty schedule for a project with nothing estimated", () => {
    const r = schedule(input([task("U", 1, { durationDays: null })]));
    expect(r).toMatchObject({ projectFinish: null, tasks: [], criticalPath: [] });
  });
});

describe("summary roll-up", () => {
  it("spans its children and weights progress by duration", () => {
    const r = schedule(
      input(
        [
          task("S", 1, { durationDays: null }),
          task("A", 2, { parentId: "S", durationDays: 3, progress: 50 }),
          task("B", 3, { parentId: "S", durationDays: 1, progress: 0 }),
        ],
        [link("l", "A", "B")],
      ),
    );
    expect(r.summaries).toEqual([{ id: "S", start: "2026-09-07", finish: "2026-09-10", progress: 38 }]);
    expect(byId(r).has("S")).toBe(false);
  });

  it("rolls up through nested summaries and counts unestimated work", () => {
    const r = schedule(
      input([
        task("S1", 1, { durationDays: null }),
        task("S2", 2, { parentId: "S1", durationDays: null }),
        task("A", 3, { parentId: "S2", durationDays: 2, state: "completed" }),
        task("U", 4, { parentId: "S1", durationDays: null }),
        task("X", 5, { parentId: "S1", durationDays: 9, state: "canceled" }),
      ]),
    );
    const summaries = new Map(r.summaries.map((s) => [s.id, s]));
    expect(summaries.get("S2")).toEqual({ id: "S2", start: "2026-09-07", finish: "2026-09-08", progress: 100 });
    expect(summaries.get("S1")).toEqual({ id: "S1", start: "2026-09-07", finish: "2026-09-08", progress: 67 });
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/engine.test.ts`
Expected: FAIL — `Cannot find module '../src/schedule/engine'`.

- [ ] **Step 3: Implement**

`src/schedule/engine.ts`:

```ts
import { WorkingCalendar, fromDay, toDay } from "./calendar";
import { type Edge, topologicalOrder } from "./graph";
import type {
  ScheduleInput,
  ScheduleLink,
  ScheduleResult,
  ScheduleTask,
  ScheduleWarning,
  SummaryRollup,
  TaskSchedule,
  Violation,
} from "./types";

/** A scheduled leaf. All positions are working-day indices; [es, ef) is half-open. */
interface Node {
  task: ScheduleTask;
  duration: number;
  floor: number;
  stored: number | null; // manual mode only
  pinned: number | null; // completed with a recorded start
  deadlineCap: number | null;
  es: number;
  ef: number;
  ls: number;
  lf: number;
}

/** The earliest start a link allows its successor. */
function requiredStart(link: ScheduleLink, pred: Node, successorDuration: number): number {
  switch (link.type) {
    case "FS":
      return pred.ef + link.lagDays;
    case "SS":
      return pred.es + link.lagDays;
    case "FF":
      return pred.ef + link.lagDays - successorDuration;
    case "SF":
      return pred.es + link.lagDays - successorDuration;
  }
}

/** The latest finish a link allows its predecessor. */
function latestFinish(link: ScheduleLink, succ: Node, predecessorDuration: number): number {
  switch (link.type) {
    case "FS":
      return succ.ls - link.lagDays;
    case "SS":
      return succ.ls - link.lagDays + predecessorDuration;
    case "FF":
      return succ.lf - link.lagDays;
    case "SF":
      return succ.lf - link.lagDays + predecessorDuration;
  }
}

/** How far the predecessor could slip, on early dates, before this link binds. */
function linkSlack(link: ScheduleLink, pred: Node, succ: Node): number {
  switch (link.type) {
    case "FS":
      return succ.es - (pred.ef + link.lagDays);
    case "SS":
      return succ.es - (pred.es + link.lagDays);
    case "FF":
      return succ.ef - (pred.ef + link.lagDays);
    case "SF":
      return succ.ef - (pred.es + link.lagDays);
  }
}

function push<K, V>(map: Map<K, V[]>, key: K, value: V): void {
  const list = map.get(key);
  if (list) list.push(value);
  else map.set(key, [value]);
}

interface Aggregate {
  start: string | null;
  finish: string | null;
  weight: number;
  weighted: number;
  count: number;
  sum: number;
}

export function schedule(input: ScheduleInput): ScheduleResult {
  const calendar = new WorkingCalendar(input.calendar);
  const indexOf = (date: string) => calendar.indexOf(toDay(date));
  const dateAt = (index: number) => fromDay(calendar.dayAt(index));
  const projectStart = indexOf(input.projectStart);

  const childrenOf = new Map<string, ScheduleTask[]>();
  for (const task of input.tasks) {
    if (task.parentId === null) continue;
    const siblings = childrenOf.get(task.parentId) ?? [];
    siblings.push(task);
    childrenOf.set(task.parentId, siblings);
  }

  const nodes = new Map<string, Node>();
  const excluded = new Map<string, "unestimated" | "canceled">();
  for (const task of input.tasks) {
    if (childrenOf.has(task.id)) continue; // summary: scheduled by its children
    if (task.state === "canceled") {
      excluded.set(task.id, "canceled");
      continue;
    }
    const duration = task.kind === "milestone" ? 0 : task.durationDays;
    if (duration === null) {
      excluded.set(task.id, "unestimated");
      continue;
    }
    const recorded = task.startDate === null ? null : indexOf(task.startDate);
    const stored = input.mode === "manual" ? recorded : null;
    let floor = stored ?? projectStart;
    if (task.constraintType === "start_no_earlier_than" && task.constraintDate !== null) {
      floor = Math.max(floor, indexOf(task.constraintDate));
    }
    nodes.set(task.id, {
      task,
      duration,
      floor,
      stored,
      pinned: task.state === "completed" ? recorded : null,
      deadlineCap: task.deadline === null ? null : calendar.indexOf(toDay(task.deadline) + 1),
      es: 0,
      ef: 0,
      ls: 0,
      lf: 0,
    });
  }

  const incoming = new Map<string, ScheduleLink[]>();
  const outgoing = new Map<string, ScheduleLink[]>();
  const edges: Edge[] = [];
  const warnings: ScheduleWarning[] = [];
  for (const link of input.links) {
    if (nodes.has(link.predecessorId) && nodes.has(link.successorId)) {
      push(incoming, link.successorId, link);
      push(outgoing, link.predecessorId, link);
      edges.push({ from: link.predecessorId, to: link.successorId });
      continue;
    }
    for (const taskId of [link.predecessorId, link.successorId]) {
      const reason = excluded.get(taskId);
      if (reason) {
        warnings.push({ code: "link_ignored", linkId: link.id, taskId, reason });
        break;
      }
    }
  }

  const ids = [...nodes.values()].sort((a, b) => a.task.number - b.task.number).map((n) => n.task.id);
  const order = topologicalOrder(ids, edges);
  const node = (id: string) => nodes.get(id) as Node;

  // Forward pass.
  const violations: Violation[] = [];
  for (const id of order) {
    const current = node(id);
    let es = current.floor;
    if (current.pinned !== null) {
      es = current.pinned;
    } else {
      for (const link of incoming.get(id) ?? []) {
        const required = requiredStart(link, node(link.predecessorId), current.duration);
        if (current.stored !== null && required > current.stored) {
          violations.push({
            linkId: link.id,
            predecessorId: link.predecessorId,
            successorId: id,
            type: link.type,
            gapDays: required - current.stored,
          });
        }
        es = Math.max(es, required);
      }
    }
    current.es = es;
    current.ef = es + current.duration;
  }

  const projectFinish = order.length === 0 ? null : Math.max(...order.map((id) => node(id).ef));

  // Backward pass.
  for (let i = order.length - 1; i >= 0; i--) {
    const current = node(order[i] as string);
    let lf = projectFinish as number;
    if (current.deadlineCap !== null) lf = Math.min(lf, current.deadlineCap);
    for (const link of outgoing.get(current.task.id) ?? []) {
      lf = Math.min(lf, latestFinish(link, node(link.successorId), current.duration));
    }
    current.lf = lf;
    current.ls = lf - current.duration;
  }

  const finishDate = (start: number, end: number) => (end > start ? dateAt(end - 1) : dateAt(start));
  const tasks: TaskSchedule[] = ids.map((id) => {
    const current = node(id);
    const totalFloat = current.ls - current.es;
    let freeFloat = (projectFinish as number) - current.ef;
    if (current.deadlineCap !== null) freeFloat = Math.min(freeFloat, current.deadlineCap - current.ef);
    for (const link of outgoing.get(id) ?? []) {
      freeFloat = Math.min(freeFloat, linkSlack(link, current, node(link.successorId)));
    }
    return {
      id,
      earlyStart: dateAt(current.es),
      earlyFinish: finishDate(current.es, current.ef),
      lateStart: dateAt(current.ls),
      lateFinish: finishDate(current.ls, current.lf),
      totalFloat,
      freeFloat: Math.min(freeFloat, totalFloat),
      critical: totalFloat <= 0,
    };
  });

  const criticalPath = tasks
    .filter((t) => t.critical)
    .sort((a, b) => node(a.id).es - node(b.id).es || node(a.id).task.number - node(b.id).task.number)
    .map((t) => t.id);

  return {
    projectFinish: tasks.reduce<string | null>((max, t) => (max === null || t.earlyFinish > max ? t.earlyFinish : max), null),
    tasks,
    summaries: rollUp(input.tasks, childrenOf, nodes, new Map(tasks.map((t) => [t.id, t]))),
    criticalPath,
    violations,
    warnings,
  };
}

function rollUp(
  all: readonly ScheduleTask[],
  childrenOf: Map<string, ScheduleTask[]>,
  nodes: Map<string, Node>,
  scheduled: Map<string, TaskSchedule>,
): SummaryRollup[] {
  const memo = new Map<string, Aggregate>();
  const aggregate = (task: ScheduleTask): Aggregate => {
    const cached = memo.get(task.id);
    if (cached) return cached;
    const children = childrenOf.get(task.id);
    let result: Aggregate;
    if (children) {
      result = { start: null, finish: null, weight: 0, weighted: 0, count: 0, sum: 0 };
      for (const child of children) {
        if (child.state === "canceled" && !childrenOf.has(child.id)) continue;
        const part = aggregate(child);
        if (part.start !== null && (result.start === null || part.start < result.start)) result.start = part.start;
        if (part.finish !== null && (result.finish === null || part.finish > result.finish)) result.finish = part.finish;
        result.weight += part.weight;
        result.weighted += part.weighted;
        result.count += part.count;
        result.sum += part.sum;
      }
    } else {
      const done = task.state === "completed";
      const timing = scheduled.get(task.id);
      const leaf = nodes.get(task.id);
      const progress = leaf ? (done ? 100 : task.progress) : done ? 100 : 0;
      const weight = leaf ? leaf.duration : 1;
      result = {
        start: timing?.earlyStart ?? null,
        finish: timing?.earlyFinish ?? null,
        weight,
        weighted: weight * progress,
        count: 1,
        sum: progress,
      };
    }
    memo.set(task.id, result);
    return result;
  };

  const summaries: SummaryRollup[] = [];
  for (const task of all) {
    if (!childrenOf.has(task.id)) continue;
    const total = aggregate(task);
    const progress = total.weight > 0 ? total.weighted / total.weight : total.count > 0 ? total.sum / total.count : 0;
    summaries.push({ id: task.id, start: total.start, finish: total.finish, progress: Math.round(progress) });
  }
  return summaries;
}
```

- [ ] **Step 4: Run to verify it passes**

Run: `cd apps/Nexus-Project && bun test tests/engine.test.ts`
Expected: PASS, 19 tests. If an expected date is off, recheck the index arithmetic against the table in this task before changing a test: the tests encode the spec.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Project/src/schedule/engine.ts apps/Nexus-Project/tests/engine.test.ts apps/Nexus-Project/tests/support/schedule.ts
git commit -m "feat(project): critical-path scheduling engine

Forward and backward passes over all four link types with lag, working-day
calendars, deadlines as negative float, manual-mode violations, auto-mode
constraints, pinned completed work, and summary roll-up.

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: Engine properties and benchmark

Example tests show the engine works on cases someone thought of; these check the defining properties on generated projects.

**Files:**
- Test: `apps/Nexus-Project/tests/engine.properties.test.ts`

**Interfaces:**
- Consumes: `schedule` (Task 5), `WorkingCalendar`, `toDay` (Task 3), fixtures (Task 5), `mulberry32`, `randomInt` (Task 2).

- [ ] **Step 1: Write the tests**

`tests/engine.properties.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { WorkingCalendar, toDay } from "../src/schedule/calendar";
import { schedule } from "../src/schedule/engine";
import type { LinkType, ScheduleInput, ScheduleLink, ScheduleTask } from "../src/schedule/types";
import { mulberry32, randomInt } from "./support/random";
import { MONDAY, task } from "./support/schedule";

const TYPES: LinkType[] = ["FS", "SS", "FF", "SF"];

/** A random acyclic project: links only run from a lower to a higher position. */
function randomProject(seed: number, size: number, linkCount: number): ScheduleInput {
  const next = mulberry32(seed);
  const weekdays = [1, 2, 3, 4, 5].filter(() => next() < 0.9);
  if (weekdays.length === 0) weekdays.push(3);
  const tasks: ScheduleTask[] = Array.from({ length: size }, (_, i) => {
    const milestone = next() < 0.1;
    return task(`t${i}`, i + 1, milestone ? { kind: "milestone", durationDays: null } : { durationDays: randomInt(next, 1, 8) });
  });
  const links: ScheduleLink[] = [];
  const seen = new Set<string>();
  for (let i = 0; i < linkCount && size > 1; i++) {
    const a = randomInt(next, 0, size - 2);
    const b = randomInt(next, a + 1, size - 1);
    const key = `${a}-${b}`;
    if (seen.has(key)) continue;
    seen.add(key);
    links.push({
      id: `l${i}`,
      predecessorId: `t${a}`,
      successorId: `t${b}`,
      type: TYPES[randomInt(next, 0, 3)] as LinkType,
      lagDays: randomInt(next, -2, 3),
    });
  }
  const exceptions = Array.from({ length: randomInt(next, 0, 4) }, () => ({
    date: new Date((toDay(MONDAY) + randomInt(next, 0, 60)) * 86_400_000).toISOString().slice(0, 10),
    working: false,
  }));
  return { projectStart: MONDAY, mode: "auto", calendar: { workingWeekdays: weekdays, exceptions }, tasks, links };
}

function positions(input: ScheduleInput) {
  const calendar = new WorkingCalendar(input.calendar);
  const result = schedule(input);
  const duration = new Map(input.tasks.map((t) => [t.id, t.kind === "milestone" ? 0 : (t.durationDays as number)]));
  const at = new Map(
    result.tasks.map((t) => {
      const es = calendar.indexOf(toDay(t.earlyStart));
      return [t.id, { es, ef: es + (duration.get(t.id) as number), tf: t.totalFloat, critical: t.critical }];
    }),
  );
  return { result, at, start: calendar.indexOf(toDay(input.projectStart)) };
}

function required(link: ScheduleLink, pred: { es: number; ef: number }, successorDuration: number): number {
  const base = link.type === "FS" || link.type === "FF" ? pred.ef : pred.es;
  const shift = link.type === "FF" || link.type === "SF" ? successorDuration : 0;
  return base + link.lagDays - shift;
}

describe("engine properties on random projects", () => {
  for (let seed = 1; seed <= 200; seed++) {
    const project = randomProject(seed, 2 + (seed % 25), seed % 40);
    const { result, at, start } = positions(project);

    it(`seed ${seed}: satisfies every link and never starts before the project`, () => {
      for (const link of project.links) {
        const pred = at.get(link.predecessorId);
        const succ = at.get(link.successorId);
        if (!pred || !succ) throw new Error("a linked task was not scheduled");
        expect(succ.es).toBeGreaterThanOrEqual(required(link, pred, succ.ef - succ.es));
      }
      for (const p of at.values()) expect(p.es).toBeGreaterThanOrEqual(start);
    });

    it(`seed ${seed}: has no negative float without deadlines, and the last finish is critical`, () => {
      const finish = Math.max(...[...at.values()].map((p) => p.ef));
      for (const p of at.values()) {
        expect(p.tf).toBeGreaterThanOrEqual(0);
        if (p.ef === finish) expect(p.critical).toBe(true);
      }
    });

    it(`seed ${seed}: every critical task after the start is driven by a critical predecessor`, () => {
      for (const [id, p] of at) {
        if (!p.critical || p.es === start) continue;
        const drivers = project.links.filter((l) => {
          if (l.successorId !== id) return false;
          const pred = at.get(l.predecessorId) as { es: number; ef: number; critical: boolean };
          return pred.critical && required(l, pred, p.ef - p.es) === p.es;
        });
        expect(drivers.length).toBeGreaterThan(0);
      }
    });

    it(`seed ${seed}: a manual plan that already matches the logic stays put with no violations`, () => {
      const byId = new Map(result.tasks.map((t) => [t.id, t]));
      const manual: ScheduleInput = {
        ...project,
        mode: "manual",
        tasks: project.tasks.map((t) => ({ ...t, startDate: byId.get(t.id)?.earlyStart ?? null })),
      };
      const again = schedule(manual);
      expect(again.violations).toEqual([]);
      expect(again.tasks.map((t) => t.earlyStart)).toEqual(result.tasks.map((t) => t.earlyStart));
    });
  }

  it("gives the same answer whatever order tasks and links arrive in", () => {
    const project = randomProject(4242, 30, 60);
    const next = mulberry32(1);
    const shuffle = <T>(items: readonly T[]) => [...items].sort(() => next() - 0.5);
    const shuffled = { ...project, tasks: shuffle(project.tasks), links: shuffle(project.links) };
    expect(schedule(shuffled)).toEqual(schedule(project));
  });
});

describe("engine performance", () => {
  it("schedules 5,000 tasks and 10,000 links in under 50 ms", () => {
    const project = randomProject(2026, 5000, 10_000);
    schedule(project); // warm up the JIT
    const runs = [0, 1, 2, 3, 4].map(() => {
      const started = performance.now();
      schedule(project);
      return performance.now() - started;
    });
    const median = [...runs].sort((a, b) => a - b)[2] as number;
    expect(median).toBeLessThan(50);
  });
});
```

- [ ] **Step 2: Run the tests**

Run: `cd apps/Nexus-Project && bun test tests/engine.properties.test.ts`
Expected: PASS, 802 tests. A failure here is a real engine bug: fix `src/schedule/engine.ts` (use `superpowers:systematic-debugging`), never loosen a property. If the benchmark alone fails, profile before changing anything; every pass is O(tasks + links).

- [ ] **Step 3: Mutation-check the properties**

Temporarily change `case "FS": return pred.ef + link.lagDays;` in `requiredStart` to `return pred.ef;`, run the file, and confirm the first property fails for many seeds. Revert the change and rerun to green. (A property that cannot fail proves nothing.)

- [ ] **Step 4: Commit**

```bash
git add apps/Nexus-Project/tests/engine.properties.test.ts apps/Nexus-Project/src/schedule/engine.ts
git commit -m "test(project): property tests and benchmark for the scheduler

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---
### Task 7: Database, identity, access rules and validation

**Files:**
- Create: `apps/Nexus-Project/src/store/db.ts`, `src/store/rows.ts`, `src/auth.ts`, `src/access.ts`, `src/validation.ts`
- Replace: `src/server.ts` (full new version below)
- Test: `tests/db.test.ts`, `tests/auth.test.ts`, `tests/validation.test.ts`; modify `tests/server.test.ts`

**Interfaces:**
- Consumes: `HttpError`, helpers (Task 1); `toDay` (Task 3).
- Produces:
  - `openDatabase(path: string): Database`, `transaction<T>(db, fn: () => T): T`, `now(): string` (`src/store/db.ts`)
  - row types `Role`, `StatusCategory`, `WorkspaceRow`, `MemberRow`, `ProjectRow`, `StatusRow`, `TaskRow`, `DependencyRow` (`src/store/rows.ts`)
  - `resolveCaller(req): Promise<{ subject: string } | null>` (`src/auth.ts`)
  - `ROLES`, `atLeast(role, min)`, `requireRole(role, min)`, `workspaceRole(db, workspaceId, subject): Role | null`, `workspaceAccess(db, workspaceId, subject): Role` (404 if not a member), `projectAccess(db, projectId, subject): { project: ProjectRow; role: Role }`, `taskAccess(db, taskId, subject): { project; role; task: TaskRow }` (`src/access.ts`)
  - `object(value, allowed)`, `has(body, key)`, `field(body, key, parse)`, `nullable(body, key, parse)`, `requiredText(body, key, max)`, `text(body, key, max)`, `dateValue(key)`, `intValue(key, min, max)`, `enumValue(key, values)`, `boolValue(key)`, `subjectValue(key)`, `idValue(key)` (`src/validation.ts`)
  - `Context { req: Request; url: URL; db: Database; subject: string }`, `API_PREFIX`, `ROUTE_MODULES` (`src/server.ts`). Later tasks register routes by adding one import and one entry to `ROUTE_MODULES`.

- [ ] **Step 1: Write the failing tests**

`tests/db.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { openDatabase, transaction } from "../src/store/db";

const TABLES = [
  "calendar_exceptions",
  "dependencies",
  "project_members",
  "projects",
  "statuses",
  "tasks",
  "workspace_members",
  "workspaces",
];

describe("database", () => {
  it("creates every table at schema version 1 with foreign keys enforced", () => {
    const db = openDatabase(":memory:");
    expect((db.query("PRAGMA user_version").get() as { user_version: number }).user_version).toBe(1);
    expect((db.query("PRAGMA foreign_keys").get() as { foreign_keys: number }).foreign_keys).toBe(1);
    const tables = (db.query("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name").all() as { name: string }[]).map(
      (t) => t.name,
    );
    expect(tables).toEqual(TABLES);
    expect(() =>
      db.query("INSERT INTO workspace_members VALUES ('missing', 's', 'owner', 'x', 'x', 'x', 'x')").run(),
    ).toThrow();
    db.close();
  });

  it("keeps data and does not re-run migrations when reopened", () => {
    const dir = mkdtempSync(join(tmpdir(), "nexus-project-db-"));
    try {
      const path = join(dir, "project.sqlite");
      const first = openDatabase(path);
      first
        .query("INSERT INTO workspaces VALUES ('w1', 'Team', 'team', NULL, 'x', 'x', 's', 's')")
        .run();
      first.close();
      const second = openDatabase(path);
      expect(second.query("SELECT name FROM workspaces").all()).toEqual([{ name: "Team" }]);
      second.close();
    } finally {
      rmSync(dir, { recursive: true, force: true });
    }
  });

  it("rolls back a transaction that throws", () => {
    const db = openDatabase(":memory:");
    expect(() =>
      transaction(db, () => {
        db.query("INSERT INTO workspaces VALUES ('w1', 'Team', 'team', NULL, 'x', 'x', 's', 's')").run();
        throw new Error("boom");
      }),
    ).toThrow("boom");
    expect(db.query("SELECT COUNT(*) AS n FROM workspaces").get()).toEqual({ n: 0 });
    db.close();
  });
});
```

`tests/auth.test.ts`:

```ts
import { afterEach, describe, expect, it } from "bun:test";
import { resolveCaller } from "../src/auth";

const SECRET = "auth-test-hop-secret"; // pragma: allowlist secret

function request(headers: Record<string, string>): Request {
  return new Request("http://project.test/api/v1/project/workspaces", { headers });
}

afterEach(() => {
  delete process.env.NEXUS_PROJECT_DASHBOARD_SECRET;
});

describe("resolveCaller", () => {
  it("accepts Dashboard's subject when the deployment secret accompanies it", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    const caller = await resolveCaller(request({ "x-nexus-subject": "usr-alice", "x-nexus-dashboard-secret": SECRET }));
    expect(caller).toEqual({ subject: "usr-alice" });
  });

  it("ignores a bare x-nexus-subject, which anyone can type", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    expect(await resolveCaller(request({ "x-nexus-subject": "usr-alice" }))).toBeNull();
  });

  it("refuses a wrong secret, including one of a different length", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    for (const wrong of ["auth-test-hop-secreX", "short", `${SECRET}-longer`]) {
      expect(await resolveCaller(request({ "x-nexus-subject": "usr-alice", "x-nexus-dashboard-secret": wrong }))).toBeNull();
    }
  });

  it("refuses the hop entirely when no secret is configured", async () => {
    expect(await resolveCaller(request({ "x-nexus-subject": "usr-alice", "x-nexus-dashboard-secret": "" }))).toBeNull();
  });

  it("refuses the right secret without a subject", async () => {
    process.env.NEXUS_PROJECT_DASHBOARD_SECRET = SECRET;
    expect(await resolveCaller(request({ "x-nexus-dashboard-secret": SECRET }))).toBeNull();
  });

  it("refuses an identity token that does not verify", async () => {
    process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:9";
    expect(await resolveCaller(request({ "x-nexus-identity": "not.a.token" }))).toBeNull();
  });
});
```

`tests/validation.test.ts`:

```ts
import { describe, expect, it } from "bun:test";
import { HttpError } from "../src/http";
import { dateValue, enumValue, intValue, nullable, object, requiredText, subjectValue } from "../src/validation";

function rejects(fn: () => unknown, message: string): void {
  try {
    fn();
  } catch (error) {
    expect(error).toBeInstanceOf(HttpError);
    expect((error as HttpError).status).toBe(400);
    expect((error as HttpError).message).toContain(message);
    return;
  }
  throw new Error("expected a 400");
}

describe("validation", () => {
  it("accepts only JSON objects with known fields", () => {
    expect(object({ name: "x" }, ["name"])).toEqual({ name: "x" });
    rejects(() => object([], ["name"]), "JSON object");
    rejects(() => object(null, ["name"]), "JSON object");
    rejects(() => object({ name: "x", owner: "me" }, ["name"]), "unknown field: owner");
  });

  it("trims required text and refuses blank or oversized values", () => {
    expect(requiredText({ name: "  Launch  " }, "name", 200)).toBe("Launch");
    rejects(() => requiredText({ name: "   " }, "name", 200), "name is required");
    rejects(() => requiredText({}, "name", 200), "name is required");
    rejects(() => requiredText({ name: "x".repeat(201) }, "name", 200), "at most 200");
  });

  it("accepts real dates between 2000 and 2199 only", () => {
    expect(dateValue("due")("2026-09-24")).toBe("2026-09-24");
    rejects(() => dateValue("due")("2026-02-30"), "real YYYY-MM-DD");
    rejects(() => dateValue("due")("1999-12-31"), "between 2000 and 2199");
    rejects(() => dateValue("due")(20260924), "YYYY-MM-DD");
  });

  it("distinguishes absent, null and present for nullable fields", () => {
    const parse = dateValue("deadline");
    expect(nullable({}, "deadline", parse)).toBeUndefined();
    expect(nullable({ deadline: null }, "deadline", parse)).toBeNull();
    expect(nullable({ deadline: "2026-10-01" }, "deadline", parse)).toBe("2026-10-01");
  });

  it("checks integers, enums and subjects", () => {
    expect(intValue("lag", -10, 10)(-3)).toBe(-3);
    rejects(() => intValue("lag", -10, 10)(1.5), "integer");
    rejects(() => intValue("lag", -10, 10)(11), "from -10 to 10");
    expect(enumValue("type", ["FS", "SS"] as const)("SS")).toBe("SS");
    rejects(() => enumValue("type", ["FS", "SS"] as const)("XX"), "one of FS, SS");
    expect(subjectValue("subject")("usr-2f9a-1")).toBe("usr-2f9a-1");
    rejects(() => subjectValue("subject")("usr alice"), "Nexus subject");
  });
});
```

Add to `tests/server.test.ts`, inside the `describe` block after the last test:

```ts
  it("requires an identity for the API", async () => {
    const res = await t.raw(null, "GET", "/api/v1/project/nothing-here");
    expect(res.status).toBe(401);
    expect(res.body).toEqual({ error: "not_authenticated", message: "sign in to use Nexus Project" });
  });

  it("returns 404 for an unknown API path once authenticated", async () => {
    const res = await t.as("usr-alice").call("GET", "/nothing-here");
    expect(res.status).toBe(404);
  });
```

- [ ] **Step 2: Run to verify they fail**

Run: `cd apps/Nexus-Project && bun test tests/`
Expected: FAIL — missing modules `../src/store/db`, `../src/auth`, `../src/validation`; the new server tests fail with 404 instead of 401.

- [ ] **Step 3: Implement the store foundation**

`src/store/rows.ts`:

```ts
/** Row shapes exactly as stored. Mapped to camelCase API objects by each store module. */
export type Role = "viewer" | "member" | "admin" | "owner";
export type StatusCategory = "backlog" | "unstarted" | "started" | "completed" | "canceled";

interface Audit {
  created_at: string;
  updated_at: string;
  created_by: string;
  updated_by: string;
}

export interface WorkspaceRow extends Audit {
  id: string;
  name: string;
  kind: "personal" | "team";
  personal_subject: string | null;
}

export interface MemberRow extends Audit {
  workspace_id: string;
  subject: string;
  role: Role;
}

export interface ProjectRow extends Audit {
  id: string;
  workspace_id: string;
  key: string;
  name: string;
  description: string;
  start_date: string;
  schedule_mode: "manual" | "auto";
  visibility: "workspace" | "restricted";
  archived: number;
  next_number: number;
  working_weekdays: number;
}

export interface StatusRow extends Audit {
  id: string;
  project_id: string;
  name: string;
  category: StatusCategory;
  position: number;
}

export interface TaskRow extends Audit {
  id: string;
  project_id: string;
  number: number;
  parent_id: string | null;
  rank: string;
  title: string;
  description: string;
  status_id: string;
  priority: "none" | "low" | "medium" | "high" | "urgent";
  assignee_subject: string | null;
  kind: "task" | "milestone";
  duration_days: number | null;
  start_date: string | null;
  finish_date: string | null;
  constraint_type: "asap" | "start_no_earlier_than";
  constraint_date: string | null;
  deadline: string | null;
  progress: number;
  version: number;
}

export interface DependencyRow extends Audit {
  id: string;
  project_id: string;
  predecessor_id: string;
  successor_id: string;
  type: "FS" | "SS" | "FF" | "SF";
  lag_days: number;
}
```

`src/store/db.ts`:

```ts
import { Database } from "bun:sqlite";

const AUDIT = `created_at TEXT NOT NULL, updated_at TEXT NOT NULL,
  created_by TEXT NOT NULL, updated_by TEXT NOT NULL`;

/** Index i migrates schema version i to i + 1. Append only; never edit a shipped entry. */
const MIGRATIONS: readonly string[] = [
  `
  CREATE TABLE workspaces (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    kind TEXT NOT NULL CHECK (kind IN ('personal', 'team')),
    personal_subject TEXT UNIQUE,
    ${AUDIT},
    CHECK ((kind = 'personal') = (personal_subject IS NOT NULL))
  );
  CREATE TABLE workspace_members (
    workspace_id TEXT NOT NULL REFERENCES workspaces(id) ON DELETE CASCADE,
    subject TEXT NOT NULL,
    role TEXT NOT NULL CHECK (role IN ('owner', 'admin', 'member', 'viewer')),
    ${AUDIT},
    PRIMARY KEY (workspace_id, subject)
  );
  CREATE INDEX workspace_members_subject ON workspace_members (subject);
  CREATE TABLE projects (
    id TEXT PRIMARY KEY,
    workspace_id TEXT NOT NULL REFERENCES workspaces(id) ON DELETE CASCADE,
    key TEXT NOT NULL,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    start_date TEXT NOT NULL,
    schedule_mode TEXT NOT NULL DEFAULT 'manual' CHECK (schedule_mode IN ('manual', 'auto')),
    visibility TEXT NOT NULL DEFAULT 'workspace' CHECK (visibility IN ('workspace', 'restricted')),
    archived INTEGER NOT NULL DEFAULT 0 CHECK (archived IN (0, 1)),
    next_number INTEGER NOT NULL DEFAULT 1,
    working_weekdays INTEGER NOT NULL DEFAULT 62 CHECK (working_weekdays BETWEEN 1 AND 127),
    ${AUDIT},
    UNIQUE (workspace_id, key)
  );
  CREATE TABLE project_members (
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    subject TEXT NOT NULL,
    created_at TEXT NOT NULL,
    created_by TEXT NOT NULL,
    PRIMARY KEY (project_id, subject)
  );
  CREATE TABLE calendar_exceptions (
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    date TEXT NOT NULL,
    working INTEGER NOT NULL CHECK (working IN (0, 1)),
    PRIMARY KEY (project_id, date)
  );
  CREATE TABLE statuses (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    category TEXT NOT NULL CHECK (category IN ('backlog', 'unstarted', 'started', 'completed', 'canceled')),
    position INTEGER NOT NULL,
    ${AUDIT},
    UNIQUE (project_id, name)
  );
  CREATE TABLE tasks (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    number INTEGER NOT NULL,
    parent_id TEXT REFERENCES tasks(id) ON DELETE CASCADE,
    rank TEXT NOT NULL,
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status_id TEXT NOT NULL REFERENCES statuses(id),
    priority TEXT NOT NULL DEFAULT 'none' CHECK (priority IN ('none', 'low', 'medium', 'high', 'urgent')),
    assignee_subject TEXT,
    kind TEXT NOT NULL DEFAULT 'task' CHECK (kind IN ('task', 'milestone')),
    duration_days INTEGER CHECK (duration_days IS NULL OR duration_days >= 0),
    start_date TEXT,
    finish_date TEXT,
    constraint_type TEXT NOT NULL DEFAULT 'asap' CHECK (constraint_type IN ('asap', 'start_no_earlier_than')),
    constraint_date TEXT,
    deadline TEXT,
    progress INTEGER NOT NULL DEFAULT 0 CHECK (progress BETWEEN 0 AND 100),
    version INTEGER NOT NULL DEFAULT 1,
    ${AUDIT},
    UNIQUE (project_id, number)
  );
  CREATE INDEX tasks_project_rank ON tasks (project_id, rank);
  CREATE INDEX tasks_parent ON tasks (parent_id);
  CREATE INDEX tasks_assignee ON tasks (assignee_subject);
  CREATE TABLE dependencies (
    id TEXT PRIMARY KEY,
    project_id TEXT NOT NULL REFERENCES projects(id) ON DELETE CASCADE,
    predecessor_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
    successor_id TEXT NOT NULL REFERENCES tasks(id) ON DELETE CASCADE,
    type TEXT NOT NULL CHECK (type IN ('FS', 'SS', 'FF', 'SF')),
    lag_days INTEGER NOT NULL DEFAULT 0,
    ${AUDIT},
    UNIQUE (predecessor_id, successor_id),
    CHECK (predecessor_id <> successor_id)
  );
  CREATE INDEX dependencies_project ON dependencies (project_id);
  CREATE INDEX dependencies_successor ON dependencies (successor_id);
  `,
];

export function openDatabase(path: string): Database {
  const db = new Database(path, { create: true });
  db.exec("PRAGMA foreign_keys = ON");
  if (path !== ":memory:") db.exec("PRAGMA journal_mode = WAL");
  migrate(db);
  return db;
}

function migrate(db: Database): void {
  const current = (db.query("PRAGMA user_version").get() as { user_version: number }).user_version;
  for (let version = current; version < MIGRATIONS.length; version++) {
    // One transaction per step: a half-applied schema is not recoverable.
    db.exec("BEGIN IMMEDIATE");
    try {
      db.exec(MIGRATIONS[version] as string);
      db.exec(`PRAGMA user_version = ${version + 1}`);
      db.exec("COMMIT");
    } catch (error) {
      db.exec("ROLLBACK");
      throw error;
    }
  }
}

/** Runs `fn` atomically. Nested calls become savepoints. A throw rolls back. */
export function transaction<T>(db: Database, fn: () => T): T {
  return db.transaction(fn).immediate();
}

export function now(): string {
  return new Date().toISOString();
}
```

- [ ] **Step 4: Implement identity, access and validation**

`src/auth.ts`:

```ts
/**
 * Who is asking. The same contract as Nexus-Calendar: identity comes from a
 * trusted front door, from exactly two places the browser cannot forge.
 *
 *   1. `x-nexus-identity` — an RS256 token the ecosystem proxy mints after
 *      checking the session, verified by @nexus/identity (issuer, typ and this
 *      service's audience all checked).
 *   2. `x-nexus-subject` — the subject Dashboard resolved through Auth on its
 *      private loopback hop, believed ONLY alongside the deployment secret,
 *      because unlike (1) this header is a bare string anyone could type.
 *
 * Anything else is anonymous.
 */
import { timingSafeEqual } from "node:crypto";
import { IDENTITY_HEADER, verifyIdentityToken } from "../../../packages/nexus-identity/src/index";

export interface Caller {
  subject: string;
}

/** Read per call, never at module load, so tests can point them elsewhere. */
function jwksUrl(): string {
  const base = (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
  return `${base}/api/v1/auth/oauth/jwks`;
}

function expectedAudience(): string {
  return process.env.NEXUS_PROJECT_JWT_AUDIENCE || "project.tnhc.dev";
}

/** Constant-time comparison that does not leak the length through an early return. */
function secretMatches(presented: string, configured: string): boolean {
  const a = Buffer.from(presented);
  const b = Buffer.from(configured);
  if (a.length !== b.length) {
    timingSafeEqual(b, b);
    return false;
  }
  return timingSafeEqual(a, b);
}

export async function resolveCaller(req: Request): Promise<Caller | null> {
  const configured = process.env.NEXUS_PROJECT_DASHBOARD_SECRET;
  if (configured) {
    const presented = req.headers.get("x-nexus-dashboard-secret");
    if (presented !== null && secretMatches(presented, configured)) {
      const subject = req.headers.get("x-nexus-subject")?.trim();
      // The right secret with no subject is a Dashboard bug, not a caller.
      return subject ? { subject } : null;
    }
  }

  const token = req.headers.get(IDENTITY_HEADER);
  if (token) {
    const result = await verifyIdentityToken(token, { audience: expectedAudience(), jwksUrl: jwksUrl() });
    if (result.ok) return { subject: result.claims.sub };
    // Logged for the operator, never returned to whoever sent the token.
    console.warn(`[nexus-project] identity token refused: ${result.reason}`);
  }
  return null;
}
```

`src/access.ts`:

```ts
import type { Database } from "bun:sqlite";
import { forbidden, notFound } from "./http";
import type { ProjectRow, Role, TaskRow } from "./store/rows";

export type { Role } from "./store/rows";

export const ROLES: readonly Role[] = ["viewer", "member", "admin", "owner"];
const RANK: Record<Role, number> = { viewer: 0, member: 1, admin: 2, owner: 3 };

export function atLeast(role: Role, minimum: Role): boolean {
  return RANK[role] >= RANK[minimum];
}

/** Seen but not permitted: 403. (Not seen at all is decided earlier, as 404.) */
export function requireRole(role: Role, minimum: Role): void {
  if (!atLeast(role, minimum)) throw forbidden();
}

export function workspaceRole(db: Database, workspaceId: string, subject: string): Role | null {
  const row = db
    .query("SELECT role FROM workspace_members WHERE workspace_id = ? AND subject = ?")
    .get(workspaceId, subject) as { role: Role } | null;
  return row ? row.role : null;
}

/** The caller's role in a workspace, or 404 so non-members cannot probe ids. */
export function workspaceAccess(db: Database, workspaceId: string, subject: string): Role {
  const role = workspaceRole(db, workspaceId, subject);
  if (!role) throw notFound("workspace");
  return role;
}

export interface ProjectAccess {
  project: ProjectRow;
  role: Role;
}

export function canSeeProject(db: Database, project: ProjectRow, subject: string, role: Role | null): boolean {
  if (!role) return false;
  if (project.visibility === "workspace" || atLeast(role, "admin")) return true;
  return Boolean(
    db.query("SELECT 1 FROM project_members WHERE project_id = ? AND subject = ?").get(project.id, subject),
  );
}

export function projectAccess(db: Database, projectId: string, subject: string): ProjectAccess {
  const project = db.query("SELECT * FROM projects WHERE id = ?").get(projectId) as ProjectRow | null;
  if (!project) throw notFound("project");
  const role = workspaceRole(db, project.workspace_id, subject);
  if (!role || !canSeeProject(db, project, subject, role)) throw notFound("project");
  return { project, role };
}

export interface TaskAccess extends ProjectAccess {
  task: TaskRow;
}

export function taskAccess(db: Database, taskId: string, subject: string): TaskAccess {
  const task = db.query("SELECT * FROM tasks WHERE id = ?").get(taskId) as TaskRow | null;
  if (!task) throw notFound("task");
  const project = db.query("SELECT * FROM projects WHERE id = ?").get(task.project_id) as ProjectRow;
  const role = workspaceRole(db, project.workspace_id, subject);
  if (!role || !canSeeProject(db, project, subject, role)) throw notFound("task");
  return { project, role, task };
}
```

`src/validation.ts`:

```ts
import { badRequest } from "./http";
import { toDay } from "./schedule/calendar";

export type Body = Record<string, unknown>;
type Parse<T> = (value: unknown) => T;

/** A plain JSON object containing only `allowed` keys. Anything else is a 400. */
export function object(value: unknown, allowed: readonly string[]): Body {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    throw badRequest("body must be a JSON object");
  }
  const body = value as Body;
  for (const key of Object.keys(body)) {
    if (!allowed.includes(key)) throw badRequest(`unknown field: ${key}`);
  }
  return body;
}

export function has(body: Body, key: string): boolean {
  return Object.hasOwn(body, key);
}

/** Absent → undefined; present → parsed (null is refused by most parsers). */
export function field<T>(body: Body, key: string, parse: Parse<T>): T | undefined {
  return has(body, key) ? parse(body[key]) : undefined;
}

/** Absent → undefined; null → null (clear it); otherwise parsed. */
export function nullable<T>(body: Body, key: string, parse: Parse<T>): T | null | undefined {
  if (!has(body, key)) return undefined;
  return body[key] === null ? null : parse(body[key]);
}

export function text(body: Body, key: string, max: number): string | undefined {
  return field(body, key, (value) => {
    if (typeof value !== "string") throw badRequest(`${key} must be a string`);
    if (value.length > max) throw badRequest(`${key} must be at most ${max} characters`);
    return value;
  });
}

export function requiredText(body: Body, key: string, max: number): string {
  const value = text(body, key, max)?.trim();
  if (!value) throw badRequest(`${key} is required`);
  return value;
}

export function dateValue(key: string): Parse<string> {
  return (value) => {
    if (typeof value !== "string") throw badRequest(`${key} must be a YYYY-MM-DD date`);
    try {
      toDay(value);
    } catch {
      throw badRequest(`${key} must be a real YYYY-MM-DD date`);
    }
    if (value < "2000-01-01" || value > "2199-12-31") throw badRequest(`${key} must be between 2000 and 2199`);
    return value;
  };
}

export function intValue(key: string, min: number, max: number): Parse<number> {
  return (value) => {
    if (typeof value !== "number" || !Number.isInteger(value) || value < min || value > max) {
      throw badRequest(`${key} must be an integer from ${min} to ${max}`);
    }
    return value;
  };
}

export function enumValue<T extends string>(key: string, values: readonly T[]): Parse<T> {
  return (value) => {
    if (typeof value !== "string" || !values.includes(value as T)) {
      throw badRequest(`${key} must be one of ${values.join(", ")}`);
    }
    return value as T;
  };
}

export function boolValue(key: string): Parse<boolean> {
  return (value) => {
    if (typeof value !== "boolean") throw badRequest(`${key} must be true or false`);
    return value;
  };
}

const SUBJECT = /^[A-Za-z0-9._:@-]{1,200}$/;

export function subjectValue(key: string): Parse<string> {
  return (value) => {
    if (typeof value !== "string" || !SUBJECT.test(value)) throw badRequest(`${key} must be a Nexus subject`);
    return value;
  };
}

const UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

export function idValue(key: string): Parse<string> {
  return (value) => {
    if (typeof value !== "string" || !UUID.test(value)) throw badRequest(`${key} must be an id`);
    return value;
  };
}
```

- [ ] **Step 5: Replace the server**

`src/server.ts`:

```ts
import type { Database } from "bun:sqlite";
import { mkdirSync } from "node:fs";
import { dirname } from "node:path";
import { resolveCaller } from "./auth";
import { startHeartbeat } from "./cloud";
import { HttpError, errorResponse, json, notFound } from "./http";
import { Router } from "./router";
import { openDatabase } from "./store/db";

export const API_PREFIX = "/api/v1/project";

export interface Context {
  req: Request;
  url: URL;
  db: Database;
  subject: string;
}

/** Each later task appends its register function here. */
const ROUTE_MODULES: ((router: Router<Context>) => void)[] = [];

export async function createServer() {
  const port = Number(process.env.PORT || "3152");
  const baseUrl = process.env.NEXUS_PROJECT_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const dbPath = process.env.NEXUS_PROJECT_DB || "data/project.sqlite";
  if (dbPath !== ":memory:") mkdirSync(dirname(dbPath), { recursive: true });
  const db = openDatabase(dbPath);

  const router = new Router<Context>();
  for (const register of ROUTE_MODULES) register(router);

  const server = Bun.serve({
    port,
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const path = url.pathname;

      // Public: liveness and capabilities only. Neither reads project data.
      if (req.method === "GET" && path === "/health") {
        return json({ service: "nexus-project", status: "ok", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      }
      if (req.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-project", status: "ready", capabilities: ["projects", "tasks", "scheduling"] });
      }
      if (path !== API_PREFIX && !path.startsWith(`${API_PREFIX}/`)) return errorResponse(notFound());

      // Identity before routing: an anonymous caller learns nothing, not even
      // which paths exist.
      const caller = await resolveCaller(req);
      if (!caller) return errorResponse(new HttpError(401, "not_authenticated", "sign in to use Nexus Project"));

      try {
        const match = router.match(req.method, path.slice(API_PREFIX.length));
        if (!match) throw notFound();
        if ("allowed" in match) {
          return json({ error: "method_not_allowed", message: `use ${match.allowed.join(", ")}` }, 405, {
            allow: match.allowed.join(", "),
          });
        }
        return await match.handler({ req, url, db, subject: caller.subject }, match.params);
      } catch (error) {
        if (error instanceof HttpError) return errorResponse(error);
        // Internal detail goes to the operator's log, never to the client.
        console.error(`[nexus-project] ${req.method} ${path} failed:`, error);
        return json({ error: "internal", message: "internal error" }, 500);
      }
    },
  });

  console.log(`[nexus-project] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    db,
    close: () => {
      stopHeartbeat();
      server.stop(true);
      db.close();
    },
  };
}
```

- [ ] **Step 6: Run the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: tsc clean; all test files ran; `PASS`.

- [ ] **Step 7: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): database, trusted identity, access rules and validation

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Workspaces and members

**Files:**
- Create: `apps/Nexus-Project/src/store/workspaces.ts`, `src/routes/workspaces.ts`
- Modify: `src/server.ts` (register the routes)
- Test: `tests/workspaces.test.ts`

**Interfaces:**
- Consumes: Task 7 (`transaction`, `now`, access helpers, validation, `Context`).
- Produces: `ensurePersonalWorkspace(db, subject)`, `listWorkspaces(db, subject): Workspace[]`, `createTeamWorkspace(db, subject, name)`, `renameWorkspace`, `deleteWorkspace`, `listMembers`, `setMember(db, subject, workspaceId, target, role): Member`, `removeMember`; `Workspace { id, name, kind, role, createdAt, updatedAt }`, `Member { subject, role, createdAt, updatedAt }`; `registerWorkspaceRoutes(router)`.

Routes: `GET /workspaces`, `POST /workspaces`, `PATCH /workspaces/:id`, `DELETE /workspaces/:id`, `GET /workspaces/:id/members`, `PUT /workspaces/:id/members/:subject`, `DELETE /workspaces/:id/members/:subject`.

Rules: a personal workspace is created on first `GET /workspaces`, is named "Personal", has exactly one member (owner), and cannot gain members or be deleted (`422 personal_workspace`). Renaming needs admin. Adding or changing members needs admin; granting `owner`, or changing an existing owner, needs owner. Anyone may remove themselves; removing someone else needs admin (owner, if the target is an owner). A workspace always keeps at least one owner (`422 last_owner`). Deleting a team workspace needs owner and removes everything in it.

- [ ] **Step 1: Write the failing test**

`tests/workspaces.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
beforeAll(async () => {
  t = await startTestServer();
});
afterAll(() => t.close());

const owner = () => t.as("usr-owner");
const admin = () => t.as("usr-admin");
const member = () => t.as("usr-member");
const viewer = () => t.as("usr-viewer");
const stranger = () => t.as("usr-stranger");

async function team(name = "Studio"): Promise<string> {
  const res = await owner().call("POST", "/workspaces", { name });
  expect(res.status).toBe(201);
  const id = res.body.id as string;
  for (const [subject, role] of [["usr-admin", "admin"], ["usr-member", "member"], ["usr-viewer", "viewer"]] as const) {
    expect((await owner().call("PUT", `/workspaces/${id}/members/${subject}`, { role })).status).toBe(200);
  }
  return id;
}

describe("personal workspace", () => {
  it("is created once, on first use, with the caller as its only owner", async () => {
    const first = await t.as("usr-solo").call("GET", "/workspaces");
    const second = await t.as("usr-solo").call("GET", "/workspaces");
    expect(first.status).toBe(200);
    expect(first.body.workspaces).toEqual(second.body.workspaces);
    expect(first.body.workspaces).toHaveLength(1);
    expect(first.body.workspaces[0]).toMatchObject({ name: "Personal", kind: "personal", role: "owner" });
    const members = await t.as("usr-solo").call("GET", `/workspaces/${first.body.workspaces[0].id}/members`);
    expect(members.body.members.map((m: { subject: string }) => m.subject)).toEqual(["usr-solo"]);
  });

  it("cannot gain members or be deleted", async () => {
    const id = (await t.as("usr-solo").call("GET", "/workspaces")).body.workspaces[0].id;
    const add = await t.as("usr-solo").call("PUT", `/workspaces/${id}/members/usr-friend`, { role: "member" });
    expect(add.status).toBe(422);
    expect(add.body.error).toBe("personal_workspace");
    expect((await t.as("usr-solo").call("DELETE", `/workspaces/${id}`)).body.error).toBe("personal_workspace");
  });
});

describe("team workspace", () => {
  it("lists the personal workspace first and the team with the caller's role", async () => {
    const id = await team("Alpha");
    const list = (await member().call("GET", "/workspaces")).body.workspaces;
    expect(list[0].kind).toBe("personal");
    expect(list.find((w: { id: string }) => w.id === id)).toMatchObject({ name: "Alpha", kind: "team", role: "member" });
  });

  it("hides itself from non-members", async () => {
    const id = await team();
    expect((await stranger().call("GET", `/workspaces/${id}/members`)).status).toBe(404);
    expect((await stranger().call("PATCH", `/workspaces/${id}`, { name: "Mine" })).status).toBe(404);
  });

  it("lets admins rename but not members or viewers", async () => {
    const id = await team();
    expect((await viewer().call("PATCH", `/workspaces/${id}`, { name: "X" })).status).toBe(403);
    expect((await member().call("PATCH", `/workspaces/${id}`, { name: "X" })).status).toBe(403);
    const renamed = await admin().call("PATCH", `/workspaces/${id}`, { name: "Renamed" });
    expect(renamed.status).toBe(200);
    expect(renamed.body.name).toBe("Renamed");
  });

  it("lets admins manage members below owner", async () => {
    const id = await team();
    expect((await member().call("PUT", `/workspaces/${id}/members/usr-new`, { role: "member" })).status).toBe(403);
    expect((await admin().call("PUT", `/workspaces/${id}/members/usr-new`, { role: "member" })).status).toBe(200);
    expect((await admin().call("PUT", `/workspaces/${id}/members/usr-new`, { role: "owner" })).status).toBe(403);
    expect((await admin().call("PUT", `/workspaces/${id}/members/usr-owner`, { role: "member" })).status).toBe(403);
    expect((await admin().call("DELETE", `/workspaces/${id}/members/usr-new`)).status).toBe(200);
  });

  it("always keeps an owner", async () => {
    const id = await team();
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-owner`, { role: "admin" })).body.error).toBe("last_owner");
    expect((await owner().call("DELETE", `/workspaces/${id}/members/usr-owner`)).body.error).toBe("last_owner");
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-admin`, { role: "owner" })).status).toBe(200);
    expect((await owner().call("DELETE", `/workspaces/${id}/members/usr-owner`)).status).toBe(200);
  });

  it("lets anyone leave but not remove others without admin", async () => {
    const id = await team();
    expect((await member().call("DELETE", `/workspaces/${id}/members/usr-viewer`)).status).toBe(403);
    expect((await viewer().call("DELETE", `/workspaces/${id}/members/usr-viewer`)).status).toBe(200);
    expect((await viewer().call("GET", `/workspaces/${id}/members`)).status).toBe(404);
  });

  it("can only be deleted by an owner", async () => {
    const id = await team();
    expect((await admin().call("DELETE", `/workspaces/${id}`)).status).toBe(403);
    expect((await owner().call("DELETE", `/workspaces/${id}`)).status).toBe(200);
    expect((await owner().call("GET", `/workspaces/${id}/members`)).status).toBe(404);
  });

  it("validates its input", async () => {
    const id = await team();
    expect((await owner().call("POST", "/workspaces", { name: " " })).status).toBe(400);
    expect((await owner().call("POST", "/workspaces", { name: "A", kind: "personal" })).status).toBe(400);
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-x`, { role: "god" })).status).toBe(400);
    expect((await owner().call("PUT", `/workspaces/${id}/members/usr-x`, {})).status).toBe(400);
    expect((await owner().call("PUT", `/workspaces/${id}/members/${encodeURIComponent("usr x")}`, { role: "member" })).status).toBe(400);
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/workspaces.test.ts`
Expected: FAIL — requests return 404 (no routes registered).

- [ ] **Step 3: Implement the store**

`src/store/workspaces.ts`:

```ts
import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { requireRole, workspaceAccess, workspaceRole } from "../access";
import { forbidden, notFound, unprocessable } from "../http";
import { now, transaction } from "./db";
import type { MemberRow, Role, WorkspaceRow } from "./rows";

export interface Workspace {
  id: string;
  name: string;
  kind: "personal" | "team";
  role: Role;
  createdAt: string;
  updatedAt: string;
}

export interface Member {
  subject: string;
  role: Role;
  createdAt: string;
  updatedAt: string;
}

function toWorkspace(row: WorkspaceRow, role: Role): Workspace {
  return { id: row.id, name: row.name, kind: row.kind, role, createdAt: row.created_at, updatedAt: row.updated_at };
}

function toMember(row: MemberRow): Member {
  return { subject: row.subject, role: row.role, createdAt: row.created_at, updatedAt: row.updated_at };
}

function workspaceRow(db: Database, id: string): WorkspaceRow {
  const row = db.query("SELECT * FROM workspaces WHERE id = ?").get(id) as WorkspaceRow | null;
  if (!row) throw notFound("workspace");
  return row;
}

function ownerCount(db: Database, id: string): number {
  return (db.query("SELECT COUNT(*) AS n FROM workspace_members WHERE workspace_id = ? AND role = 'owner'").get(id) as { n: number }).n;
}

function insertMember(db: Database, workspaceId: string, subject: string, role: Role, by: string): void {
  const at = now();
  db.query(
    `INSERT INTO workspace_members (workspace_id, subject, role, created_at, updated_at, created_by, updated_by)
     VALUES (?, ?, ?, ?, ?, ?, ?)
     ON CONFLICT (workspace_id, subject) DO UPDATE SET
       role = excluded.role, updated_at = excluded.updated_at, updated_by = excluded.updated_by`,
  ).run(workspaceId, subject, role, at, at, by, by);
}

const PERSONAL = "a personal workspace has exactly one member; move the project into a team workspace to collaborate";

/** Every subject gets a personal workspace on first use, so a solo user never sees setup. */
export function ensurePersonalWorkspace(db: Database, subject: string): void {
  if (db.query("SELECT 1 FROM workspaces WHERE personal_subject = ?").get(subject)) return;
  transaction(db, () => {
    const id = randomUUID();
    const at = now();
    // OR IGNORE: two first requests racing must still produce one workspace.
    const inserted = db
      .query(
        `INSERT OR IGNORE INTO workspaces (id, name, kind, personal_subject, created_at, updated_at, created_by, updated_by)
         VALUES (?, 'Personal', 'personal', ?, ?, ?, ?, ?)`,
      )
      .run(id, subject, at, at, subject, subject);
    if (inserted.changes === 1) insertMember(db, id, subject, "owner", subject);
  });
}

export function listWorkspaces(db: Database, subject: string): Workspace[] {
  const rows = db
    .query(
      `SELECT w.*, m.role AS member_role FROM workspaces w
       JOIN workspace_members m ON m.workspace_id = w.id
       WHERE m.subject = ?
       ORDER BY w.kind = 'team', w.name COLLATE NOCASE, w.id`,
    )
    .all(subject) as (WorkspaceRow & { member_role: Role })[];
  return rows.map((row) => toWorkspace(row, row.member_role));
}

export function createTeamWorkspace(db: Database, subject: string, name: string): Workspace {
  return transaction(db, () => {
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO workspaces (id, name, kind, personal_subject, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, 'team', NULL, ?, ?, ?, ?)`,
    ).run(id, name, at, at, subject, subject);
    insertMember(db, id, subject, "owner", subject);
    return toWorkspace(workspaceRow(db, id), "owner");
  });
}

export function renameWorkspace(db: Database, subject: string, id: string, name: string): Workspace {
  const role = workspaceAccess(db, id, subject);
  requireRole(role, "admin");
  db.query("UPDATE workspaces SET name = ?, updated_at = ?, updated_by = ? WHERE id = ?").run(name, now(), subject, id);
  return toWorkspace(workspaceRow(db, id), role);
}

export function deleteWorkspace(db: Database, subject: string, id: string): void {
  const role = workspaceAccess(db, id, subject);
  requireRole(role, "owner");
  if (workspaceRow(db, id).kind === "personal") throw unprocessable("personal_workspace", "a personal workspace cannot be deleted");
  db.query("DELETE FROM workspaces WHERE id = ?").run(id);
}

export function listMembers(db: Database, subject: string, id: string): Member[] {
  workspaceAccess(db, id, subject);
  const rows = db.query("SELECT * FROM workspace_members WHERE workspace_id = ? ORDER BY subject").all(id) as MemberRow[];
  return rows.map(toMember);
}

export function setMember(db: Database, subject: string, id: string, target: string, role: Role): Member {
  const callerRole = workspaceAccess(db, id, subject);
  requireRole(callerRole, "admin");
  if (workspaceRow(db, id).kind === "personal") throw unprocessable("personal_workspace", PERSONAL);
  return transaction(db, () => {
    const current = workspaceRole(db, id, target);
    if ((role === "owner" || current === "owner") && callerRole !== "owner") throw forbidden();
    if (current === "owner" && role !== "owner" && ownerCount(db, id) === 1) {
      throw unprocessable("last_owner", "a workspace must keep at least one owner");
    }
    insertMember(db, id, target, role, subject);
    const row = db
      .query("SELECT * FROM workspace_members WHERE workspace_id = ? AND subject = ?")
      .get(id, target) as MemberRow;
    return toMember(row);
  });
}

export function removeMember(db: Database, subject: string, id: string, target: string): void {
  const callerRole = workspaceAccess(db, id, subject);
  transaction(db, () => {
    const current = workspaceRole(db, id, target);
    if (!current) throw notFound("member");
    if (target !== subject) {
      requireRole(callerRole, "admin");
      if (current === "owner" && callerRole !== "owner") throw forbidden();
    }
    if (current === "owner" && ownerCount(db, id) === 1) {
      throw unprocessable("last_owner", "a workspace must keep at least one owner");
    }
    db.query("DELETE FROM workspace_members WHERE workspace_id = ? AND subject = ?").run(id, target);
    // Access to restricted projects in this workspace goes with the membership.
    db.query(
      "DELETE FROM project_members WHERE subject = ? AND project_id IN (SELECT id FROM projects WHERE workspace_id = ?)",
    ).run(target, id);
  });
}
```

- [ ] **Step 4: Implement the routes and register them**

`src/routes/workspaces.ts`:

```ts
import { ROLES } from "../access";
import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as workspaces from "../store/workspaces";
import { enumValue, field, object, requiredText, subjectValue } from "../validation";

export function registerWorkspaceRoutes(router: Router<Context>): void {
  router.add("GET", "/workspaces", ({ db, subject }) => {
    workspaces.ensurePersonalWorkspace(db, subject);
    return json({ workspaces: workspaces.listWorkspaces(db, subject) });
  });

  router.add("POST", "/workspaces", async ({ db, subject, req }) => {
    const body = object(await readJson(req), ["name"]);
    return json(workspaces.createTeamWorkspace(db, subject, requiredText(body, "name", 200)), 201);
  });

  router.add("PATCH", "/workspaces/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name"]);
    return json(workspaces.renameWorkspace(db, subject, param(params, "id"), requiredText(body, "name", 200)));
  });

  router.add("DELETE", "/workspaces/:id", ({ db, subject }, params) => {
    workspaces.deleteWorkspace(db, subject, param(params, "id"));
    return json({ deleted: true });
  });

  router.add("GET", "/workspaces/:id/members", ({ db, subject }, params) =>
    json({ members: workspaces.listMembers(db, subject, param(params, "id")) }),
  );

  router.add("PUT", "/workspaces/:id/members/:subject", async ({ db, subject, req }, params) => {
    const target = subjectValue("subject")(param(params, "subject"));
    const body = object(await readJson(req), ["role"]);
    const role = field(body, "role", enumValue("role", ROLES));
    if (role === undefined) throw badRequest("role is required");
    return json(workspaces.setMember(db, subject, param(params, "id"), target, role));
  });

  router.add("DELETE", "/workspaces/:id/members/:subject", ({ db, subject }, params) => {
    const target = subjectValue("subject")(param(params, "subject"));
    workspaces.removeMember(db, subject, param(params, "id"), target);
    return json({ deleted: true });
  });
}
```

In `src/server.ts`, add the import after the other imports and the entry to `ROUTE_MODULES`:

```ts
import { registerWorkspaceRoutes } from "./routes/workspaces";
```

```ts
const ROUTE_MODULES: ((router: Router<Context>) => void)[] = [registerWorkspaceRoutes];
```

- [ ] **Step 5: Run the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: `PASS`; `tests/workspaces.test.ts` 10 tests pass.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): personal and team workspaces with roles

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---
### Task 9: Projects, restricted visibility and moving between workspaces

**Files:**
- Create: `apps/Nexus-Project/src/store/projects.ts`, `src/routes/projects.ts`
- Modify: `src/server.ts` (register)
- Test: `tests/projects.test.ts`, `tests/support/fixtures.ts`

**Interfaces:**
- Consumes: Tasks 7–8.
- Produces: `Project { id, workspaceId, key, name, description, startDate, scheduleMode, visibility, archived, role, createdAt, updatedAt }`, `toProject(row, role)`, `createProject(db, subject, workspaceId, input: ProjectInput)`, `listProjects(db, subject, workspaceId, includeArchived)`, `getProject`, `updateProject(db, subject, id, patch: ProjectPatch)`, `moveProject(db, subject, id, targetWorkspaceId)`, `listProjectMembers`, `addProjectMember`, `removeProjectMember`, `DEFAULT_STATUSES`; `registerProjectRoutes(router)`. Test fixtures `teamWithRoles(t)` and `createProject(client, workspaceId, overrides?)` in `tests/support/fixtures.ts`.

Routes: `GET/POST /workspaces/:id/projects` (`?archived=true` includes archived), `GET/PATCH /projects/:id`, `POST /projects/:id/move`, `GET /projects/:id/members`, `PUT/DELETE /projects/:id/members/:subject`.

Rules: members and above create projects; admins and above change settings, move, and manage restricted-project members. A new project gets five statuses: Backlog (backlog), Todo (unstarted), In Progress (started), Done (completed), Canceled (canceled). Keys match `^[A-Z][A-Z0-9]{1,9}$` and are unique per workspace (`409 key_taken`). A restricted project is visible to workspace owners and admins and its listed members; its creator is listed automatically. Only workspace members can be listed (`422 not_workspace_member`). Moving needs admin in both workspaces; listed members who are not in the target workspace are dropped.

- [ ] **Step 1: Write the fixtures and the failing test**

`tests/support/fixtures.ts`:

```ts
import { expect } from "bun:test";
import type { Client, startTestServer } from "./server";

type Server = Awaited<ReturnType<typeof startTestServer>>;

/** A team workspace owned by usr-owner with one member per other role. */
export async function teamWithRoles(t: Server, name = "Studio"): Promise<string> {
  const created = await t.as("usr-owner").call("POST", "/workspaces", { name });
  expect(created.status).toBe(201);
  const id = created.body.id as string;
  for (const [subject, role] of [["usr-admin", "admin"], ["usr-member", "member"], ["usr-viewer", "viewer"]] as const) {
    const res = await t.as("usr-owner").call("PUT", `/workspaces/${id}/members/${subject}`, { role });
    expect(res.status).toBe(200);
  }
  return id;
}

let keyCounter = 0;

/** Creates a project and returns its body. Keys are unique per call. */
export async function createProject(client: Client, workspaceId: string, overrides: Record<string, unknown> = {}) {
  keyCounter += 1;
  const res = await client.call("POST", `/workspaces/${workspaceId}/projects`, {
    key: `P${keyCounter}`,
    name: `Project ${keyCounter}`,
    startDate: "2026-09-07",
    ...overrides,
  });
  expect(res.status).toBe(201);
  return res.body;
}
```

`tests/projects.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
beforeAll(async () => {
  t = await startTestServer();
});
afterAll(() => t.close());

describe("projects", () => {
  it("lets a member create one, seeded with a status per category", async () => {
    const ws = await teamWithRoles(t);
    const project = await createProject(t.as("usr-member"), ws, { key: "WEB", name: "Website" });
    expect(project).toMatchObject({
      key: "WEB",
      name: "Website",
      startDate: "2026-09-07",
      scheduleMode: "manual",
      visibility: "workspace",
      archived: false,
      role: "member",
    });
    const statuses = await t.as("usr-viewer").call("GET", `/projects/${project.id}/statuses`);
    // Statuses routes arrive in Task 10; until then this is a 404 and the next line is skipped.
    if (statuses.status === 200) {
      expect(statuses.body.statuses.map((s: { category: string }) => s.category)).toEqual([
        "backlog",
        "unstarted",
        "started",
        "completed",
        "canceled",
      ]);
    }
  });

  it("refuses viewers and strangers", async () => {
    const ws = await teamWithRoles(t);
    const body = { key: "NOPE", name: "No", startDate: "2026-09-07" };
    expect((await t.as("usr-viewer").call("POST", `/workspaces/${ws}/projects`, body)).status).toBe(403);
    expect((await t.as("usr-stranger").call("POST", `/workspaces/${ws}/projects`, body)).status).toBe(404);
  });

  it("keeps keys well-formed and unique per workspace", async () => {
    const ws = await teamWithRoles(t);
    const other = await teamWithRoles(t, "Other");
    await createProject(t.as("usr-owner"), ws, { key: "OPS" });
    const dup = await t.as("usr-owner").call("POST", `/workspaces/${ws}/projects`, { key: "OPS", name: "x", startDate: "2026-09-07" });
    expect(dup.status).toBe(409);
    expect(dup.body.error).toBe("key_taken");
    await createProject(t.as("usr-owner"), other, { key: "OPS" });
    for (const key of ["ops", "O", "1OPS", "TOOLONGKEY1", "OP-S"]) {
      expect((await t.as("usr-owner").call("POST", `/workspaces/${ws}/projects`, { key, name: "x", startDate: "2026-09-07" })).status).toBe(400);
    }
  });

  it("hides a restricted project from members who are not listed", async () => {
    const ws = await teamWithRoles(t);
    const secret = await createProject(t.as("usr-member"), ws, { visibility: "restricted" });
    expect((await t.as("usr-member").call("GET", `/projects/${secret.id}`)).status).toBe(200);
    expect((await t.as("usr-admin").call("GET", `/projects/${secret.id}`)).status).toBe(200);
    expect((await t.as("usr-viewer").call("GET", `/projects/${secret.id}`)).status).toBe(404);
    const listed = await t.as("usr-viewer").call("GET", `/workspaces/${ws}/projects`);
    expect(listed.body.projects.some((p: { id: string }) => p.id === secret.id)).toBe(false);

    expect((await t.as("usr-member").call("PUT", `/projects/${secret.id}/members/usr-viewer`)).status).toBe(403);
    expect((await t.as("usr-admin").call("PUT", `/projects/${secret.id}/members/usr-viewer`)).status).toBe(200);
    expect((await t.as("usr-viewer").call("GET", `/projects/${secret.id}`)).status).toBe(200);
    const outsider = await t.as("usr-admin").call("PUT", `/projects/${secret.id}/members/usr-stranger`);
    expect(outsider.status).toBe(422);
    expect(outsider.body.error).toBe("not_workspace_member");
    expect((await t.as("usr-admin").call("DELETE", `/projects/${secret.id}/members/usr-viewer`)).status).toBe(200);
    expect((await t.as("usr-viewer").call("GET", `/projects/${secret.id}`)).status).toBe(404);
  });

  it("lets admins change settings and hides archived projects by default", async () => {
    const ws = await teamWithRoles(t);
    const project = await createProject(t.as("usr-member"), ws);
    expect((await t.as("usr-member").call("PATCH", `/projects/${project.id}`, { name: "x" })).status).toBe(403);
    const updated = await t.as("usr-admin").call("PATCH", `/projects/${project.id}`, {
      name: "Renamed",
      scheduleMode: "auto",
      startDate: "2026-10-01",
      archived: true,
    });
    expect(updated.status).toBe(200);
    expect(updated.body).toMatchObject({ name: "Renamed", scheduleMode: "auto", startDate: "2026-10-01", archived: true });
    const active = await t.as("usr-member").call("GET", `/workspaces/${ws}/projects`);
    expect(active.body.projects.some((p: { id: string }) => p.id === project.id)).toBe(false);
    const all = await t.as("usr-member").call("GET", `/workspaces/${ws}/projects?archived=true`);
    expect(all.body.projects.some((p: { id: string }) => p.id === project.id)).toBe(true);
  });

  it("moves a project between workspaces where the caller is admin in both", async () => {
    const from = await teamWithRoles(t, "From");
    const to = await teamWithRoles(t, "To");
    const project = await createProject(t.as("usr-owner"), from, { key: "MOVE" });

    // A member can see the project but may not move it.
    expect((await t.as("usr-member").call("POST", `/projects/${project.id}/move`, { workspaceId: to })).status).toBe(403);
    // A workspace the caller is not in does not exist, as far as they can tell.
    const elsewhere = (await t.as("usr-stranger").call("GET", "/workspaces")).body.workspaces[0].id;
    expect((await t.as("usr-owner").call("POST", `/projects/${project.id}/move`, { workspaceId: elsewhere })).status).toBe(404);
    // Admin in the source but only a member in the target is not enough.
    await t.as("usr-owner").call("PUT", `/workspaces/${to}/members/usr-admin`, { role: "member" });
    expect((await t.as("usr-admin").call("POST", `/projects/${project.id}/move`, { workspaceId: to })).status).toBe(403);

    await createProject(t.as("usr-owner"), to, { key: "MOVE" });
    const clash = await t.as("usr-owner").call("POST", `/projects/${project.id}/move`, { workspaceId: to });
    expect(clash.status).toBe(409);
    expect(clash.body.error).toBe("key_taken");

    const solo = await createProject(t.as("usr-owner"), from, { key: "SOLO" });
    const moved = await t.as("usr-owner").call("POST", `/projects/${solo.id}/move`, { workspaceId: to });
    expect(moved.status).toBe(200);
    expect(moved.body.workspaceId).toBe(to);
    expect((await t.as("usr-viewer").call("GET", `/workspaces/${to}/projects`)).body.projects.map((p: { key: string }) => p.key)).toContain("SOLO");
  });

  it("drops listed members who are not in the target workspace", async () => {
    const from = await teamWithRoles(t, "From2");
    const to = await teamWithRoles(t, "To2");
    await t.as("usr-owner").call("DELETE", `/workspaces/${to}/members/usr-member`);
    const project = await createProject(t.as("usr-owner"), from, { visibility: "restricted" });
    await t.as("usr-owner").call("PUT", `/projects/${project.id}/members/usr-member`);
    const moved = await t.as("usr-owner").call("POST", `/projects/${project.id}/move`, { workspaceId: to });
    expect(moved.status).toBe(200);
    const members = await t.as("usr-owner").call("GET", `/projects/${project.id}/members`);
    expect(members.body.members.map((m: { subject: string }) => m.subject)).toEqual(["usr-owner"]);
  });

  it("validates its input", async () => {
    const ws = await teamWithRoles(t);
    const post = (body: unknown) => t.as("usr-owner").call("POST", `/workspaces/${ws}/projects`, body);
    expect((await post({ key: "AB", name: "x" })).status).toBe(400);
    expect((await post({ key: "AB", name: "x", startDate: "2026-09-07", scheduleMode: "magic" })).status).toBe(400);
    expect((await post({ key: "AB", name: "x", startDate: "2026-09-07", owner: "me" })).status).toBe(400);
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/projects.test.ts`
Expected: FAIL — project routes return 404.

- [ ] **Step 3: Implement the store**

`src/store/projects.ts`:

```ts
import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { canSeeProject, projectAccess, requireRole, workspaceAccess, workspaceRole } from "../access";
import { conflict, notFound, unprocessable } from "../http";
import { now, transaction } from "./db";
import type { ProjectRow, Role, StatusCategory } from "./rows";

export interface Project {
  id: string;
  workspaceId: string;
  key: string;
  name: string;
  description: string;
  startDate: string;
  scheduleMode: "manual" | "auto";
  visibility: "workspace" | "restricted";
  archived: boolean;
  role: Role;
  createdAt: string;
  updatedAt: string;
}

export interface ProjectInput {
  key: string;
  name: string;
  description: string;
  startDate: string;
  scheduleMode: "manual" | "auto";
  visibility: "workspace" | "restricted";
}

export interface ProjectPatch {
  name?: string;
  description?: string;
  startDate?: string;
  scheduleMode?: "manual" | "auto";
  visibility?: "workspace" | "restricted";
  archived?: boolean;
}

export interface ProjectMember {
  subject: string;
  createdAt: string;
}

export const DEFAULT_STATUSES: readonly (readonly [string, StatusCategory])[] = [
  ["Backlog", "backlog"],
  ["Todo", "unstarted"],
  ["In Progress", "started"],
  ["Done", "completed"],
  ["Canceled", "canceled"],
];

export function toProject(row: ProjectRow, role: Role): Project {
  return {
    id: row.id,
    workspaceId: row.workspace_id,
    key: row.key,
    name: row.name,
    description: row.description,
    startDate: row.start_date,
    scheduleMode: row.schedule_mode,
    visibility: row.visibility,
    archived: row.archived === 1,
    role,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
  };
}

function projectRow(db: Database, id: string): ProjectRow {
  return db.query("SELECT * FROM projects WHERE id = ?").get(id) as ProjectRow;
}

function assertKeyFree(db: Database, workspaceId: string, key: string): void {
  if (db.query("SELECT 1 FROM projects WHERE workspace_id = ? AND key = ?").get(workspaceId, key)) {
    throw conflict("key_taken", `project key ${key} is already used in this workspace`);
  }
}

export function createProject(db: Database, subject: string, workspaceId: string, input: ProjectInput): Project {
  const role = workspaceAccess(db, workspaceId, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    assertKeyFree(db, workspaceId, input.key);
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO projects (id, workspace_id, key, name, description, start_date, schedule_mode, visibility,
         archived, next_number, working_weekdays, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, 1, 62, ?, ?, ?, ?)`,
    ).run(id, workspaceId, input.key, input.name, input.description, input.startDate, input.scheduleMode, input.visibility, at, at, subject, subject);
    const insertStatus = db.query(
      `INSERT INTO statuses (id, project_id, name, category, position, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    );
    DEFAULT_STATUSES.forEach(([name, category], position) => {
      insertStatus.run(randomUUID(), id, name, category, position, at, at, subject, subject);
    });
    // A member who creates a restricted project must still be able to see it.
    if (input.visibility === "restricted") {
      db.query("INSERT INTO project_members (project_id, subject, created_at, created_by) VALUES (?, ?, ?, ?)").run(id, subject, at, subject);
    }
    return toProject(projectRow(db, id), role);
  });
}

export function listProjects(db: Database, subject: string, workspaceId: string, includeArchived: boolean): Project[] {
  const role = workspaceAccess(db, workspaceId, subject);
  const rows = db
    .query(`SELECT * FROM projects WHERE workspace_id = ? AND (? = 1 OR archived = 0) ORDER BY name COLLATE NOCASE, id`)
    .all(workspaceId, includeArchived ? 1 : 0) as ProjectRow[];
  return rows.filter((row) => canSeeProject(db, row, subject, role)).map((row) => toProject(row, role));
}

export function getProject(db: Database, subject: string, id: string): Project {
  const { project, role } = projectAccess(db, id, subject);
  return toProject(project, role);
}

export function updateProject(db: Database, subject: string, id: string, patch: ProjectPatch): Project {
  const { project, role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  return transaction(db, () => {
    db.query(
      `UPDATE projects SET name = ?, description = ?, start_date = ?, schedule_mode = ?, visibility = ?, archived = ?,
         updated_at = ?, updated_by = ? WHERE id = ?`,
    ).run(
      patch.name ?? project.name,
      patch.description ?? project.description,
      patch.startDate ?? project.start_date,
      patch.scheduleMode ?? project.schedule_mode,
      patch.visibility ?? project.visibility,
      patch.archived === undefined ? project.archived : patch.archived ? 1 : 0,
      now(),
      subject,
      id,
    );
    return toProject(projectRow(db, id), role);
  });
}

export function moveProject(db: Database, subject: string, id: string, targetWorkspaceId: string): Project {
  const { project, role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  const targetRole = workspaceAccess(db, targetWorkspaceId, subject);
  requireRole(targetRole, "admin");
  if (targetWorkspaceId === project.workspace_id) return toProject(project, role);
  return transaction(db, () => {
    assertKeyFree(db, targetWorkspaceId, project.key);
    db.query("UPDATE projects SET workspace_id = ?, updated_at = ?, updated_by = ? WHERE id = ?").run(targetWorkspaceId, now(), subject, id);
    db.query(
      `DELETE FROM project_members WHERE project_id = ?
         AND subject NOT IN (SELECT subject FROM workspace_members WHERE workspace_id = ?)`,
    ).run(id, targetWorkspaceId);
    return toProject(projectRow(db, id), targetRole);
  });
}

export function listProjectMembers(db: Database, subject: string, id: string): ProjectMember[] {
  projectAccess(db, id, subject);
  const rows = db.query("SELECT subject, created_at FROM project_members WHERE project_id = ? ORDER BY subject").all(id) as {
    subject: string;
    created_at: string;
  }[];
  return rows.map((row) => ({ subject: row.subject, createdAt: row.created_at }));
}

export function addProjectMember(db: Database, subject: string, id: string, target: string): ProjectMember {
  const { project, role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  if (!workspaceRole(db, project.workspace_id, target)) {
    throw unprocessable("not_workspace_member", "only members of the project's workspace can be listed on it");
  }
  const at = now();
  db.query("INSERT OR IGNORE INTO project_members (project_id, subject, created_at, created_by) VALUES (?, ?, ?, ?)").run(id, target, at, subject);
  const row = db.query("SELECT subject, created_at FROM project_members WHERE project_id = ? AND subject = ?").get(id, target) as {
    subject: string;
    created_at: string;
  };
  return { subject: row.subject, createdAt: row.created_at };
}

export function removeProjectMember(db: Database, subject: string, id: string, target: string): void {
  const { role } = projectAccess(db, id, subject);
  requireRole(role, "admin");
  const removed = db.query("DELETE FROM project_members WHERE project_id = ? AND subject = ?").run(id, target);
  if (removed.changes === 0) throw notFound("project member");
}
```

- [ ] **Step 4: Implement the routes and register them**

`src/routes/projects.ts`:

```ts
import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as projects from "../store/projects";
import { boolValue, dateValue, enumValue, field, idValue, object, requiredText, subjectValue, text } from "../validation";

const KEY = /^[A-Z][A-Z0-9]{1,9}$/;
const MODES = ["manual", "auto"] as const;
const VISIBILITY = ["workspace", "restricted"] as const;

function projectKey(value: unknown): string {
  if (typeof value !== "string" || !KEY.test(value)) {
    throw badRequest("key must be 2-10 characters: an uppercase letter, then uppercase letters or digits");
  }
  return value;
}

export function registerProjectRoutes(router: Router<Context>): void {
  router.add("GET", "/workspaces/:id/projects", ({ db, subject, url }, params) =>
    json({
      projects: projects.listProjects(db, subject, param(params, "id"), url.searchParams.get("archived") === "true"),
    }),
  );

  router.add("POST", "/workspaces/:id/projects", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["key", "name", "description", "startDate", "scheduleMode", "visibility"]);
    const startDate = field(body, "startDate", dateValue("startDate"));
    if (startDate === undefined) throw badRequest("startDate is required");
    const input: projects.ProjectInput = {
      key: projectKey(body.key),
      name: requiredText(body, "name", 200),
      description: text(body, "description", 50_000) ?? "",
      startDate,
      scheduleMode: field(body, "scheduleMode", enumValue("scheduleMode", MODES)) ?? "manual",
      visibility: field(body, "visibility", enumValue("visibility", VISIBILITY)) ?? "workspace",
    };
    return json(projects.createProject(db, subject, param(params, "id"), input), 201);
  });

  router.add("GET", "/projects/:id", ({ db, subject }, params) => json(projects.getProject(db, subject, param(params, "id"))));

  router.add("PATCH", "/projects/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name", "description", "startDate", "scheduleMode", "visibility", "archived"]);
    const patch: projects.ProjectPatch = {};
    if (body.name !== undefined) patch.name = requiredText(body, "name", 200);
    const description = text(body, "description", 50_000);
    if (description !== undefined) patch.description = description;
    const startDate = field(body, "startDate", dateValue("startDate"));
    if (startDate !== undefined) patch.startDate = startDate;
    const scheduleMode = field(body, "scheduleMode", enumValue("scheduleMode", MODES));
    if (scheduleMode !== undefined) patch.scheduleMode = scheduleMode;
    const visibility = field(body, "visibility", enumValue("visibility", VISIBILITY));
    if (visibility !== undefined) patch.visibility = visibility;
    const archived = field(body, "archived", boolValue("archived"));
    if (archived !== undefined) patch.archived = archived;
    return json(projects.updateProject(db, subject, param(params, "id"), patch));
  });

  router.add("POST", "/projects/:id/move", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["workspaceId"]);
    const workspaceId = field(body, "workspaceId", idValue("workspaceId"));
    if (workspaceId === undefined) throw badRequest("workspaceId is required");
    return json(projects.moveProject(db, subject, param(params, "id"), workspaceId));
  });

  router.add("GET", "/projects/:id/members", ({ db, subject }, params) =>
    json({ members: projects.listProjectMembers(db, subject, param(params, "id")) }),
  );

  router.add("PUT", "/projects/:id/members/:subject", ({ db, subject }, params) =>
    json(projects.addProjectMember(db, subject, param(params, "id"), subjectValue("subject")(param(params, "subject")))),
  );

  router.add("DELETE", "/projects/:id/members/:subject", ({ db, subject }, params) => {
    projects.removeProjectMember(db, subject, param(params, "id"), subjectValue("subject")(param(params, "subject")));
    return json({ deleted: true });
  });
}
```

In `src/server.ts` add `import { registerProjectRoutes } from "./routes/projects";` and append `registerProjectRoutes` to `ROUTE_MODULES`.

- [ ] **Step 5: Run the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: `PASS`; `tests/projects.test.ts` 8 tests pass.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): projects with restricted visibility and workspace moves

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 10: Statuses

**Files:**
- Create: `apps/Nexus-Project/src/store/statuses.ts`, `src/routes/statuses.ts`
- Modify: `src/server.ts` (register); `tests/projects.test.ts` (make the status assertion unconditional)
- Test: `tests/statuses.test.ts`

**Interfaces:**
- Consumes: Tasks 7–9.
- Produces: `Status { id, projectId, name, category, position }`, `STATUS_CATEGORIES`, `listStatuses(db, subject, projectId)`, `createStatus(db, subject, projectId, name, category)`, `updateStatus(db, subject, statusId, patch: { name?, category?, position? })`, `deleteStatus(db, subject, statusId, moveTasksTo: string | null)`, `defaultStatusId(db, projectId): string` (first `unstarted` status by position, else the first status); `registerStatusRoutes(router)`.

Routes: `GET/POST /projects/:id/statuses`, `PATCH /statuses/:id`, `DELETE /statuses/:id?moveTasksTo=<statusId>`.

Rules: viewers read; admins change. Names are unique per project (`409 status_name_taken`). `position` moves a status to that index and renumbers the rest 0…n−1. A project keeps at least one status (`422 last_status`). Deleting a status that tasks use requires `moveTasksTo`, another status of the same project (`422 status_in_use` without it, `400` if invalid); moved tasks get a new version.

- [ ] **Step 1: Write the failing test**

In `tests/projects.test.ts`, replace:

```ts
    const statuses = await t.as("usr-viewer").call("GET", `/projects/${project.id}/statuses`);
    // Statuses routes arrive in Task 10; until then this is a 404 and the next line is skipped.
    if (statuses.status === 200) {
      expect(statuses.body.statuses.map((s: { category: string }) => s.category)).toEqual([
        "backlog",
        "unstarted",
        "started",
        "completed",
        "canceled",
      ]);
    }
```

with:

```ts
    const statuses = await t.as("usr-viewer").call("GET", `/projects/${project.id}/statuses`);
    expect(statuses.status).toBe(200);
    expect(statuses.body.statuses.map((s: { category: string }) => s.category)).toEqual([
      "backlog",
      "unstarted",
      "started",
      "completed",
      "canceled",
    ]);
```

`tests/statuses.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

async function statuses(projectId: string) {
  return (await t.as("usr-viewer").call("GET", `/projects/${projectId}/statuses`)).body.statuses as {
    id: string;
    name: string;
    position: number;
  }[];
}

describe("statuses", () => {
  it("are ordered by position and read by viewers", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const list = await statuses(project.id);
    expect(list.map((s) => s.name)).toEqual(["Backlog", "Todo", "In Progress", "Done", "Canceled"]);
    expect(list.map((s) => s.position)).toEqual([0, 1, 2, 3, 4]);
  });

  it("are managed by admins only", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const body = { name: "Review", category: "started" };
    expect((await t.as("usr-member").call("POST", `/projects/${project.id}/statuses`, body)).status).toBe(403);
    const created = await t.as("usr-admin").call("POST", `/projects/${project.id}/statuses`, body);
    expect(created.status).toBe(201);
    expect(created.body).toMatchObject({ name: "Review", category: "started", position: 5 });
    const dup = await t.as("usr-admin").call("POST", `/projects/${project.id}/statuses`, body);
    expect(dup.status).toBe(409);
    expect(dup.body.error).toBe("status_name_taken");
  });

  it("reorders by moving one status and renumbering the rest", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const done = (await statuses(project.id)).find((s) => s.name === "Done") as { id: string };
    const moved = await t.as("usr-admin").call("PATCH", `/statuses/${done.id}`, { position: 0, name: "Shipped" });
    expect(moved.status).toBe(200);
    const list = await statuses(project.id);
    expect(list.map((s) => s.name)).toEqual(["Shipped", "Backlog", "Todo", "In Progress", "Canceled"]);
    expect(list.map((s) => s.position)).toEqual([0, 1, 2, 3, 4]);
  });

  it("keeps at least one status", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    const list = await statuses(project.id);
    for (const status of list.slice(1)) {
      expect((await t.as("usr-admin").call("DELETE", `/statuses/${status.id}`)).status).toBe(200);
    }
    const last = await t.as("usr-admin").call("DELETE", `/statuses/${(list[0] as { id: string }).id}`);
    expect(last.status).toBe(422);
    expect(last.body.error).toBe("last_status");
    expect((await statuses(project.id)).map((s) => s.position)).toEqual([0]);
  });

  it("refuses a moveTasksTo from another project", async () => {
    const a = await createProject(t.as("usr-owner"), ws);
    const b = await createProject(t.as("usr-owner"), ws);
    const target = (await statuses(b.id))[0] as { id: string };
    const source = (await statuses(a.id))[0] as { id: string };
    const res = await t.as("usr-admin").call("DELETE", `/statuses/${source.id}?moveTasksTo=${target.id}`);
    expect(res.status).toBe(400);
  });

  it("validates its input and hides other workspaces' statuses", async () => {
    const project = await createProject(t.as("usr-owner"), ws);
    expect((await t.as("usr-admin").call("POST", `/projects/${project.id}/statuses`, { name: "X", category: "doing" })).status).toBe(400);
    const id = ((await statuses(project.id))[0] as { id: string }).id;
    expect((await t.as("usr-stranger").call("PATCH", `/statuses/${id}`, { name: "Mine" })).status).toBe(404);
    expect((await t.as("usr-admin").call("PATCH", `/statuses/${id}`, { position: 99 })).status).toBe(400);
  });
});
```

(Deleting a status that tasks use is tested in Task 12, once tasks exist.)

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/statuses.test.ts tests/projects.test.ts`
Expected: FAIL — status routes return 404.

- [ ] **Step 3: Implement the store**

`src/store/statuses.ts`:

```ts
import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { type Role, projectAccess, requireRole } from "../access";
import { badRequest, conflict, notFound, unprocessable } from "../http";
import { now, transaction } from "./db";
import type { ProjectRow, StatusCategory, StatusRow } from "./rows";

export const STATUS_CATEGORIES: readonly StatusCategory[] = ["backlog", "unstarted", "started", "completed", "canceled"];

export interface Status {
  id: string;
  projectId: string;
  name: string;
  category: StatusCategory;
  position: number;
}

export interface StatusPatch {
  name?: string;
  category?: StatusCategory;
  position?: number;
}

function toStatus(row: StatusRow): Status {
  return { id: row.id, projectId: row.project_id, name: row.name, category: row.category, position: row.position };
}

function rows(db: Database, projectId: string): StatusRow[] {
  return db.query("SELECT * FROM statuses WHERE project_id = ? ORDER BY position, id").all(projectId) as StatusRow[];
}

function statusAccess(db: Database, statusId: string, subject: string): { status: StatusRow; project: ProjectRow; role: Role } {
  const status = db.query("SELECT * FROM statuses WHERE id = ?").get(statusId) as StatusRow | null;
  if (!status) throw notFound("status");
  try {
    return { status, ...projectAccess(db, status.project_id, subject) };
  } catch {
    throw notFound("status");
  }
}

function renumber(db: Database, ordered: readonly string[]): void {
  const update = db.query("UPDATE statuses SET position = ? WHERE id = ?");
  ordered.forEach((id, position) => update.run(position, id));
}

function assertNameFree(db: Database, projectId: string, name: string, except: string | null): void {
  const clash = db.query("SELECT id FROM statuses WHERE project_id = ? AND name = ?").get(projectId, name) as { id: string } | null;
  if (clash && clash.id !== except) throw conflict("status_name_taken", `a status named ${name} already exists`);
}

export function listStatuses(db: Database, subject: string, projectId: string): Status[] {
  projectAccess(db, projectId, subject);
  return rows(db, projectId).map(toStatus);
}

export function defaultStatusId(db: Database, projectId: string): string {
  const all = rows(db, projectId);
  const chosen = all.find((s) => s.category === "unstarted") ?? all[0];
  if (!chosen) throw new Error(`project ${projectId} has no statuses`);
  return chosen.id;
}

export function createStatus(db: Database, subject: string, projectId: string, name: string, category: StatusCategory): Status {
  const { role } = projectAccess(db, projectId, subject);
  requireRole(role, "admin");
  return transaction(db, () => {
    assertNameFree(db, projectId, name, null);
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO statuses (id, project_id, name, category, position, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    ).run(id, projectId, name, category, rows(db, projectId).length, at, at, subject, subject);
    return toStatus(db.query("SELECT * FROM statuses WHERE id = ?").get(id) as StatusRow);
  });
}

export function updateStatus(db: Database, subject: string, statusId: string, patch: StatusPatch): Status {
  const { status, role } = statusAccess(db, statusId, subject);
  requireRole(role, "admin");
  return transaction(db, () => {
    if (patch.name !== undefined) assertNameFree(db, status.project_id, patch.name, status.id);
    db.query("UPDATE statuses SET name = ?, category = ?, updated_at = ?, updated_by = ? WHERE id = ?").run(
      patch.name ?? status.name,
      patch.category ?? status.category,
      now(),
      subject,
      status.id,
    );
    if (patch.position !== undefined) {
      const others = rows(db, status.project_id).map((s) => s.id).filter((id) => id !== status.id);
      if (patch.position > others.length) throw badRequest(`position must be from 0 to ${others.length}`);
      others.splice(patch.position, 0, status.id);
      renumber(db, others);
    }
    return toStatus(db.query("SELECT * FROM statuses WHERE id = ?").get(status.id) as StatusRow);
  });
}

export function deleteStatus(db: Database, subject: string, statusId: string, moveTasksTo: string | null): void {
  const { status, role } = statusAccess(db, statusId, subject);
  requireRole(role, "admin");
  transaction(db, () => {
    const all = rows(db, status.project_id);
    if (all.length === 1) throw unprocessable("last_status", "a project needs at least one status");
    if (moveTasksTo !== null && (moveTasksTo === status.id || !all.some((s) => s.id === moveTasksTo))) {
      throw badRequest("moveTasksTo must be another status of the same project");
    }
    const inUse = (db.query("SELECT COUNT(*) AS n FROM tasks WHERE status_id = ?").get(status.id) as { n: number }).n;
    if (inUse > 0) {
      if (moveTasksTo === null) throw unprocessable("status_in_use", `${inUse} task(s) use this status; pass moveTasksTo`);
      db.query("UPDATE tasks SET status_id = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE status_id = ?").run(
        moveTasksTo,
        now(),
        subject,
        status.id,
      );
    }
    db.query("DELETE FROM statuses WHERE id = ?").run(status.id);
    renumber(db, all.map((s) => s.id).filter((id) => id !== status.id));
  });
}
```

- [ ] **Step 4: Implement the routes and register them**

`src/routes/statuses.ts`:

```ts
import { json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as statuses from "../store/statuses";
import { enumValue, field, idValue, intValue, object, requiredText } from "../validation";

export function registerStatusRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/statuses", ({ db, subject }, params) =>
    json({ statuses: statuses.listStatuses(db, subject, param(params, "id")) }),
  );

  router.add("POST", "/projects/:id/statuses", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name", "category"]);
    const name = requiredText(body, "name", 200);
    const category = enumValue("category", statuses.STATUS_CATEGORIES)(body.category);
    return json(statuses.createStatus(db, subject, param(params, "id"), name, category), 201);
  });

  router.add("PATCH", "/statuses/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["name", "category", "position"]);
    const patch: statuses.StatusPatch = {};
    if (body.name !== undefined) patch.name = requiredText(body, "name", 200);
    const category = field(body, "category", enumValue("category", statuses.STATUS_CATEGORIES));
    if (category !== undefined) patch.category = category;
    const position = field(body, "position", intValue("position", 0, 1000));
    if (position !== undefined) patch.position = position;
    return json(statuses.updateStatus(db, subject, param(params, "id"), patch));
  });

  router.add("DELETE", "/statuses/:id", ({ db, subject, url }, params) => {
    const target = url.searchParams.get("moveTasksTo");
    statuses.deleteStatus(db, subject, param(params, "id"), target === null ? null : idValue("moveTasksTo")(target));
    return json({ deleted: true });
  });
}
```

In `src/server.ts` add `import { registerStatusRoutes } from "./routes/statuses";` and append `registerStatusRoutes` to `ROUTE_MODULES`.

- [ ] **Step 5: Run the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: `PASS`; `tests/statuses.test.ts` 6 tests and the tightened project test pass.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): per-project statuses mapped to fixed categories

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---
### Task 11: The bridge between the store and the engine

**Files:**
- Create: `apps/Nexus-Project/src/store/scheduling.ts`, `src/store/queries.ts`
- Modify: `src/store/projects.ts` (`updateProject` reschedules), `src/store/statuses.ts` (category changes and task moves reschedule)
- Test: `tests/scheduling.test.ts`

**Interfaces:**
- Consumes: `schedule` (Task 5), `WorkingCalendar`, `toDay`, `fromDay` (Task 3), row types (Task 7), store functions (Tasks 8–10).
- Produces (`src/store/scheduling.ts`): `weekdaysFromMask(mask): number[]`, `maskFromWeekdays(days): number`, `loadCalendar(db, project): CalendarSpec`, `projectCalendar(db, project): WorkingCalendar`, `finishFor(calendar, start, durationDays): string`, `durationBetween(calendar, start, finish): number` (inclusive working days), `firstWorkingDay(calendar, date): string`, `loadScheduleInput(db, project): ScheduleInput`, `computeSchedule(db, project): ScheduleResult`, `reschedule(db, projectId, subject): void`, `refreshDerivedFinishes(db, project, subject): void`.
- Produces (`src/store/queries.ts`): `hasChildren(db, taskId)`, `hasLinks(db, taskId)`, `statusInProject(db, projectId, statusId)`, `taskKey(projectKey, number)`.

`reschedule` runs inside the caller's transaction. In auto mode it writes each scheduled leaf's `start_date`/`finish_date` from the engine and bumps `version` only on rows whose dates changed. In manual mode it does nothing: stored dates are the user's, and violations are reported on read.

- [ ] **Step 1: Write the failing test**

`tests/scheduling.test.ts`:

```ts
import type { Database } from "bun:sqlite";
import { beforeEach, describe, expect, it } from "bun:test";
import { randomUUID } from "node:crypto";
import { WorkingCalendar } from "../src/schedule/calendar";
import { openDatabase } from "../src/store/db";
import { createProject, updateProject } from "../src/store/projects";
import type { ProjectRow } from "../src/store/rows";
import {
  durationBetween,
  finishFor,
  firstWorkingDay,
  loadScheduleInput,
  maskFromWeekdays,
  reschedule,
  weekdaysFromMask,
} from "../src/store/scheduling";
import { updateStatus } from "../src/store/statuses";
import { createTeamWorkspace } from "../src/store/workspaces";

const OWNER = "usr-owner";
let db: Database;
let project: ProjectRow;

function statusId(category: string): string {
  return (db.query("SELECT id FROM statuses WHERE project_id = ? AND category = ?").get(project.id, category) as { id: string }).id;
}

function insertTask(number: number, fields: { duration?: number | null; start?: string | null; parent?: string | null; category?: string }): string {
  const id = randomUUID();
  db.query(
    `INSERT INTO tasks (id, project_id, number, parent_id, rank, title, status_id, duration_days, start_date,
       created_at, updated_at, created_by, updated_by)
     VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 'x', 'x', ?, ?)`,
  ).run(id, project.id, number, fields.parent ?? null, `r${number}`, `Task ${number}`, statusId(fields.category ?? "unstarted"),
    fields.duration === undefined ? 1 : fields.duration, fields.start ?? null, OWNER, OWNER);
  return id;
}

function link(from: string, to: string): void {
  db.query(
    `INSERT INTO dependencies (id, project_id, predecessor_id, successor_id, type, lag_days, created_at, updated_at, created_by, updated_by)
     VALUES (?, ?, ?, ?, 'FS', 0, 'x', 'x', ?, ?)`,
  ).run(randomUUID(), project.id, from, to, OWNER, OWNER);
}

function dates(id: string) {
  return db.query("SELECT start_date, finish_date, version FROM tasks WHERE id = ?").get(id) as {
    start_date: string | null;
    finish_date: string | null;
    version: number;
  };
}

beforeEach(() => {
  db = openDatabase(":memory:");
  const ws = createTeamWorkspace(db, OWNER, "Team");
  const created = createProject(db, OWNER, ws.id, {
    key: "WEB",
    name: "Web",
    description: "",
    startDate: "2026-09-07",
    scheduleMode: "manual",
    visibility: "workspace",
  });
  project = db.query("SELECT * FROM projects WHERE id = ?").get(created.id) as ProjectRow;
});

describe("calendar helpers", () => {
  const cal = new WorkingCalendar({ workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] });

  it("round-trips weekday masks", () => {
    expect(weekdaysFromMask(62)).toEqual([1, 2, 3, 4, 5]);
    expect(maskFromWeekdays([1, 2, 3, 4, 5])).toBe(62);
  });

  it("derives finish dates and inclusive durations in working days", () => {
    expect(finishFor(cal, "2026-09-07", 3)).toBe("2026-09-09");
    expect(finishFor(cal, "2026-09-11", 2)).toBe("2026-09-14");
    expect(finishFor(cal, "2026-09-07", 0)).toBe("2026-09-07");
    expect(durationBetween(cal, "2026-09-07", "2026-09-14")).toBe(6);
    expect(durationBetween(cal, "2026-09-09", "2026-09-07")).toBeLessThan(1);
    expect(firstWorkingDay(cal, "2026-09-05")).toBe("2026-09-07");
  });
});

describe("store to engine", () => {
  it("maps rows to engine input, with status categories as states", () => {
    const parent = insertTask(1, { duration: null });
    insertTask(2, { parent, category: "completed", start: "2026-09-07" });
    insertTask(3, { category: "canceled" });
    const input = loadScheduleInput(db, project);
    expect(input).toMatchObject({ projectStart: "2026-09-07", mode: "manual", calendar: { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] } });
    expect(input.tasks.map((t) => [t.key, t.state, t.parentId === parent])).toEqual([
      ["WEB-1", "open", false],
      ["WEB-2", "completed", true],
      ["WEB-3", "canceled", false],
    ]);
  });

  it("leaves manual dates alone", () => {
    const a = insertTask(1, { duration: 3, start: "2026-09-14" });
    reschedule(db, project.id, OWNER);
    expect(dates(a)).toEqual({ start_date: "2026-09-14", finish_date: null, version: 1 });
  });

  it("writes auto dates, and only bumps versions that changed", () => {
    db.query("UPDATE projects SET schedule_mode = 'auto' WHERE id = ?").run(project.id);
    const a = insertTask(1, { duration: 3 });
    const b = insertTask(2, { duration: 2 });
    link(a, b);
    reschedule(db, project.id, OWNER);
    expect(dates(a)).toEqual({ start_date: "2026-09-07", finish_date: "2026-09-09", version: 2 });
    expect(dates(b)).toEqual({ start_date: "2026-09-10", finish_date: "2026-09-11", version: 2 });
    reschedule(db, project.id, OWNER);
    expect(dates(a).version).toBe(2);
    expect(dates(b).version).toBe(2);
  });

  it("reschedules when a project switches to auto or moves its start", () => {
    const a = insertTask(1, { duration: 2 });
    updateProject(db, OWNER, project.id, { scheduleMode: "auto" });
    expect(dates(a).start_date).toBe("2026-09-07");
    updateProject(db, OWNER, project.id, { startDate: "2026-09-14" });
    expect(dates(a)).toMatchObject({ start_date: "2026-09-14", finish_date: "2026-09-15" });
  });

  it("reschedules when a status becomes completed, pinning its tasks", () => {
    db.query("UPDATE projects SET schedule_mode = 'auto' WHERE id = ?").run(project.id);
    const a = insertTask(1, { duration: 3 });
    const b = insertTask(2, { duration: 1, category: "started" });
    link(a, b);
    reschedule(db, project.id, OWNER);
    expect(dates(b).start_date).toBe("2026-09-10");
    // A grows, which would push B, but B's status is about to become "completed".
    db.query("UPDATE tasks SET duration_days = 5 WHERE id = ?").run(a);
    updateStatus(db, OWNER, statusId("started"), { category: "completed" });
    // The category change rescheduled (A's finish moved) and pinned B where it was.
    expect(dates(a).finish_date).toBe("2026-09-11");
    expect(dates(b).start_date).toBe("2026-09-10");
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/scheduling.test.ts`
Expected: FAIL — `Cannot find module '../src/store/scheduling'`.

- [ ] **Step 3: Implement**

`src/store/queries.ts`:

```ts
import type { Database } from "bun:sqlite";

export function hasChildren(db: Database, taskId: string): boolean {
  return Boolean(db.query("SELECT 1 FROM tasks WHERE parent_id = ? LIMIT 1").get(taskId));
}

export function hasLinks(db: Database, taskId: string): boolean {
  return Boolean(
    db.query("SELECT 1 FROM dependencies WHERE predecessor_id = ? OR successor_id = ? LIMIT 1").get(taskId, taskId),
  );
}

export function statusInProject(db: Database, projectId: string, statusId: string): boolean {
  return Boolean(db.query("SELECT 1 FROM statuses WHERE id = ? AND project_id = ?").get(statusId, projectId));
}

export function taskKey(projectKey: string, number: number): string {
  return `${projectKey}-${number}`;
}
```

`src/store/scheduling.ts`:

```ts
import type { Database } from "bun:sqlite";
import { WorkingCalendar, fromDay, toDay } from "../schedule/calendar";
import { schedule } from "../schedule/engine";
import type { CalendarSpec, ScheduleInput, ScheduleResult, TaskState } from "../schedule/types";
import { now } from "./db";
import { taskKey } from "./queries";
import type { DependencyRow, ProjectRow, StatusCategory, TaskRow } from "./rows";

const STATE: Record<StatusCategory, TaskState> = {
  backlog: "open",
  unstarted: "open",
  started: "open",
  completed: "completed",
  canceled: "canceled",
};

export function weekdaysFromMask(mask: number): number[] {
  return [0, 1, 2, 3, 4, 5, 6].filter((day) => (mask >> day) & 1);
}

export function maskFromWeekdays(days: readonly number[]): number {
  return days.reduce((mask, day) => mask | (1 << day), 0);
}

export function loadCalendar(db: Database, project: ProjectRow): CalendarSpec {
  const rows = db.query("SELECT date, working FROM calendar_exceptions WHERE project_id = ? ORDER BY date").all(project.id) as {
    date: string;
    working: number;
  }[];
  return {
    workingWeekdays: weekdaysFromMask(project.working_weekdays),
    exceptions: rows.map((row) => ({ date: row.date, working: row.working === 1 })),
  };
}

export function projectCalendar(db: Database, project: ProjectRow): WorkingCalendar {
  return new WorkingCalendar(loadCalendar(db, project));
}

/** The last working day of a task starting on `start` and lasting `durationDays`. */
export function finishFor(calendar: WorkingCalendar, start: string, durationDays: number): string {
  const first = calendar.indexOf(toDay(start));
  return fromDay(calendar.dayAt(durationDays > 0 ? first + durationDays - 1 : first));
}

/** Working days from `start` through `finish`, both inclusive. Below 1 means finish precedes start. */
export function durationBetween(calendar: WorkingCalendar, start: string, finish: string): number {
  return calendar.indexOf(toDay(finish) + 1) - calendar.indexOf(toDay(start));
}

export function firstWorkingDay(calendar: WorkingCalendar, date: string): string {
  return fromDay(calendar.dayAt(calendar.indexOf(toDay(date))));
}

export function loadScheduleInput(db: Database, project: ProjectRow): ScheduleInput {
  const tasks = db
    .query("SELECT t.*, s.category FROM tasks t JOIN statuses s ON s.id = t.status_id WHERE t.project_id = ? ORDER BY t.number")
    .all(project.id) as (TaskRow & { category: StatusCategory })[];
  const links = db
    .query("SELECT * FROM dependencies WHERE project_id = ? ORDER BY created_at, id")
    .all(project.id) as DependencyRow[];
  return {
    projectStart: project.start_date,
    mode: project.schedule_mode,
    calendar: loadCalendar(db, project),
    tasks: tasks.map((row) => ({
      id: row.id,
      key: taskKey(project.key, row.number),
      number: row.number,
      parentId: row.parent_id,
      kind: row.kind,
      state: STATE[row.category],
      durationDays: row.duration_days,
      startDate: row.start_date,
      constraintType: row.constraint_type,
      constraintDate: row.constraint_date,
      deadline: row.deadline,
      progress: row.progress,
    })),
    links: links.map((row) => ({
      id: row.id,
      predecessorId: row.predecessor_id,
      successorId: row.successor_id,
      type: row.type,
      lagDays: row.lag_days,
    })),
  };
}

export function computeSchedule(db: Database, project: ProjectRow): ScheduleResult {
  return schedule(loadScheduleInput(db, project));
}

/**
 * Re-derives dates after a schedule-affecting write, inside the caller's
 * transaction. Auto mode: the engine owns leaf dates and writes them back.
 * Manual mode: nothing moves; violations are reported when the schedule is read.
 */
export function reschedule(db: Database, projectId: string, subject: string): void {
  const project = db.query("SELECT * FROM projects WHERE id = ?").get(projectId) as ProjectRow | null;
  if (!project || project.schedule_mode !== "auto") return;
  const result = computeSchedule(db, project);
  const current = new Map(
    (db.query("SELECT id, start_date, finish_date FROM tasks WHERE project_id = ?").all(projectId) as {
      id: string;
      start_date: string | null;
      finish_date: string | null;
    }[]).map((row) => [row.id, row]),
  );
  const update = db.query(
    "UPDATE tasks SET start_date = ?, finish_date = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE id = ?",
  );
  const at = now();
  for (const task of result.tasks) {
    const row = current.get(task.id);
    if (!row || (row.start_date === task.earlyStart && row.finish_date === task.earlyFinish)) continue;
    update.run(task.earlyStart, task.earlyFinish, at, subject, task.id);
  }
}

/** After a calendar change a stored finish must follow the task's working-day duration. */
export function refreshDerivedFinishes(db: Database, project: ProjectRow, subject: string): void {
  const calendar = projectCalendar(db, project);
  const rows = db
    .query("SELECT id, start_date, finish_date, duration_days FROM tasks WHERE project_id = ? AND start_date IS NOT NULL AND duration_days IS NOT NULL")
    .all(project.id) as { id: string; start_date: string; finish_date: string | null; duration_days: number }[];
  const update = db.query(
    "UPDATE tasks SET start_date = ?, finish_date = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE id = ?",
  );
  const at = now();
  for (const row of rows) {
    const start = firstWorkingDay(calendar, row.start_date);
    const finish = finishFor(calendar, start, row.duration_days);
    if (start !== row.start_date || finish !== row.finish_date) update.run(start, finish, at, subject, row.id);
  }
}
```

- [ ] **Step 4: Reschedule from project and status writes**

In `src/store/projects.ts`, add `import { reschedule } from "./scheduling";` and in `updateProject`, immediately before `return toProject(projectRow(db, id), role);` inside the transaction, add:

```ts
    if (patch.startDate !== undefined || patch.scheduleMode !== undefined) reschedule(db, id, subject);
```

In `src/store/statuses.ts`, add `import { reschedule } from "./scheduling";`. In `updateStatus`, immediately before the final `return` inside the transaction, add:

```ts
    // A category decides whether tasks are open, pinned as completed or left out.
    if (patch.category !== undefined && patch.category !== status.category) reschedule(db, status.project_id, subject);
```

In `deleteStatus`, immediately after the `UPDATE tasks SET status_id = ?…` statement inside `if (inUse > 0)`, add:

```ts
      reschedule(db, status.project_id, subject);
```

- [ ] **Step 5: Run the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: `PASS`; `tests/scheduling.test.ts` 7 tests pass.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): connect the store to the scheduling engine

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 12: Tasks and the work breakdown structure

**Files:**
- Create: `apps/Nexus-Project/src/store/tasks.ts`, `src/routes/tasks.ts`
- Modify: `src/server.ts` (register)
- Test: `tests/tasks.test.ts`

**Interfaces:**
- Consumes: Tasks 2, 7–11.
- Produces: `Task` (camelCase of every `tasks` column, plus `key`), `PRIORITIES`, `TaskFields`, `TaskCreate`, `TaskMove`, `TaskFilters`, `toTask(row, projectKey)`, `listTasks`, `getTask`, `createTask`, `updateTask(db, subject, id, expectedVersion, fields)`, `moveTask(db, subject, id, expectedVersion, move)`, `deleteTask`; `registerTaskRoutes(router)`; `expectedVersion(req)` exported from `src/routes/tasks.ts`.

Routes: `GET/POST /projects/:id/tasks` (query `status=<id>`, `assignee=<subject>|none`, `parent=<id>|root`), `GET/PATCH/DELETE /tasks/:id`, `POST /tasks/:id/move`.

Rules:
- Members and above write; viewers read.
- `number` comes from the project's counter; `key` is `KEY-number`. A new task goes last in rank order, in the project's first `unstarted` status unless `statusId` is given.
- `statusId` must belong to the project (400). `assigneeSubject` must be a member of the project's workspace (`422 assignee_not_member`); `null` clears it.
- Dates: a start on a non-working day moves to the next working day. `finishDate` sets the duration (inclusive working days); a finish before the start is `422 invalid_dates`; a finish without a start is 400. `finish_date` is always derived from start + duration. A task has at least one working day (`422 zero_duration`); a milestone has none (`422 milestone_duration`). Switching a milestone back to a task clears its duration unless one is given.
- `constraintType: start_no_earlier_than` requires `constraintDate` (400); `asap` clears it. In auto mode a requested `startDate` becomes a start-no-earlier-than constraint, since the engine owns start dates.
- A task with children is a summary: changing `kind`, `durationDays`, `startDate`, `finishDate`, `constraintType`, `constraintDate` or `progress` is `422 summary_fields`.
- A parent must be a task of the same project (`422 invalid_parent`), not a milestone (`422 milestone_children`), not linked (`422 summary_link`), and not the task itself or one of its descendants (`422 wbs_cycle`).
- `PATCH` and `move` require `If-Match: <version>`: missing → `428 version_required`, stale → `409 version_conflict` with the current `task` in the body. Every write bumps `version`.
- `move` takes `parentId` (null = top level), `statusId`, and `afterId` (null = first; an id = directly after that task).
- `DELETE` removes the task, its whole subtree and every link touching them.
- Every write reschedules the project.

- [ ] **Step 1: Write the failing test**

`tests/tasks.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { type Client, startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

const member = () => t.as("usr-member");

async function newTask(client: Client, projectId: string, body: Record<string, unknown> = {}) {
  const res = await client.call("POST", `/projects/${projectId}/tasks`, { title: "Task", ...body });
  expect(res.status).toBe(201);
  return res.body;
}

async function patch(id: string, version: number, body: Record<string, unknown>) {
  return member().call("PATCH", `/tasks/${id}`, body, { "if-match": String(version) });
}

async function titles(projectId: string, query = "") {
  const res = await member().call("GET", `/projects/${projectId}/tasks${query}`);
  return res.body.tasks.map((task: { title: string }) => task.title);
}

describe("creating tasks", () => {
  it("numbers them per project and files them under Todo", async () => {
    const project = await createProject(member(), ws, { key: "APP" });
    const first = await newTask(member(), project.id, { title: "First" });
    const second = await newTask(member(), project.id, { title: "Second" });
    const statuses = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses;
    expect(first).toMatchObject({ key: "APP-1", number: 1, version: 1, priority: "none", kind: "task", createdBy: "usr-member" });
    expect(second.key).toBe("APP-2");
    expect(first.statusId).toBe(statuses.find((s: { name: string }) => s.name === "Todo").id);
    expect(second.rank > first.rank).toBe(true);
  });

  it("is refused to viewers and invisible to strangers", async () => {
    const project = await createProject(member(), ws);
    expect((await t.as("usr-viewer").call("POST", `/projects/${project.id}/tasks`, { title: "x" })).status).toBe(403);
    expect((await t.as("usr-stranger").call("GET", `/projects/${project.id}/tasks`)).status).toBe(404);
  });

  it("validates its input", async () => {
    const project = await createProject(member(), ws);
    const post = (body: unknown) => member().call("POST", `/projects/${project.id}/tasks`, body);
    expect((await post({})).status).toBe(400);
    expect((await post({ title: "x".repeat(513) })).status).toBe(400);
    expect((await post({ title: "x", color: "red" })).status).toBe(400);
    expect((await post({ title: "x", progress: 101 })).status).toBe(400);
    const other = await createProject(member(), ws);
    const foreignStatus = (await member().call("GET", `/projects/${other.id}/statuses`)).body.statuses[0].id;
    expect((await post({ title: "x", statusId: foreignStatus })).status).toBe(400);
    const outsider = await post({ title: "x", assigneeSubject: "usr-stranger" });
    expect(outsider.status).toBe(422);
    expect(outsider.body.error).toBe("assignee_not_member");
  });
});

describe("dates", () => {
  it("moves a weekend start to Monday and derives the finish", async () => {
    const project = await createProject(member(), ws);
    const task = await newTask(member(), project.id, { startDate: "2026-09-05", durationDays: 3 });
    expect(task).toMatchObject({ startDate: "2026-09-07", durationDays: 3, finishDate: "2026-09-09" });
  });

  it("turns a finish date into a working-day duration", async () => {
    const project = await createProject(member(), ws);
    const task = await newTask(member(), project.id, { startDate: "2026-09-07", finishDate: "2026-09-14" });
    expect(task).toMatchObject({ durationDays: 6, finishDate: "2026-09-14" });
    const backwards = await member().call("POST", `/projects/${project.id}/tasks`, {
      title: "x",
      startDate: "2026-09-09",
      finishDate: "2026-09-07",
    });
    expect(backwards.body.error).toBe("invalid_dates");
    expect((await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", finishDate: "2026-09-07" })).status).toBe(400);
  });

  it("keeps tasks at least a day long and milestones at none", async () => {
    const project = await createProject(member(), ws);
    const zero = await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", durationDays: 0 });
    expect(zero.body.error).toBe("zero_duration");
    const long = await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", kind: "milestone", durationDays: 2 });
    expect(long.body.error).toBe("milestone_duration");
    const milestone = await newTask(member(), project.id, { kind: "milestone", startDate: "2026-09-08" });
    expect(milestone).toMatchObject({ durationDays: 0, startDate: "2026-09-08", finishDate: "2026-09-08" });
    const back = await patch(milestone.id, milestone.version, { kind: "task" });
    expect(back.body).toMatchObject({ kind: "task", durationDays: null, finishDate: null });
  });

  it("requires a date for start-no-earlier-than and clears it for asap", async () => {
    const project = await createProject(member(), ws);
    expect(
      (await member().call("POST", `/projects/${project.id}/tasks`, { title: "x", constraintType: "start_no_earlier_than" })).status,
    ).toBe(400);
    const task = await newTask(member(), project.id, { constraintType: "start_no_earlier_than", constraintDate: "2026-09-10" });
    const cleared = await patch(task.id, task.version, { constraintType: "asap" });
    expect(cleared.body).toMatchObject({ constraintType: "asap", constraintDate: null });
  });

  it("turns a requested start into a constraint in auto mode", async () => {
    const project = await createProject(t.as("usr-admin"), ws, { scheduleMode: "auto" });
    const task = await newTask(member(), project.id, { startDate: "2026-09-09", durationDays: 2 });
    expect(task).toMatchObject({
      constraintType: "start_no_earlier_than",
      constraintDate: "2026-09-09",
      startDate: "2026-09-09",
      finishDate: "2026-09-10",
    });
  });
});

describe("work breakdown structure", () => {
  it("turns a parent into a summary whose schedule fields are derived", async () => {
    const project = await createProject(member(), ws);
    const phase = await newTask(member(), project.id, { title: "Phase", durationDays: 5 });
    await newTask(member(), project.id, { title: "Child", parentId: phase.id });
    const res = await patch(phase.id, phase.version, { durationDays: 2 });
    expect(res.status).toBe(422);
    expect(res.body.error).toBe("summary_fields");
    expect((await patch(phase.id, phase.version, { title: "Phase 1" })).status).toBe(200);
  });

  it("refuses parents from elsewhere, milestones, and cycles", async () => {
    const project = await createProject(member(), ws);
    const other = await createProject(member(), ws);
    const foreign = await newTask(member(), other.id);
    const milestone = await newTask(member(), project.id, { kind: "milestone" });
    const post = (body: Record<string, unknown>) => member().call("POST", `/projects/${project.id}/tasks`, { title: "x", ...body });
    expect((await post({ parentId: foreign.id })).body.error).toBe("invalid_parent");
    expect((await post({ parentId: milestone.id })).body.error).toBe("milestone_children");

    const parent = await newTask(member(), project.id, { title: "Parent" });
    const child = await newTask(member(), project.id, { title: "Child", parentId: parent.id });
    const grandchild = await newTask(member(), project.id, { title: "Grandchild", parentId: child.id });
    const current = (await member().call("GET", `/tasks/${parent.id}`)).body;
    const cycle = await member().call("POST", `/tasks/${parent.id}/move`, { parentId: grandchild.id }, { "if-match": String(current.version) });
    expect(cycle.body.error).toBe("wbs_cycle");
  });

  it("deletes a whole subtree", async () => {
    const project = await createProject(member(), ws);
    const parent = await newTask(member(), project.id, { title: "Parent" });
    const child = await newTask(member(), project.id, { title: "Child", parentId: parent.id });
    expect((await member().call("DELETE", `/tasks/${parent.id}`)).status).toBe(200);
    expect((await member().call("GET", `/tasks/${child.id}`)).status).toBe(404);
  });
});

describe("versions", () => {
  it("requires If-Match and refuses a stale version with the current task", async () => {
    const project = await createProject(member(), ws);
    const task = await newTask(member(), project.id);
    expect((await member().call("PATCH", `/tasks/${task.id}`, { title: "x" })).status).toBe(428);
    const first = await patch(task.id, task.version, { title: "Mine" });
    expect(first.status).toBe(200);
    expect(first.body.version).toBe(task.version + 1);
    const stale = await patch(task.id, task.version, { title: "Theirs" });
    expect(stale.status).toBe(409);
    expect(stale.body.error).toBe("version_conflict");
    expect(stale.body.task).toMatchObject({ title: "Mine", version: task.version + 1 });
  });
});

describe("listing and moving", () => {
  it("filters by status, assignee and parent", async () => {
    const project = await createProject(member(), ws);
    const statuses = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses;
    const doing = statuses.find((s: { name: string }) => s.name === "In Progress").id;
    const parent = await newTask(member(), project.id, { title: "Parent" });
    await newTask(member(), project.id, { title: "Mine", assigneeSubject: "usr-member", statusId: doing });
    await newTask(member(), project.id, { title: "Child", parentId: parent.id });
    expect(await titles(project.id, `?status=${doing}`)).toEqual(["Mine"]);
    expect(await titles(project.id, "?assignee=usr-member")).toEqual(["Mine"]);
    expect(await titles(project.id, "?assignee=none")).toEqual(["Parent", "Child"]);
    expect(await titles(project.id, "?parent=root")).toEqual(["Parent", "Mine"]);
    expect(await titles(project.id, `?parent=${parent.id}`)).toEqual(["Child"]);
  });

  it("reorders, reparents and changes status", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(member(), project.id, { title: "A" });
    const b = await newTask(member(), project.id, { title: "B" });
    const c = await newTask(member(), project.id, { title: "C" });
    const move = async (id: string, body: Record<string, unknown>) => {
      const current = (await member().call("GET", `/tasks/${id}`)).body;
      return member().call("POST", `/tasks/${id}/move`, body, { "if-match": String(current.version) });
    };
    expect((await move(c.id, { afterId: null })).status).toBe(200);
    expect(await titles(project.id)).toEqual(["C", "A", "B"]);
    await move(c.id, { afterId: a.id });
    expect(await titles(project.id)).toEqual(["A", "C", "B"]);
    const done = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses.find(
      (s: { name: string }) => s.name === "Done",
    ).id;
    const moved = await move(b.id, { parentId: a.id, statusId: done });
    expect(moved.body).toMatchObject({ parentId: a.id, statusId: done });
    expect((await move(b.id, { afterId: b.id })).body.error).toBe("invalid_after");
  });

  it("deleting a status in use needs somewhere to put its tasks", async () => {
    const project = await createProject(member(), ws);
    const statuses = (await member().call("GET", `/projects/${project.id}/statuses`)).body.statuses;
    const todo = statuses.find((s: { name: string }) => s.name === "Todo").id;
    const backlog = statuses.find((s: { name: string }) => s.name === "Backlog").id;
    const task = await newTask(member(), project.id);
    const refused = await t.as("usr-admin").call("DELETE", `/statuses/${todo}`);
    expect(refused.body.error).toBe("status_in_use");
    expect((await t.as("usr-admin").call("DELETE", `/statuses/${todo}?moveTasksTo=${backlog}`)).status).toBe(200);
    const after = (await member().call("GET", `/tasks/${task.id}`)).body;
    expect(after).toMatchObject({ statusId: backlog, version: task.version + 1 });
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/tasks.test.ts`
Expected: FAIL — task routes return 404.

- [ ] **Step 3: Implement the store**

`src/store/tasks.ts`:

```ts
import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { projectAccess, requireRole, taskAccess, workspaceRole } from "../access";
import { badRequest, conflict, unprocessable } from "../http";
import { rankBetween } from "../rank";
import type { WorkingCalendar } from "../schedule/calendar";
import { now, transaction } from "./db";
import { hasChildren, hasLinks, statusInProject, taskKey } from "./queries";
import type { ProjectRow, TaskRow } from "./rows";
import { durationBetween, finishFor, firstWorkingDay, projectCalendar, reschedule } from "./scheduling";
import { defaultStatusId } from "./statuses";

export const PRIORITIES = ["none", "low", "medium", "high", "urgent"] as const;
export type Priority = (typeof PRIORITIES)[number];

export interface Task {
  id: string;
  projectId: string;
  key: string;
  number: number;
  parentId: string | null;
  rank: string;
  title: string;
  description: string;
  statusId: string;
  priority: Priority;
  assigneeSubject: string | null;
  kind: "task" | "milestone";
  durationDays: number | null;
  startDate: string | null;
  finishDate: string | null;
  constraintType: "asap" | "start_no_earlier_than";
  constraintDate: string | null;
  deadline: string | null;
  progress: number;
  version: number;
  createdAt: string;
  updatedAt: string;
  createdBy: string;
  updatedBy: string;
}

export interface TaskFields {
  title?: string;
  description?: string;
  statusId?: string;
  priority?: Priority;
  assigneeSubject?: string | null;
  kind?: "task" | "milestone";
  durationDays?: number | null;
  startDate?: string | null;
  finishDate?: string | null;
  constraintType?: "asap" | "start_no_earlier_than";
  constraintDate?: string | null;
  deadline?: string | null;
  progress?: number;
}

export interface TaskCreate extends TaskFields {
  title: string;
  parentId?: string | null;
}

export interface TaskMove {
  parentId?: string | null;
  statusId?: string;
  afterId?: string | null;
}

export interface TaskFilters {
  statusId?: string;
  assignee?: string | null;
  parent?: string | null;
}

const SUMMARY_LOCKED = ["kind", "durationDays", "startDate", "finishDate", "constraintType", "constraintDate", "progress"] as const;

type Schedule = Pick<
  TaskRow,
  "kind" | "duration_days" | "start_date" | "finish_date" | "constraint_type" | "constraint_date" | "deadline" | "progress"
>;

const EMPTY_SCHEDULE: Schedule = {
  kind: "task",
  duration_days: null,
  start_date: null,
  finish_date: null,
  constraint_type: "asap",
  constraint_date: null,
  deadline: null,
  progress: 0,
};

export function toTask(row: TaskRow, projectKey: string): Task {
  return {
    id: row.id,
    projectId: row.project_id,
    key: taskKey(projectKey, row.number),
    number: row.number,
    parentId: row.parent_id,
    rank: row.rank,
    title: row.title,
    description: row.description,
    statusId: row.status_id,
    priority: row.priority,
    assigneeSubject: row.assignee_subject,
    kind: row.kind,
    durationDays: row.duration_days,
    startDate: row.start_date,
    finishDate: row.finish_date,
    constraintType: row.constraint_type,
    constraintDate: row.constraint_date,
    deadline: row.deadline,
    progress: row.progress,
    version: row.version,
    createdAt: row.created_at,
    updatedAt: row.updated_at,
    createdBy: row.created_by,
    updatedBy: row.updated_by,
  };
}

function taskRow(db: Database, id: string): TaskRow {
  return db.query("SELECT * FROM tasks WHERE id = ?").get(id) as TaskRow;
}

/** Merges requested schedule fields into stored ones and derives the finish date. */
function resolveSchedule(calendar: WorkingCalendar, mode: "manual" | "auto", current: Schedule, patch: TaskFields): Schedule {
  const next: Schedule = { ...current };
  if (patch.kind !== undefined) next.kind = patch.kind;
  if (patch.durationDays !== undefined) next.duration_days = patch.durationDays;
  if (patch.constraintType !== undefined) next.constraint_type = patch.constraintType;
  if (patch.constraintDate !== undefined) next.constraint_date = patch.constraintDate;
  if (patch.deadline !== undefined) next.deadline = patch.deadline;
  if (patch.progress !== undefined) next.progress = patch.progress;
  if (patch.startDate !== undefined) {
    if (mode === "auto" && patch.startDate !== null) {
      // The engine owns start dates in auto mode; asking for one sets the constraint instead.
      next.constraint_type = "start_no_earlier_than";
      next.constraint_date = patch.startDate;
    } else {
      next.start_date = patch.startDate;
    }
  }

  if (next.constraint_type === "asap") next.constraint_date = null;
  else if (next.constraint_date === null) throw badRequest("constraintDate is required with start_no_earlier_than");

  if (next.kind === "milestone") {
    if (patch.durationDays !== undefined && patch.durationDays !== null && patch.durationDays !== 0) {
      throw unprocessable("milestone_duration", "a milestone has no duration");
    }
    if (patch.finishDate !== undefined && patch.finishDate !== null) {
      throw unprocessable("milestone_duration", "a milestone has no finish date of its own");
    }
    next.duration_days = 0;
  } else {
    if (current.kind === "milestone" && patch.durationDays === undefined && patch.finishDate === undefined) {
      next.duration_days = null;
    }
    if (next.duration_days === 0) {
      throw unprocessable("zero_duration", 'a task lasts at least one working day; use kind "milestone" for a zero-length event');
    }
  }

  if (next.start_date !== null) next.start_date = firstWorkingDay(calendar, next.start_date);
  if (next.kind === "task" && patch.finishDate !== undefined) {
    if (patch.finishDate === null) {
      if (patch.durationDays === undefined) next.duration_days = null;
    } else {
      const anchor = patch.startDate ?? next.start_date;
      if (anchor === null) throw badRequest("finishDate needs a startDate");
      const duration = durationBetween(calendar, firstWorkingDay(calendar, anchor), patch.finishDate);
      if (duration < 1) throw unprocessable("invalid_dates", "finishDate must be on or after startDate");
      next.duration_days = duration;
    }
  }
  next.finish_date =
    next.start_date !== null && next.duration_days !== null ? finishFor(calendar, next.start_date, next.duration_days) : null;
  return next;
}

function assertStatus(db: Database, projectId: string, statusId: string): void {
  if (!statusInProject(db, projectId, statusId)) throw badRequest("statusId must be a status of this project");
}

function assertAssignee(db: Database, project: ProjectRow, subject: string): void {
  if (!workspaceRole(db, project.workspace_id, subject)) {
    throw unprocessable("assignee_not_member", "tasks can only be assigned to members of the project's workspace");
  }
}

function assertParent(db: Database, projectId: string, parentId: string, movingTaskId: string | null): void {
  const parent = db.query("SELECT id, project_id, kind FROM tasks WHERE id = ?").get(parentId) as {
    id: string;
    project_id: string;
    kind: string;
  } | null;
  if (!parent || parent.project_id !== projectId) throw unprocessable("invalid_parent", "parentId must be a task in this project");
  if (parent.kind === "milestone") throw unprocessable("milestone_children", "a milestone cannot have subtasks");
  if (hasLinks(db, parentId)) {
    throw unprocessable("summary_link", "a task with dependencies cannot become a summary task; remove its links first");
  }
  if (movingTaskId === null) return;
  for (let id: string | null = parentId; id !== null; ) {
    if (id === movingTaskId) throw unprocessable("wbs_cycle", "a task cannot move under itself or its own subtask");
    id = (db.query("SELECT parent_id FROM tasks WHERE id = ?").get(id) as { parent_id: string | null }).parent_id;
  }
}

function assertVersion(task: TaskRow, expected: number, projectKey: string): void {
  if (task.version !== expected) {
    throw conflict("version_conflict", "this task was changed by someone else", { task: toTask(task, projectKey) });
  }
}

export function listTasks(db: Database, subject: string, projectId: string, filters: TaskFilters): Task[] {
  const { project } = projectAccess(db, projectId, subject);
  const clauses = ["project_id = ?"];
  const args: string[] = [projectId];
  if (filters.statusId !== undefined) {
    clauses.push("status_id = ?");
    args.push(filters.statusId);
  }
  if (filters.assignee !== undefined) {
    if (filters.assignee === null) clauses.push("assignee_subject IS NULL");
    else {
      clauses.push("assignee_subject = ?");
      args.push(filters.assignee);
    }
  }
  if (filters.parent !== undefined) {
    if (filters.parent === null) clauses.push("parent_id IS NULL");
    else {
      clauses.push("parent_id = ?");
      args.push(filters.parent);
    }
  }
  const rows = db.query(`SELECT * FROM tasks WHERE ${clauses.join(" AND ")} ORDER BY rank, id`).all(...args) as TaskRow[];
  return rows.map((row) => toTask(row, project.key));
}

export function getTask(db: Database, subject: string, id: string): Task {
  const { task, project } = taskAccess(db, id, subject);
  return toTask(task, project.key);
}

export function createTask(db: Database, subject: string, projectId: string, input: TaskCreate): Task {
  const { project, role } = projectAccess(db, projectId, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    const parentId = input.parentId ?? null;
    if (parentId !== null) assertParent(db, project.id, parentId, null);
    const statusId = input.statusId ?? defaultStatusId(db, project.id);
    assertStatus(db, project.id, statusId);
    if (input.assigneeSubject) assertAssignee(db, project, input.assigneeSubject);
    const fields = resolveSchedule(projectCalendar(db, project), project.schedule_mode, EMPTY_SCHEDULE, input);

    const { next_number: number } = db.query("SELECT next_number FROM projects WHERE id = ?").get(project.id) as { next_number: number };
    db.query("UPDATE projects SET next_number = next_number + 1 WHERE id = ?").run(project.id);
    const { last } = db.query("SELECT MAX(rank) AS last FROM tasks WHERE project_id = ?").get(project.id) as { last: string | null };

    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO tasks (id, project_id, number, parent_id, rank, title, description, status_id, priority, assignee_subject,
         kind, duration_days, start_date, finish_date, constraint_type, constraint_date, deadline, progress, version,
         created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 1, ?, ?, ?, ?)`,
    ).run(
      id, project.id, number, parentId, rankBetween(last, null), input.title, input.description ?? "", statusId,
      input.priority ?? "none", input.assigneeSubject ?? null, fields.kind, fields.duration_days, fields.start_date,
      fields.finish_date, fields.constraint_type, fields.constraint_date, fields.deadline, fields.progress,
      at, at, subject, subject,
    );
    reschedule(db, project.id, subject);
    return toTask(taskRow(db, id), project.key);
  });
}

export function updateTask(db: Database, subject: string, id: string, expected: number, patch: TaskFields): Task {
  const { project, role } = taskAccess(db, id, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    const task = taskRow(db, id);
    assertVersion(task, expected, project.key);
    if (hasChildren(db, id) && SUMMARY_LOCKED.some((key) => patch[key] !== undefined)) {
      throw unprocessable("summary_fields", "a summary task's schedule comes from its subtasks");
    }
    if (patch.statusId !== undefined) assertStatus(db, project.id, patch.statusId);
    if (patch.assigneeSubject) assertAssignee(db, project, patch.assigneeSubject);
    const fields = resolveSchedule(projectCalendar(db, project), project.schedule_mode, task, patch);
    db.query(
      `UPDATE tasks SET title = ?, description = ?, status_id = ?, priority = ?, assignee_subject = ?, kind = ?,
         duration_days = ?, start_date = ?, finish_date = ?, constraint_type = ?, constraint_date = ?, deadline = ?,
         progress = ?, version = version + 1, updated_at = ?, updated_by = ?
       WHERE id = ?`,
    ).run(
      patch.title ?? task.title, patch.description ?? task.description, patch.statusId ?? task.status_id,
      patch.priority ?? task.priority, patch.assigneeSubject === undefined ? task.assignee_subject : patch.assigneeSubject,
      fields.kind, fields.duration_days, fields.start_date, fields.finish_date, fields.constraint_type,
      fields.constraint_date, fields.deadline, fields.progress, now(), subject, id,
    );
    reschedule(db, project.id, subject);
    return toTask(taskRow(db, id), project.key);
  });
}

export function moveTask(db: Database, subject: string, id: string, expected: number, move: TaskMove): Task {
  const { project, role } = taskAccess(db, id, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    const task = taskRow(db, id);
    assertVersion(task, expected, project.key);
    if (move.parentId !== undefined && move.parentId !== null) assertParent(db, project.id, move.parentId, id);
    if (move.statusId !== undefined) assertStatus(db, project.id, move.statusId);

    let rank = task.rank;
    if (move.afterId === null) {
      const { first } = db.query("SELECT MIN(rank) AS first FROM tasks WHERE project_id = ? AND id <> ?").get(project.id, id) as {
        first: string | null;
      };
      rank = rankBetween(null, first);
    } else if (move.afterId !== undefined) {
      const after = db.query("SELECT rank FROM tasks WHERE id = ? AND project_id = ? AND id <> ?").get(move.afterId, project.id, id) as {
        rank: string;
      } | null;
      if (!after) throw unprocessable("invalid_after", "afterId must be another task in this project");
      const { following } = db
        .query("SELECT MIN(rank) AS following FROM tasks WHERE project_id = ? AND rank > ? AND id <> ?")
        .get(project.id, after.rank, id) as { following: string | null };
      rank = rankBetween(after.rank, following);
    }

    db.query(
      "UPDATE tasks SET parent_id = ?, status_id = ?, rank = ?, version = version + 1, updated_at = ?, updated_by = ? WHERE id = ?",
    ).run(
      move.parentId === undefined ? task.parent_id : move.parentId,
      move.statusId ?? task.status_id,
      rank,
      now(),
      subject,
      id,
    );
    reschedule(db, project.id, subject);
    return toTask(taskRow(db, id), project.key);
  });
}

export function deleteTask(db: Database, subject: string, id: string): void {
  const { project, role } = taskAccess(db, id, subject);
  requireRole(role, "member");
  transaction(db, () => {
    // ON DELETE CASCADE removes the subtree and every link touching it.
    db.query("DELETE FROM tasks WHERE id = ?").run(id);
    reschedule(db, project.id, subject);
  });
}
```

- [ ] **Step 4: Implement the routes and register them**

`src/routes/tasks.ts`:

```ts
import { HttpError, badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as tasks from "../store/tasks";
import {
  type Body,
  dateValue,
  enumValue,
  field,
  has,
  idValue,
  intValue,
  nullable,
  object,
  requiredText,
  subjectValue,
  text,
} from "../validation";

const FIELDS = [
  "title",
  "description",
  "statusId",
  "priority",
  "assigneeSubject",
  "kind",
  "durationDays",
  "startDate",
  "finishDate",
  "constraintType",
  "constraintDate",
  "deadline",
  "progress",
] as const;

const KINDS = ["task", "milestone"] as const;
const CONSTRAINTS = ["asap", "start_no_earlier_than"] as const;

/** The version the client last saw, from If-Match. Required on every task write. */
export function expectedVersion(req: Request): number {
  const header = req.headers.get("if-match");
  if (header === null) throw new HttpError(428, "version_required", "send If-Match with the task's version");
  const match = /^"?(\d+)"?$/.exec(header.trim());
  if (!match) throw badRequest("If-Match must be the task's version number");
  return Number(match[1]);
}

function parseFields(body: Body): tasks.TaskFields {
  const fields: tasks.TaskFields = {};
  if (has(body, "title")) fields.title = requiredText(body, "title", 512);
  const description = text(body, "description", 50_000);
  if (description !== undefined) fields.description = description;
  const statusId = field(body, "statusId", idValue("statusId"));
  if (statusId !== undefined) fields.statusId = statusId;
  const priority = field(body, "priority", enumValue("priority", tasks.PRIORITIES));
  if (priority !== undefined) fields.priority = priority;
  const assignee = nullable(body, "assigneeSubject", subjectValue("assigneeSubject"));
  if (assignee !== undefined) fields.assigneeSubject = assignee;
  const kind = field(body, "kind", enumValue("kind", KINDS));
  if (kind !== undefined) fields.kind = kind;
  const duration = nullable(body, "durationDays", intValue("durationDays", 0, 3650));
  if (duration !== undefined) fields.durationDays = duration;
  const startDate = nullable(body, "startDate", dateValue("startDate"));
  if (startDate !== undefined) fields.startDate = startDate;
  const finishDate = nullable(body, "finishDate", dateValue("finishDate"));
  if (finishDate !== undefined) fields.finishDate = finishDate;
  const constraintType = field(body, "constraintType", enumValue("constraintType", CONSTRAINTS));
  if (constraintType !== undefined) fields.constraintType = constraintType;
  const constraintDate = nullable(body, "constraintDate", dateValue("constraintDate"));
  if (constraintDate !== undefined) fields.constraintDate = constraintDate;
  const deadline = nullable(body, "deadline", dateValue("deadline"));
  if (deadline !== undefined) fields.deadline = deadline;
  const progress = field(body, "progress", intValue("progress", 0, 100));
  if (progress !== undefined) fields.progress = progress;
  return fields;
}

export function registerTaskRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/tasks", ({ db, subject, url }, params) => {
    const filters: tasks.TaskFilters = {};
    const status = url.searchParams.get("status");
    if (status !== null) filters.statusId = idValue("status")(status);
    const assignee = url.searchParams.get("assignee");
    if (assignee !== null) filters.assignee = assignee === "none" ? null : subjectValue("assignee")(assignee);
    const parent = url.searchParams.get("parent");
    if (parent !== null) filters.parent = parent === "root" ? null : idValue("parent")(parent);
    return json({ tasks: tasks.listTasks(db, subject, param(params, "id"), filters) });
  });

  router.add("POST", "/projects/:id/tasks", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), [...FIELDS, "parentId"]);
    const fields = parseFields(body);
    if (fields.title === undefined) throw badRequest("title is required");
    const input: tasks.TaskCreate = { ...fields, title: fields.title };
    const parentId = nullable(body, "parentId", idValue("parentId"));
    if (parentId !== undefined) input.parentId = parentId;
    return json(tasks.createTask(db, subject, param(params, "id"), input), 201);
  });

  router.add("GET", "/tasks/:id", ({ db, subject }, params) => json(tasks.getTask(db, subject, param(params, "id"))));

  router.add("PATCH", "/tasks/:id", async ({ db, subject, req }, params) => {
    const version = expectedVersion(req);
    const body = object(await readJson(req), FIELDS);
    return json(tasks.updateTask(db, subject, param(params, "id"), version, parseFields(body)));
  });

  router.add("POST", "/tasks/:id/move", async ({ db, subject, req }, params) => {
    const version = expectedVersion(req);
    const body = object(await readJson(req), ["parentId", "statusId", "afterId"]);
    const move: tasks.TaskMove = {};
    const parentId = nullable(body, "parentId", idValue("parentId"));
    if (parentId !== undefined) move.parentId = parentId;
    const statusId = field(body, "statusId", idValue("statusId"));
    if (statusId !== undefined) move.statusId = statusId;
    const afterId = nullable(body, "afterId", idValue("afterId"));
    if (afterId !== undefined) move.afterId = afterId;
    return json(tasks.moveTask(db, subject, param(params, "id"), version, move));
  });

  router.add("DELETE", "/tasks/:id", ({ db, subject }, params) => {
    tasks.deleteTask(db, subject, param(params, "id"));
    return json({ deleted: true });
  });
}
```

In `src/server.ts` add `import { registerTaskRoutes } from "./routes/tasks";` and append `registerTaskRoutes` to `ROUTE_MODULES`.

- [ ] **Step 5: Run the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: `PASS`; `tests/tasks.test.ts` 15 tests pass.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): tasks with a nested WBS, ranks and optimistic concurrency

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---
### Task 13: Dependencies and the project calendar

**Files:**
- Create: `apps/Nexus-Project/src/store/dependencies.ts`, `src/store/calendars.ts`, `src/routes/dependencies.ts`, `src/routes/calendars.ts`
- Modify: `src/server.ts` (register both)
- Test: `tests/dependencies.test.ts`, `tests/calendar-api.test.ts`

**Interfaces:**
- Consumes: `findCycle` (Task 4), `WorkingCalendar` (Task 3), Tasks 7–12.
- Produces: `Dependency { id, projectId, predecessorId, successorId, type, lagDays }`, `LINK_TYPES`, `listDependencies`, `createDependency(db, subject, projectId, input)`, `updateDependency(db, subject, id, patch)`, `deleteDependency`; `ProjectCalendar { workingWeekdays: number[]; exceptions: { date; working }[] }`, `getCalendar`, `setCalendar`; `registerDependencyRoutes`, `registerCalendarRoutes`.

Routes: `GET/POST /projects/:id/dependencies`, `PATCH/DELETE /dependencies/:id`, `GET/PUT /projects/:id/calendar`.

Rules:
- Members and above write links; admins and above write the calendar; viewers read both.
- A link needs two tasks of this project: `422 self_link`, `422 summary_link` (either task has children), `409 duplicate_link`, `422 dependency_cycle` with `cycle: ["KEY-1", "KEY-2", "KEY-1"]`. A task in another project is `422 cross_project_link` if the caller can see that project and `404` if not.
- `lagDays` is −3650…3650; `type` defaults to `FS`, `lagDays` to 0.
- The calendar has at least one working weekday (each 0 = Sunday … 6 = Saturday, no repeats) and at most 1,000 exceptions with unique dates. Changing it re-derives every stored finish date from start + duration (bumping versions) and reschedules.
- Every link or calendar write reschedules the project.

- [ ] **Step 1: Write the failing tests**

`tests/dependencies.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

const member = () => t.as("usr-member");

async function newTask(projectId: string, body: Record<string, unknown> = {}) {
  const res = await member().call("POST", `/projects/${projectId}/tasks`, { title: "Task", durationDays: 1, ...body });
  expect(res.status).toBe(201);
  return res.body as { id: string; key: string };
}

async function linkTasks(projectId: string, predecessorId: string, successorId: string, extra: Record<string, unknown> = {}) {
  return member().call("POST", `/projects/${projectId}/dependencies`, { predecessorId, successorId, ...extra });
}

async function startOf(taskId: string): Promise<string> {
  return (await member().call("GET", `/tasks/${taskId}`)).body.startDate;
}

describe("dependencies", () => {
  it("links two tasks, defaulting to finish-to-start", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    const res = await linkTasks(project.id, a.id, b.id);
    expect(res.status).toBe(201);
    expect(res.body).toMatchObject({ predecessorId: a.id, successorId: b.id, type: "FS", lagDays: 0 });
    const list = await t.as("usr-viewer").call("GET", `/projects/${project.id}/dependencies`);
    expect(list.body.dependencies).toHaveLength(1);
    const viewerTry = await t.as("usr-viewer").call("POST", `/projects/${project.id}/dependencies`, { predecessorId: b.id, successorId: a.id });
    expect(viewerTry.status).toBe(403);
  });

  it("refuses self links, summary links and duplicates", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    const parent = await newTask(project.id, { durationDays: null });
    await newTask(project.id, { parentId: parent.id });
    expect((await linkTasks(project.id, a.id, a.id)).body.error).toBe("self_link");
    expect((await linkTasks(project.id, a.id, parent.id)).body.error).toBe("summary_link");
    expect((await linkTasks(project.id, a.id, b.id)).status).toBe(201);
    const dup = await linkTasks(project.id, a.id, b.id, { type: "SS" });
    expect(dup.status).toBe(409);
    expect(dup.body.error).toBe("duplicate_link");
  });

  it("refuses links across projects, and hides tasks the caller cannot see", async () => {
    const project = await createProject(member(), ws);
    const other = await createProject(member(), ws);
    const hidden = await createProject(t.as("usr-admin"), ws, { visibility: "restricted" });
    const a = await newTask(project.id);
    const foreign = await newTask(other.id);
    const secret = await t.as("usr-admin").call("POST", `/projects/${hidden.id}/tasks`, { title: "Secret" });
    expect((await linkTasks(project.id, a.id, foreign.id)).body.error).toBe("cross_project_link");
    expect((await linkTasks(project.id, a.id, secret.body.id)).status).toBe(404);
  });

  it("names the cycle it refuses", async () => {
    const project = await createProject(member(), ws, { key: "CYC" });
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    const c = await newTask(project.id);
    await linkTasks(project.id, a.id, b.id);
    await linkTasks(project.id, b.id, c.id);
    const res = await linkTasks(project.id, c.id, a.id);
    expect(res.status).toBe(422);
    expect(res.body.error).toBe("dependency_cycle");
    expect(res.body.cycle).toEqual(["CYC-1", "CYC-2", "CYC-3", "CYC-1"]);
  });

  it("stops a linked task from becoming a summary", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    await linkTasks(project.id, a.id, b.id);
    const res = await member().call("POST", `/projects/${project.id}/tasks`, { title: "Child", parentId: a.id });
    expect(res.body.error).toBe("summary_link");
  });

  it("moves successors in auto mode and restores them when the link goes", async () => {
    const project = await createProject(member(), ws, { scheduleMode: "auto" });
    const a = await newTask(project.id, { durationDays: 3 });
    const b = await newTask(project.id, { durationDays: 2 });
    expect(await startOf(b.id)).toBe("2026-09-07");
    const link = (await linkTasks(project.id, a.id, b.id)).body;
    expect(await startOf(b.id)).toBe("2026-09-10");
    expect((await member().call("PATCH", `/dependencies/${link.id}`, { lagDays: 2 })).status).toBe(200);
    expect(await startOf(b.id)).toBe("2026-09-14");
    expect((await member().call("DELETE", `/dependencies/${link.id}`)).status).toBe(200);
    expect(await startOf(b.id)).toBe("2026-09-07");
  });

  it("disappears with a deleted task", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    await linkTasks(project.id, a.id, b.id);
    await member().call("DELETE", `/tasks/${a.id}`);
    expect((await member().call("GET", `/projects/${project.id}/dependencies`)).body.dependencies).toEqual([]);
  });

  it("validates its input", async () => {
    const project = await createProject(member(), ws);
    const a = await newTask(project.id);
    const b = await newTask(project.id);
    expect((await linkTasks(project.id, a.id, b.id, { type: "XX" })).status).toBe(400);
    expect((await linkTasks(project.id, a.id, b.id, { lagDays: 99_999 })).status).toBe(400);
    expect((await linkTasks(project.id, a.id, b.id, { weight: 1 })).status).toBe(400);
    expect((await member().call("POST", `/projects/${project.id}/dependencies`, { predecessorId: a.id })).status).toBe(400);
  });
});
```

`tests/calendar-api.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

const HOLIDAY = { workingWeekdays: [1, 2, 3, 4, 5], exceptions: [{ date: "2026-09-08", working: false }] };

describe("project calendar", () => {
  it("defaults to Monday to Friday with no exceptions", async () => {
    const project = await createProject(t.as("usr-member"), ws);
    const res = await t.as("usr-viewer").call("GET", `/projects/${project.id}/calendar`);
    expect(res.body).toEqual({ workingWeekdays: [1, 2, 3, 4, 5], exceptions: [] });
  });

  it("is changed by admins only and re-derives finish dates", async () => {
    const project = await createProject(t.as("usr-member"), ws);
    const task = (await t.as("usr-member").call("POST", `/projects/${project.id}/tasks`, { title: "A", startDate: "2026-09-07", durationDays: 3 })).body;
    expect(task.finishDate).toBe("2026-09-09");
    expect((await t.as("usr-member").call("PUT", `/projects/${project.id}/calendar`, HOLIDAY)).status).toBe(403);
    const res = await t.as("usr-admin").call("PUT", `/projects/${project.id}/calendar`, HOLIDAY);
    expect(res.status).toBe(200);
    expect(res.body).toEqual(HOLIDAY);
    const after = (await t.as("usr-member").call("GET", `/tasks/${task.id}`)).body;
    expect(after).toMatchObject({ finishDate: "2026-09-10", durationDays: 3, version: task.version + 1 });
  });

  it("reschedules an auto project", async () => {
    const project = await createProject(t.as("usr-member"), ws, { scheduleMode: "auto" });
    const post = (body: Record<string, unknown>) => t.as("usr-member").call("POST", `/projects/${project.id}/tasks`, body);
    const a = (await post({ title: "A", durationDays: 3 })).body;
    const b = (await post({ title: "B", durationDays: 1 })).body;
    await t.as("usr-member").call("POST", `/projects/${project.id}/dependencies`, { predecessorId: a.id, successorId: b.id });
    expect((await t.as("usr-member").call("GET", `/tasks/${b.id}`)).body.startDate).toBe("2026-09-10");
    await t.as("usr-admin").call("PUT", `/projects/${project.id}/calendar`, HOLIDAY);
    expect((await t.as("usr-member").call("GET", `/tasks/${b.id}`)).body.startDate).toBe("2026-09-11");
  });

  it("validates its input", async () => {
    const project = await createProject(t.as("usr-member"), ws);
    const put = (body: unknown) => t.as("usr-admin").call("PUT", `/projects/${project.id}/calendar`, body);
    expect((await put({ workingWeekdays: [], exceptions: [] })).status).toBe(400);
    expect((await put({ workingWeekdays: [7], exceptions: [] })).status).toBe(400);
    expect((await put({ workingWeekdays: [1, 1], exceptions: [] })).status).toBe(400);
    expect((await put({ workingWeekdays: [1], exceptions: [{ date: "2026-09-08", working: false }, { date: "2026-09-08", working: true }] })).status).toBe(400);
    expect((await put({ workingWeekdays: [1], exceptions: [{ date: "2026-02-30", working: false }] })).status).toBe(400);
    expect((await put({ workingWeekdays: [1], exceptions: [{ date: "2026-09-08" }] })).status).toBe(400);
    const many = Array.from({ length: 1001 }, (_, i) => ({ date: new Date(Date.UTC(2026, 0, 1 + i)).toISOString().slice(0, 10), working: false }));
    expect((await put({ workingWeekdays: [1], exceptions: many })).status).toBe(400);
  });
});
```

- [ ] **Step 2: Run to verify they fail**

Run: `cd apps/Nexus-Project && bun test tests/dependencies.test.ts tests/calendar-api.test.ts`
Expected: FAIL — the routes return 404.

- [ ] **Step 3: Implement the stores**

`src/store/dependencies.ts`:

```ts
import type { Database } from "bun:sqlite";
import { randomUUID } from "node:crypto";
import { type Role, projectAccess, requireRole } from "../access";
import { conflict, notFound, unprocessable } from "../http";
import { findCycle } from "../schedule/graph";
import type { LinkType } from "../schedule/types";
import { now, transaction } from "./db";
import { hasChildren, taskKey } from "./queries";
import type { DependencyRow, ProjectRow } from "./rows";
import { reschedule } from "./scheduling";

export const LINK_TYPES: readonly LinkType[] = ["FS", "SS", "FF", "SF"];

export interface Dependency {
  id: string;
  projectId: string;
  predecessorId: string;
  successorId: string;
  type: LinkType;
  lagDays: number;
}

export interface DependencyInput {
  predecessorId: string;
  successorId: string;
  type: LinkType;
  lagDays: number;
}

export interface DependencyPatch {
  type?: LinkType;
  lagDays?: number;
}

function toDependency(row: DependencyRow): Dependency {
  return {
    id: row.id,
    projectId: row.project_id,
    predecessorId: row.predecessor_id,
    successorId: row.successor_id,
    type: row.type,
    lagDays: row.lag_days,
  };
}

function linkAccess(db: Database, id: string, subject: string): { link: DependencyRow; project: ProjectRow; role: Role } {
  const link = db.query("SELECT * FROM dependencies WHERE id = ?").get(id) as DependencyRow | null;
  if (!link) throw notFound("dependency");
  try {
    return { link, ...projectAccess(db, link.project_id, subject) };
  } catch {
    throw notFound("dependency");
  }
}

/** A task a link may use. Elsewhere: 422 if the caller can see where it lives, 404 if not. */
function linkableTask(db: Database, project: ProjectRow, taskId: string, subject: string): void {
  const task = db.query("SELECT id, project_id FROM tasks WHERE id = ?").get(taskId) as { id: string; project_id: string } | null;
  if (!task) throw notFound("task");
  if (task.project_id !== project.id) {
    try {
      projectAccess(db, task.project_id, subject);
    } catch {
      throw notFound("task");
    }
    throw unprocessable("cross_project_link", "dependencies must stay within one project");
  }
  if (hasChildren(db, task.id)) {
    throw unprocessable("summary_link", "summary tasks are scheduled by their subtasks and cannot be linked");
  }
}

export function listDependencies(db: Database, subject: string, projectId: string): Dependency[] {
  projectAccess(db, projectId, subject);
  const rows = db.query("SELECT * FROM dependencies WHERE project_id = ? ORDER BY created_at, id").all(projectId) as DependencyRow[];
  return rows.map(toDependency);
}

export function createDependency(db: Database, subject: string, projectId: string, input: DependencyInput): Dependency {
  const { project, role } = projectAccess(db, projectId, subject);
  requireRole(role, "member");
  if (input.predecessorId === input.successorId) throw unprocessable("self_link", "a task cannot depend on itself");
  return transaction(db, () => {
    linkableTask(db, project, input.predecessorId, subject);
    linkableTask(db, project, input.successorId, subject);
    if (db.query("SELECT 1 FROM dependencies WHERE predecessor_id = ? AND successor_id = ?").get(input.predecessorId, input.successorId)) {
      throw conflict("duplicate_link", "these tasks are already linked");
    }
    const tasks = db.query("SELECT id, number FROM tasks WHERE project_id = ? ORDER BY number").all(project.id) as {
      id: string;
      number: number;
    }[];
    const edges = (
      db.query("SELECT predecessor_id, successor_id FROM dependencies WHERE project_id = ? ORDER BY created_at, id").all(project.id) as {
        predecessor_id: string;
        successor_id: string;
      }[]
    ).map((row) => ({ from: row.predecessor_id, to: row.successor_id }));
    edges.push({ from: input.predecessorId, to: input.successorId });
    const cycle = findCycle(tasks.map((task) => task.id), edges);
    if (cycle) {
      const numbers = new Map(tasks.map((task) => [task.id, task.number]));
      const keys = cycle.map((id) => taskKey(project.key, numbers.get(id) as number));
      throw unprocessable("dependency_cycle", `this link would create a cycle: ${keys.join(" → ")}`, { cycle: keys });
    }
    const id = randomUUID();
    const at = now();
    db.query(
      `INSERT INTO dependencies (id, project_id, predecessor_id, successor_id, type, lag_days, created_at, updated_at, created_by, updated_by)
       VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)`,
    ).run(id, project.id, input.predecessorId, input.successorId, input.type, input.lagDays, at, at, subject, subject);
    reschedule(db, project.id, subject);
    return toDependency(db.query("SELECT * FROM dependencies WHERE id = ?").get(id) as DependencyRow);
  });
}

export function updateDependency(db: Database, subject: string, id: string, patch: DependencyPatch): Dependency {
  const { link, role } = linkAccess(db, id, subject);
  requireRole(role, "member");
  return transaction(db, () => {
    db.query("UPDATE dependencies SET type = ?, lag_days = ?, updated_at = ?, updated_by = ? WHERE id = ?").run(
      patch.type ?? link.type,
      patch.lagDays ?? link.lag_days,
      now(),
      subject,
      id,
    );
    reschedule(db, link.project_id, subject);
    return toDependency(db.query("SELECT * FROM dependencies WHERE id = ?").get(id) as DependencyRow);
  });
}

export function deleteDependency(db: Database, subject: string, id: string): void {
  const { link, role } = linkAccess(db, id, subject);
  requireRole(role, "member");
  transaction(db, () => {
    db.query("DELETE FROM dependencies WHERE id = ?").run(id);
    reschedule(db, link.project_id, subject);
  });
}
```

`src/store/calendars.ts`:

```ts
import type { Database } from "bun:sqlite";
import { projectAccess, requireRole } from "../access";
import { badRequest } from "../http";
import { WorkingCalendar } from "../schedule/calendar";
import type { CalendarException } from "../schedule/types";
import { transaction } from "./db";
import type { ProjectRow } from "./rows";
import { loadCalendar, maskFromWeekdays, refreshDerivedFinishes, reschedule } from "./scheduling";

export interface ProjectCalendar {
  workingWeekdays: number[];
  exceptions: CalendarException[];
}

function snapshot(db: Database, project: ProjectRow): ProjectCalendar {
  const spec = loadCalendar(db, project);
  return { workingWeekdays: [...spec.workingWeekdays], exceptions: [...spec.exceptions] };
}

export function getCalendar(db: Database, subject: string, projectId: string): ProjectCalendar {
  return snapshot(db, projectAccess(db, projectId, subject).project);
}

export function setCalendar(db: Database, subject: string, projectId: string, input: ProjectCalendar): ProjectCalendar {
  const { project, role } = projectAccess(db, projectId, subject);
  requireRole(role, "admin");
  try {
    new WorkingCalendar(input);
  } catch (error) {
    throw badRequest((error as Error).message);
  }
  return transaction(db, () => {
    db.query("UPDATE projects SET working_weekdays = ? WHERE id = ?").run(maskFromWeekdays(input.workingWeekdays), project.id);
    db.query("DELETE FROM calendar_exceptions WHERE project_id = ?").run(project.id);
    const insert = db.query("INSERT INTO calendar_exceptions (project_id, date, working) VALUES (?, ?, ?)");
    for (const exception of input.exceptions) insert.run(project.id, exception.date, exception.working ? 1 : 0);
    const updated = db.query("SELECT * FROM projects WHERE id = ?").get(project.id) as ProjectRow;
    refreshDerivedFinishes(db, updated, subject);
    reschedule(db, project.id, subject);
    return snapshot(db, updated);
  });
}
```

- [ ] **Step 4: Implement the routes and register them**

`src/routes/dependencies.ts`:

```ts
import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as dependencies from "../store/dependencies";
import { enumValue, field, idValue, intValue, object } from "../validation";

const LAG = intValue("lagDays", -3650, 3650);

export function registerDependencyRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/dependencies", ({ db, subject }, params) =>
    json({ dependencies: dependencies.listDependencies(db, subject, param(params, "id")) }),
  );

  router.add("POST", "/projects/:id/dependencies", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["predecessorId", "successorId", "type", "lagDays"]);
    const predecessorId = field(body, "predecessorId", idValue("predecessorId"));
    const successorId = field(body, "successorId", idValue("successorId"));
    if (predecessorId === undefined || successorId === undefined) throw badRequest("predecessorId and successorId are required");
    const input: dependencies.DependencyInput = {
      predecessorId,
      successorId,
      type: field(body, "type", enumValue("type", dependencies.LINK_TYPES)) ?? "FS",
      lagDays: field(body, "lagDays", LAG) ?? 0,
    };
    return json(dependencies.createDependency(db, subject, param(params, "id"), input), 201);
  });

  router.add("PATCH", "/dependencies/:id", async ({ db, subject, req }, params) => {
    const body = object(await readJson(req), ["type", "lagDays"]);
    const patch: dependencies.DependencyPatch = {};
    const type = field(body, "type", enumValue("type", dependencies.LINK_TYPES));
    if (type !== undefined) patch.type = type;
    const lagDays = field(body, "lagDays", LAG);
    if (lagDays !== undefined) patch.lagDays = lagDays;
    return json(dependencies.updateDependency(db, subject, param(params, "id"), patch));
  });

  router.add("DELETE", "/dependencies/:id", ({ db, subject }, params) => {
    dependencies.deleteDependency(db, subject, param(params, "id"));
    return json({ deleted: true });
  });
}
```

`src/routes/calendars.ts`:

```ts
import { badRequest, json, readJson } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import * as calendars from "../store/calendars";
import { boolValue, dateValue, intValue, object } from "../validation";

const WEEKDAY = intValue("workingWeekdays[]", 0, 6);
const DATE = dateValue("exceptions[].date");
const WORKING = boolValue("exceptions[].working");

function parseCalendar(value: unknown): calendars.ProjectCalendar {
  const body = object(value, ["workingWeekdays", "exceptions"]);
  if (!Array.isArray(body.workingWeekdays) || body.workingWeekdays.length === 0) {
    throw badRequest("workingWeekdays must list at least one day (0 = Sunday … 6 = Saturday)");
  }
  const workingWeekdays = body.workingWeekdays.map(WEEKDAY);
  if (new Set(workingWeekdays).size !== workingWeekdays.length) throw badRequest("workingWeekdays must not repeat a day");
  const rawExceptions = body.exceptions ?? [];
  if (!Array.isArray(rawExceptions) || rawExceptions.length > 1000) throw badRequest("exceptions must be a list of at most 1000 dates");
  const exceptions = rawExceptions.map((raw) => {
    const entry = object(raw, ["date", "working"]);
    if (entry.working === undefined) throw badRequest("each exception needs date and working");
    return { date: DATE(entry.date), working: WORKING(entry.working) };
  });
  if (new Set(exceptions.map((e) => e.date)).size !== exceptions.length) throw badRequest("exception dates must be unique");
  return { workingWeekdays: [...workingWeekdays].sort((a, b) => a - b), exceptions };
}

export function registerCalendarRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/calendar", ({ db, subject }, params) =>
    json(calendars.getCalendar(db, subject, param(params, "id"))),
  );

  router.add("PUT", "/projects/:id/calendar", async ({ db, subject, req }, params) =>
    json(calendars.setCalendar(db, subject, param(params, "id"), parseCalendar(await readJson(req)))),
  );
}
```

In `src/server.ts` add both imports (`registerDependencyRoutes` from `./routes/dependencies`, `registerCalendarRoutes` from `./routes/calendars`) and append both to `ROUTE_MODULES`.

- [ ] **Step 5: Run the gate**

Run: `cd apps/Nexus-Project && bash check.sh`
Expected: `PASS`; `tests/dependencies.test.ts` 8 and `tests/calendar-api.test.ts` 4 tests pass.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): dependencies with cycle detection and a working-day calendar

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 14: The schedule, My tasks, and the finished gate

**Files:**
- Create: `apps/Nexus-Project/src/store/me.ts`, `src/routes/schedule.ts`, `README.md`
- Modify: `src/server.ts` (register)
- Test: `tests/schedule-api.test.ts`

**Interfaces:**
- Consumes: `computeSchedule` (Task 11), `toTask` (Task 12), access (Task 7).
- Produces: `GET /projects/:id/schedule` → `ScheduleResult` (Task 3 types); `GET /me/tasks` → `{ tasks: (Task & { projectKey: string; projectName: string })[] }`; `listAssignedTasks(db, subject)`; `registerScheduleRoutes(router)`.

Rules: the schedule is readable by viewers and above. My tasks lists tasks assigned to the caller in every project they can see, excluding archived projects and tasks whose status is completed or canceled, ordered by deadline (none last), then project key, then number.

- [ ] **Step 1: Write the failing test**

`tests/schedule-api.test.ts`:

```ts
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createProject, teamWithRoles } from "./support/fixtures";
import { startTestServer } from "./support/server";

let t: Awaited<ReturnType<typeof startTestServer>>;
let ws: string;
beforeAll(async () => {
  t = await startTestServer();
  ws = await teamWithRoles(t);
});
afterAll(() => t.close());

const member = () => t.as("usr-member");
const post = async (projectId: string, body: Record<string, unknown>) => {
  const res = await member().call("POST", `/projects/${projectId}/tasks`, body);
  expect(res.status).toBe(201);
  return res.body;
};

describe("GET /projects/:id/schedule", () => {
  it("reports dates, the critical path, violations, roll-ups and warnings", async () => {
    const project = await createProject(member(), ws);
    const phase = await post(project.id, { title: "Phase" });
    const a = await post(project.id, { title: "A", parentId: phase.id, startDate: "2026-09-07", durationDays: 3 });
    const b = await post(project.id, { title: "B", parentId: phase.id, startDate: "2026-09-08", durationDays: 2 });
    const u = await post(project.id, { title: "Unestimated" });
    const ab = (await member().call("POST", `/projects/${project.id}/dependencies`, { predecessorId: a.id, successorId: b.id })).body;
    const bu = (await member().call("POST", `/projects/${project.id}/dependencies`, { predecessorId: b.id, successorId: u.id })).body;

    const res = await t.as("usr-viewer").call("GET", `/projects/${project.id}/schedule`);
    expect(res.status).toBe(200);
    expect(res.body.projectFinish).toBe("2026-09-11");
    expect(res.body.criticalPath).toEqual([a.id, b.id]);
    expect(res.body.violations).toEqual([{ linkId: ab.id, predecessorId: a.id, successorId: b.id, type: "FS", gapDays: 2 }]);
    expect(res.body.summaries).toEqual([{ id: phase.id, start: "2026-09-07", finish: "2026-09-11", progress: 0 }]);
    expect(res.body.warnings).toEqual([{ code: "link_ignored", linkId: bu.id, taskId: u.id, reason: "unestimated" }]);
    // Manual mode reports; it does not move the stored date.
    expect((await member().call("GET", `/tasks/${b.id}`)).body.startDate).toBe("2026-09-08");
  });

  it("is hidden from strangers", async () => {
    const project = await createProject(member(), ws);
    expect((await t.as("usr-stranger").call("GET", `/projects/${project.id}/schedule`)).status).toBe(404);
  });
});

describe("GET /me/tasks", () => {
  it("lists open tasks assigned to me in projects I can see", async () => {
    const one = await createProject(member(), ws, { key: "ONE" });
    const two = await createProject(member(), ws, { key: "TWO" });
    const archived = await createProject(member(), ws, { key: "OLD" });
    const hidden = await createProject(t.as("usr-admin"), ws, { key: "HID", visibility: "restricted" });
    const statuses = (await member().call("GET", `/projects/${one.id}/statuses`)).body.statuses;
    const done = statuses.find((s: { name: string }) => s.name === "Done").id;

    await post(one.id, { title: "Later", assigneeSubject: "usr-member", deadline: "2026-10-01" });
    await post(two.id, { title: "Sooner", assigneeSubject: "usr-member", deadline: "2026-09-15" });
    await post(one.id, { title: "Whenever", assigneeSubject: "usr-member" });
    await post(one.id, { title: "Finished", assigneeSubject: "usr-member", statusId: done });
    await post(one.id, { title: "Someone else's", assigneeSubject: "usr-admin" });
    await post(archived.id, { title: "Archived", assigneeSubject: "usr-member" });
    await t.as("usr-admin").call("PATCH", `/projects/${archived.id}`, { archived: true });
    await t.as("usr-admin").call("POST", `/projects/${hidden.id}/tasks`, { title: "Hidden", assigneeSubject: "usr-member" });

    const res = await member().call("GET", "/me/tasks");
    expect(res.status).toBe(200);
    expect(res.body.tasks.map((task: { title: string; projectKey: string }) => [task.projectKey, task.title])).toEqual([
      ["TWO", "Sooner"],
      ["ONE", "Later"],
      ["ONE", "Whenever"],
    ]);
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Project && bun test tests/schedule-api.test.ts`
Expected: FAIL — the routes return 404.

- [ ] **Step 3: Implement**

`src/store/me.ts`:

```ts
import type { Database } from "bun:sqlite";
import type { TaskRow } from "./rows";
import { type Task, toTask } from "./tasks";

export type AssignedTask = Task & { projectKey: string; projectName: string };

/** Open tasks assigned to `subject`, in every project they can currently see. */
export function listAssignedTasks(db: Database, subject: string): AssignedTask[] {
  const rows = db
    .query(
      `SELECT t.*, p.key AS project_key, p.name AS project_name
       FROM tasks t
       JOIN projects p ON p.id = t.project_id
       JOIN statuses s ON s.id = t.status_id
       JOIN workspace_members m ON m.workspace_id = p.workspace_id AND m.subject = ?
       WHERE t.assignee_subject = ?
         AND p.archived = 0
         AND s.category NOT IN ('completed', 'canceled')
         AND (p.visibility = 'workspace' OR m.role IN ('owner', 'admin')
              OR EXISTS (SELECT 1 FROM project_members pm WHERE pm.project_id = p.id AND pm.subject = ?))
       ORDER BY t.deadline IS NULL, t.deadline, p.key, t.number`,
    )
    .all(subject, subject, subject) as (TaskRow & { project_key: string; project_name: string })[];
  return rows.map((row) => ({ ...toTask(row, row.project_key), projectKey: row.project_key, projectName: row.project_name }));
}
```

`src/routes/schedule.ts`:

```ts
import { projectAccess } from "../access";
import { json } from "../http";
import { type Router, param } from "../router";
import type { Context } from "../server";
import { listAssignedTasks } from "../store/me";
import { computeSchedule } from "../store/scheduling";

export function registerScheduleRoutes(router: Router<Context>): void {
  router.add("GET", "/projects/:id/schedule", ({ db, subject }, params) => {
    const { project } = projectAccess(db, param(params, "id"), subject);
    return json(computeSchedule(db, project));
  });

  router.add("GET", "/me/tasks", ({ db, subject }) => json({ tasks: listAssignedTasks(db, subject) }));
}
```

In `src/server.ts` add `import { registerScheduleRoutes } from "./routes/schedule";` and append `registerScheduleRoutes` to `ROUTE_MODULES`.

- [ ] **Step 4: Write the README**

`apps/Nexus-Project/README.md`:

````markdown
# Nexus-Project

Project management for one person or a team, with the same code: boards and
lists for day-to-day work, and a real critical-path schedule for plans.

- **Workspaces.** Everyone gets a personal workspace on first use. Teams share
  a team workspace with roles: viewer, member, admin, owner.
- **Projects** have their own statuses (each mapped to backlog, unstarted,
  started, completed or canceled), a working-day calendar, and a schedule mode.
- **Tasks** nest to any depth (a WBS). A task with subtasks is a summary whose
  dates and progress come from its subtasks.
- **Dependencies** of all four types (FS, SS, FF, SF) with lag or lead, within
  a project, checked for cycles.
- **Scheduling.** Manual mode never moves your dates and reports broken links as
  violations; auto mode schedules every task as early as its links allow. Both
  compute float and the critical path; deadlines show as negative float.

The engine in `src/schedule/` is pure (no database, clock or I/O) so the
frontend can run the same code for instant previews.

## Running

```bash
npm install --no-audit --no-fund   # not bun install: it hangs in this monorepo
bun run src/index.ts               # 127.0.0.1:3152
bash check.sh                      # typecheck + every test file must run
```

| Variable | Default | Purpose |
|---|---|---|
| `PORT` | 3152 | Listen port |
| `NEXUS_BIND_HOST` | 127.0.0.1 | Keep on loopback in production |
| `NEXUS_PROJECT_DB` | data/project.sqlite | SQLite path |
| `NEXUS_PROJECT_DASHBOARD_SECRET` | (none) | Secret for Dashboard's private hop; without it that hop is refused |
| `NEXUS_PROJECT_JWT_AUDIENCE` | project.tnhc.dev | Required audience on `x-nexus-identity` |
| `NEXUS_AUTH_INTERNAL_URL` | http://127.0.0.1:4310 | Where Auth's JWKS lives |
| `NEXUS_PROJECT_BASE_URL` | http://localhost:$PORT | Address registered with Cloud |
| `NEXUS_CLOUD_URL` / `NEXUS_CLOUD_API_KEY` | http://localhost:8787 / (none) | Cloud registration |
| `NEXUS_PROJECT_ENABLE_CLOUD_INTEGRATION` | true | Turn Cloud registration off |

## API

Everything below is under `/api/v1/project` and needs an identity from a
trusted front door (see `src/auth.ts`). Errors are
`{ "error": "<code>", "message": "…" }`. Something you cannot see is `404`;
something you can see but may not change is `403`.

| Method | Path | Notes |
|---|---|---|
| GET | /workspaces | Creates your personal workspace on first call |
| POST | /workspaces | `{ name }` → team workspace, you are owner |
| PATCH / DELETE | /workspaces/:id | Rename (admin) / delete (owner, team only) |
| GET | /workspaces/:id/members | |
| PUT / DELETE | /workspaces/:id/members/:subject | `{ role }`; a workspace keeps one owner |
| GET / POST | /workspaces/:id/projects | `?archived=true`; `{ key, name, startDate, … }` |
| GET / PATCH | /projects/:id | Settings: admin |
| POST | /projects/:id/move | `{ workspaceId }`, admin in both |
| GET / PUT / DELETE | /projects/:id/members[/:subject] | Who sees a restricted project |
| GET / POST | /projects/:id/statuses | |
| PATCH / DELETE | /statuses/:id | `DELETE ?moveTasksTo=<statusId>` when in use |
| GET / POST | /projects/:id/tasks | `?status=&assignee=<subject or none>&parent=<id or root>` |
| GET / PATCH / DELETE | /tasks/:id | `PATCH` needs `If-Match: <version>` |
| POST | /tasks/:id/move | `{ parentId, statusId, afterId }`, needs `If-Match` |
| GET / POST | /projects/:id/dependencies | `{ predecessorId, successorId, type, lagDays }` |
| PATCH / DELETE | /dependencies/:id | |
| GET / PUT | /projects/:id/calendar | `{ workingWeekdays: [1..5], exceptions: [{ date, working }] }` |
| GET | /projects/:id/schedule | Dates, float, critical path, violations, roll-ups, warnings |
| GET | /me/tasks | Open tasks assigned to you, soonest deadline first |

Dates are `YYYY-MM-DD`; durations and lags are working days. A milestone is
shown on the working day at whose start it occurs.
````

- [ ] **Step 5: Run the full gate and format**

```bash
cd apps/Nexus-Project
./node_modules/.bin/biome format --write src tests
bash check.sh
```

Expected: `PASS`. The test-file count equals the number of `tests/*.test.ts` files (19). Then run the service once for real and exercise it:

```bash
cd apps/Nexus-Project
NEXUS_PROJECT_DB=/tmp/claude-project-smoke.sqlite NEXUS_PROJECT_DASHBOARD_SECRET=smoke NEXUS_PROJECT_ENABLE_CLOUD_INTEGRATION=false PORT=3998 bun run src/index.ts &
SERVER=$!
sleep 1
curl -s http://127.0.0.1:3998/health
curl -s -H 'x-nexus-subject: usr-smoke' -H 'x-nexus-dashboard-secret: smoke' http://127.0.0.1:3998/api/v1/project/workspaces
kill $SERVER; rm -f /tmp/claude-project-smoke.sqlite*
```

Expected: a health body with `"status":"ok"`, then `{"workspaces":[{…"name":"Personal","kind":"personal","role":"owner"…}]}`.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Project
git commit -m "feat(project): schedule and My tasks endpoints; document the service

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

## After this plan

Plan 2 (frontend and delivery) is written once this lands, against the real API: the Board, List, Timeline and Home views; the Dashboard proxied-app factory (Calendar migrated onto it); a Caddy front door on port 8093; `deploy.sh` registration of `project` and `nexus-project-web`; adding `apps/Nexus-Project/check.sh` to CI; and the `project.tnhc.dev` DNS record. Auth has no username→subject lookup today, so Plan 2 also decides how the member picker resolves people.
