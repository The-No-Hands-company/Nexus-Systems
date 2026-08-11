# SSO Phase 3 — The Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give the ecosystem a front door — `app.tnhc.dev`, where a stranger requests access, a returning visitor claims their account, and a signed-in user sees every app they can reach.

**Architecture:** One Bun process in `apps/Nexus-Dashboard` serves the built React SPA **and** reverse-proxies `/api/v1/auth/*` to Nexus-Auth. That makes every auth call same-origin, which avoids credentialed CORS across subdomains entirely — the fiddliest part of a browser SSO. The app grid is read from Cloud's registry, never hardcoded.

**Tech Stack:** Bun (server), React + Vite + Tailwind + vitest (SPA) — matching `apps/Nexus-Draw/frontend`.

**Spec:** `docs/superpowers/specs/2026-08-11-single-sign-on-and-front-door-design.md`
**Depends on:** Phase 1 (identity lifecycle) and Phase 2 (the gate), both merged.

## Global Constraints

- **`app.tnhc.dev` is PUBLIC — never gated.** It hosts request-access and claim, which unauthenticated people must reach. Gating it would be the same deadlock as gating the auth host. The *apps* are gated; the dashboard is the door to them.
- **Server tests use `bun test`; SPA tests use `vitest`.** Never mix the runners — `bun test` from an app root sweeps up vitest files and reports phantom failures.
- **No secrets in the SPA bundle.** The browser only ever talks to same-origin `/api/...`; the Cloud API key lives in the dashboard server and is never sent to the client.
- **The grid is data, never a hardcoded list.** A new app must appear by registering with Cloud, not by editing the dashboard.
- **Codes are shown once.** Claim codes and recovery codes are rendered exactly where the API returns them and never re-fetched, because they cannot be.
- Type check (`bunx tsc --noEmit` server, `tsc --noEmit` SPA) before every commit.

---

## File Structure

| File | Responsibility |
|---|---|
| `apps/Nexus-Dashboard/src/apps.ts` (create) | Read Cloud's registry, shape it into grid entries |
| `apps/Nexus-Dashboard/src/server.ts` (rewrite) | Static SPA + `/api/v1/auth/*` proxy + `/api/apps` |
| `apps/Nexus-Dashboard/tests/apps.test.ts` (create) | Registry filtering |
| `apps/Nexus-Dashboard/tests/server.test.ts` (rewrite) | Routing, proxying, header hygiene |
| `apps/Nexus-Dashboard/frontend/` (create) | Vite + React + Tailwind SPA |
| `frontend/src/api.ts` | Typed calls to the same-origin API |
| `frontend/src/pages/RequestAccess.tsx` | Public: request form, shows claim code once |
| `frontend/src/pages/Claim.tsx` | Public: redeem code, set password, show recovery codes once |
| `frontend/src/pages/Grid.tsx` | Signed in: app tiles from `/api/apps` |
| `frontend/src/pages/Account.tsx` | Signed in: password, recovery codes, sessions |
| `frontend/src/pages/Admin.tsx` | Operator: queue, approve/reject, invites |
| `deploy/production/deploy.sh` (modify) | Start the dashboard |

The registry read is its own module because it is the only part of the server with real logic worth testing in isolation; everything else in `server.ts` is routing.

---

### Task 1: Grid entries from Cloud's registry

**Files:**
- Create: `apps/Nexus-Dashboard/src/apps.ts`
- Test: `apps/Nexus-Dashboard/tests/apps.test.ts`

**Interfaces:**
- Produces:
  - `type AppEntry = { id: string; name: string; description: string; url: string; health: "healthy" | "offline" }`
  - `toAppEntries(tools: unknown, authHost: string): AppEntry[]`

Only 3 of the 85 registered tools currently carry a `publicUrl`; the rest are
empty scaffolds. The grid must show what a user can actually open, so entries
without a public URL are dropped, and the auth host is dropped too — it is the
sign-in provider, not a destination you click into.

- [ ] **Step 1: Write the failing test**

Create `apps/Nexus-Dashboard/tests/apps.test.ts`:

```ts
import { describe, it, expect } from "bun:test";
import { toAppEntries } from "../src/apps";

const AUTH = "auth.tnhc.dev";

const TOOLS = {
  tools: [
    { id: "nexus-cloud", name: "Nexus Cloud", description: "Control panel",
      publicUrl: "https://cloud.tnhc.dev", health: "healthy", registrationStatus: "active" },
    { id: "nexus-auth", name: "Nexus Auth", description: "Identity",
      publicUrl: "https://auth.tnhc.dev", health: "healthy", registrationStatus: "active" },
    { id: "nexus-chat", name: "Nexus Chat", description: "Chat",
      publicUrl: "https://chat.tnhc.dev", health: "healthy", registrationStatus: "registered" },
    { id: "nexus-video", name: "Nexus Video", description: "Video",
      health: "offline", registrationStatus: "offline" },
  ],
};

describe("app grid entries", () => {
  it("keeps only tools a user can actually open", () => {
    const entries = toAppEntries(TOOLS, AUTH);
    expect(entries.map((e) => e.id).sort()).toEqual(["nexus-chat", "nexus-cloud"]);
  });

  it("drops the auth host — it is the sign-in provider, not a destination", () => {
    expect(toAppEntries(TOOLS, AUTH).some((e) => e.url.includes(AUTH))).toBe(false);
  });

  it("drops the 82 scaffolds that have no public URL", () => {
    expect(toAppEntries(TOOLS, AUTH).some((e) => e.id === "nexus-video")).toBe(false);
  });

  it("carries health through so a down app shows as down rather than a dead link", () => {
    const entries = toAppEntries({
      tools: [{ id: "x", name: "X", publicUrl: "https://x.tnhc.dev", health: "offline" }],
    }, AUTH);
    expect(entries[0]!.health).toBe("offline");
  });

  it("sorts by name so the grid does not reshuffle between polls", () => {
    const entries = toAppEntries({
      tools: [
        { id: "b", name: "Zeta", publicUrl: "https://z.tnhc.dev", health: "healthy" },
        { id: "a", name: "Alpha", publicUrl: "https://a.tnhc.dev", health: "healthy" },
      ],
    }, AUTH);
    expect(entries.map((e) => e.name)).toEqual(["Alpha", "Zeta"]);
  });

  it("survives a malformed payload rather than taking the dashboard down", () => {
    expect(toAppEntries(null, AUTH)).toEqual([]);
    expect(toAppEntries({}, AUTH)).toEqual([]);
    expect(toAppEntries({ tools: "nope" }, AUTH)).toEqual([]);
    expect(toAppEntries({ tools: [null, 42] }, AUTH)).toEqual([]);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd apps/Nexus-Dashboard && bun test tests/apps.test.ts`
Expected: FAIL — cannot resolve `../src/apps`.

- [ ] **Step 3: Implement**

Create `apps/Nexus-Dashboard/src/apps.ts`:

```ts
/**
 * Turns Cloud's tool registry into the grid the dashboard renders.
 *
 * The grid is data, never a hardcoded list: a new app appears by registering
 * with Cloud, and a second node's apps appear with no code change here.
 */

export type AppEntry = {
  id: string;
  name: string;
  description: string;
  url: string;
  health: "healthy" | "offline";
};

type RawTool = {
  id?: unknown;
  name?: unknown;
  description?: unknown;
  publicUrl?: unknown;
  health?: unknown;
};

function str(v: unknown): string {
  return typeof v === "string" ? v : "";
}

/**
 * `authHost` is excluded deliberately. Auth is where you sign in, not an app
 * you open, and a tile leading to the login page from inside the dashboard is
 * a dead end for someone already signed in.
 *
 * Tools without a publicUrl are dropped: most of the registry is empty
 * scaffolds, and a tile that cannot be clicked is worse than no tile.
 */
export function toAppEntries(payload: unknown, authHost: string): AppEntry[] {
  const tools = (payload as { tools?: unknown } | null)?.tools;
  if (!Array.isArray(tools)) return [];

  const entries: AppEntry[] = [];
  for (const raw of tools as RawTool[]) {
    if (!raw || typeof raw !== "object") continue;

    const url = str(raw.publicUrl);
    const id = str(raw.id);
    if (!url || !id) continue;
    if (url.includes(authHost)) continue;

    entries.push({
      id,
      name: str(raw.name) || id,
      description: str(raw.description),
      url,
      health: raw.health === "healthy" ? "healthy" : "offline",
    });
  }

  return entries.sort((a, b) => a.name.localeCompare(b.name));
}
```

- [ ] **Step 4: Run test and typecheck**

Run: `cd apps/Nexus-Dashboard && bun test tests/apps.test.ts && bun run check`
Expected: 6 tests PASS, tsc clean.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/src/apps.ts apps/Nexus-Dashboard/tests/apps.test.ts
git commit -m "feat(dashboard): build the app grid from Cloud's registry

Only tools with a public URL become tiles — 82 of the 85 registered tools are
empty scaffolds, and a tile that cannot be clicked is worse than no tile. The
auth host is excluded too: it is where you sign in, not an app you open.

Health is carried through so a down service shows as down rather than as a
dead link, and entries are sorted so the grid does not reshuffle between
polls. A malformed payload yields an empty grid rather than a broken page."
```

---

### Task 2: The dashboard server

Serves the SPA, proxies auth same-origin, exposes the grid.

**Files:**
- Rewrite: `apps/Nexus-Dashboard/src/server.ts`
- Rewrite: `apps/Nexus-Dashboard/tests/server.test.ts`

**Interfaces:**
- Consumes: `toAppEntries` (Task 1)
- Produces:
  - `export async function handleRequest(req: Request): Promise<Response>`
  - `export function startServer(): Server`
  - `GET /api/apps` → `{ apps: AppEntry[] }`
  - `ALL /api/v1/auth/*` → proxied to Auth, cookies preserved
  - everything else → SPA `index.html` (client-side routing)

- [ ] **Step 1: Write the failing test**

Replace `apps/Nexus-Dashboard/tests/server.test.ts`:

```ts
import { describe, it, expect, beforeEach, afterEach } from "bun:test";
import type { Server } from "bun";

process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_CLOUD_URL = "http://127.0.0.1:4398";
const { handleRequest } = await import("../src/server");

let auth: Server | null = null;
let cloud: Server | null = null;

beforeEach(() => {
  auth = Bun.serve({
    port: 4399,
    fetch(req) {
      const u = new URL(req.url);
      return Response.json(
        { seenPath: u.pathname, seenCookie: req.headers.get("cookie"), method: req.method },
        { headers: { "set-cookie": "nexus_session=abc; Path=/" } },
      );
    },
  });
  cloud = Bun.serve({
    port: 4398,
    fetch() {
      return Response.json({
        tools: [{ id: "nexus-chat", name: "Nexus Chat", publicUrl: "https://chat.tnhc.dev", health: "healthy" }],
      });
    },
  });
});

afterEach(() => { auth?.stop(true); cloud?.stop(true); });

describe("dashboard server", () => {
  it("serves the grid from Cloud", async () => {
    const res = await handleRequest(new Request("http://app.test/api/apps"));
    expect(res.status).toBe(200);
    const { apps } = await res.json() as { apps: Array<{ id: string }> };
    expect(apps.map((a) => a.id)).toEqual(["nexus-chat"]);
  });

  it("returns an empty grid rather than failing when Cloud is down", async () => {
    cloud?.stop(true); cloud = null;
    const res = await handleRequest(new Request("http://app.test/api/apps"));
    expect(res.status).toBe(200);
    expect((await res.json() as { apps: unknown[] }).apps).toEqual([]);
  });

  it("proxies auth calls same-origin, preserving path and method", async () => {
    const res = await handleRequest(new Request("http://app.test/api/v1/auth/access-requests", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ username: "x", email: "x@y.dev" }),
    }));
    const body = await res.json() as { seenPath: string; method: string };
    expect(body.seenPath).toBe("/api/v1/auth/access-requests");
    expect(body.method).toBe("POST");
  });

  it("forwards the session cookie to Auth", async () => {
    const res = await handleRequest(new Request("http://app.test/api/v1/auth/me", {
      headers: { cookie: "nexus_session=zzz" },
    }));
    expect((await res.json() as { seenCookie: string }).seenCookie).toContain("nexus_session=zzz");
  });

  it("passes Set-Cookie back so signing in actually creates a session", async () => {
    const res = await handleRequest(new Request("http://app.test/api/v1/auth/login", { method: "POST" }));
    expect(res.headers.get("set-cookie") ?? "").toContain("nexus_session=");
  });

  it("never proxies anything outside /api/v1/auth", async () => {
    // An open proxy would let the dashboard be used to reach arbitrary
    // internal services from the public internet.
    const res = await handleRequest(new Request("http://app.test/api/v1/admin/secrets"));
    expect(res.status).toBe(404);
  });

  it("serves the SPA shell for a client route", async () => {
    const res = await handleRequest(new Request("http://app.test/claim"));
    expect(res.status).toBe(200);
    expect(res.headers.get("content-type") ?? "").toContain("text/html");
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd apps/Nexus-Dashboard && bun test tests/server.test.ts`
Expected: FAIL — `handleRequest` not exported.

- [ ] **Step 3: Implement**

Replace `apps/Nexus-Dashboard/src/server.ts`:

```ts
import { join } from "node:path";
import { toAppEntries } from "./apps";

/**
 * The ecosystem front door.
 *
 * Serves the dashboard SPA and reverse-proxies the auth API onto the same
 * origin. Same-origin is the whole point: the session cookie is scoped to the
 * parent domain, but a credentialed cross-origin XHR from app.<domain> to
 * auth.<domain> needs CORS with explicit origins and Allow-Credentials, which
 * is easy to get subtly wrong. Proxying sidesteps it.
 */

const PORT = Number(process.env.PORT || "3132");
const DOMAIN = process.env.DOMAIN || "tnhc.dev";
const AUTH_HOST = process.env.NEXUS_AUTH_HOST || `auth.${DOMAIN}`;
const AUTH_INTERNAL_URL = process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310";
const CLOUD_URL = process.env.NEXUS_CLOUD_URL || "http://127.0.0.1:8787";
const CLOUD_API_KEY = process.env.NEXUS_CLOUD_API_KEY || "";
const WEB_ROOT = process.env.NEXUS_DASHBOARD_WEB_ROOT || join(import.meta.dir, "..", "frontend", "dist");

/** Only this prefix is proxied. Anything broader would be an open relay into
 *  the private network, reachable from the public internet. */
const AUTH_PREFIX = "/api/v1/auth/";

async function fetchApps(): Promise<Response> {
  try {
    const res = await fetch(`${CLOUD_URL.replace(/\/+$/, "")}/api/v1/tools`, {
      headers: CLOUD_API_KEY ? { "X-Api-Key": CLOUD_API_KEY } : {},
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) return Response.json({ apps: [] });
    return Response.json({ apps: toAppEntries(await res.json(), AUTH_HOST) });
  } catch {
    // Cloud being down must degrade to an empty grid, not a broken dashboard —
    // the account pages still work and the user can still sign in.
    return Response.json({ apps: [] });
  }
}

async function proxyToAuth(req: Request, path: string): Promise<Response> {
  const target = new URL(req.url);
  const upstream = new URL(AUTH_INTERNAL_URL);
  upstream.pathname = path;
  upstream.search = target.search;

  const headers = new Headers(req.headers);
  headers.delete("host");
  headers.delete("content-length");
  headers.delete("connection");

  const res = await fetch(upstream, {
    method: req.method,
    headers,
    body: req.method === "GET" || req.method === "HEAD" ? undefined : await req.arrayBuffer(),
    redirect: "manual",
  });

  // Pass the response through verbatim. Set-Cookie especially: dropping it
  // would make login appear to succeed while leaving the user signed out.
  return new Response(res.body, { status: res.status, headers: res.headers });
}

export async function handleRequest(req: Request): Promise<Response> {
  const url = new URL(req.url);
  const path = url.pathname;

  if (req.method === "GET" && path === "/health") {
    return Response.json({ service: "nexus-dashboard", status: "ok" });
  }

  if (req.method === "GET" && path === "/api/apps") return fetchApps();

  if (path.startsWith(AUTH_PREFIX)) return proxyToAuth(req, path);

  // Anything else under /api is not ours and must not fall through to the SPA,
  // or a typo'd API call would silently return HTML.
  if (path.startsWith("/api/")) {
    return Response.json({ error: "not_found" }, { status: 404 });
  }

  // Static asset, else the SPA shell so client-side routes work on hard reload.
  const asset = Bun.file(join(WEB_ROOT, path));
  if (path !== "/" && (await asset.exists())) return new Response(asset);
  return new Response(Bun.file(join(WEB_ROOT, "index.html")), {
    headers: { "content-type": "text/html; charset=utf-8" },
  });
}

export function startServer() {
  const server = Bun.serve({ port: PORT, fetch: handleRequest });
  console.log(`[nexus-dashboard] Listening on port ${server.port}`);
  return server;
}

if (import.meta.main) startServer();
```

Delete `src/dashboard-engine.ts`, `src/cloud.ts`, `src/contracts.ts` and `src/index.ts` if they are unreferenced ghost scaffolding — check with `grep -r` first and leave anything still imported.

- [ ] **Step 4: Run tests and typecheck**

Run: `cd apps/Nexus-Dashboard && bun test tests/ && bun run check`
Expected: 13 tests PASS (6 from Task 1 + 7 here), tsc clean.

The SPA test will fail until Task 3 creates `frontend/dist`; create a placeholder
`frontend/dist/index.html` containing `<!doctype html><title>Nexus</title>` so
the shell route is exercised now and replaced by the real build later.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/src apps/Nexus-Dashboard/tests apps/Nexus-Dashboard/frontend/dist
git commit -m "feat(dashboard): serve the SPA and proxy auth onto one origin

The session cookie is parent-domain scoped, but a credentialed cross-origin
call from app.<domain> to auth.<domain> still needs CORS with explicit origins
and Allow-Credentials — easy to get subtly wrong. Proxying /api/v1/auth/*
through this server makes every auth call same-origin and sidesteps it.

Only that one prefix is proxied; anything broader would be an open relay into
the private network from the public internet, and a test pins that shut.
Set-Cookie is passed back verbatim, without which login would appear to
succeed while leaving the user signed out. Cloud being unreachable degrades to
an empty grid rather than a broken dashboard."
```

---

### Task 3: SPA scaffold and the public pages

**Files:**
- Create: `apps/Nexus-Dashboard/frontend/` (package.json, vite.config.ts, tailwind, tsconfig, index.html)
- Create: `frontend/src/api.ts`, `frontend/src/App.tsx`, `frontend/src/main.tsx`
- Create: `frontend/src/pages/RequestAccess.tsx`, `frontend/src/pages/Claim.tsx`
- Test: `frontend/src/pages/RequestAccess.test.tsx`, `frontend/src/pages/Claim.test.tsx`

**Interfaces:**
- Produces (`api.ts`):
  - `requestAccess(input: { username: string; email: string; note?: string }): Promise<{ user: { id: string }; claimCode: string }>`
  - `claimAccount(input: { email: string; claimCode: string; password: string }): Promise<{ recoveryCodes: string[] }>`
  - `me(): Promise<{ user: { id: string; username: string; role: string } } | null>`
  - `listApps(): Promise<AppEntry[]>`

Mirror `apps/Nexus-Draw/frontend` exactly for config: Vite + React 18 +
Tailwind + vitest with jsdom and `globals: true`.

- [ ] **Step 1: Scaffold**

```bash
cd apps/Nexus-Dashboard && mkdir -p frontend/src/pages
cp ../Nexus-Draw/frontend/vite.config.ts ../Nexus-Draw/frontend/vitest.config.ts \
   ../Nexus-Draw/frontend/tsconfig.json ../Nexus-Draw/frontend/postcss.config.js frontend/ 2>/dev/null
```

Edit `frontend/vite.config.ts` so the dev proxy points at this app's server:

```ts
server: {
  port: 5175,
  proxy: { "/api": { target: "http://localhost:3132", changeOrigin: true } },
},
```

`frontend/package.json` mirrors Draw's scripts (`dev`, `build`, `preview`,
`check`, `test`) with dependencies `react`, `react-dom`, `react-router-dom` and
dev dependencies `@vitejs/plugin-react`, `@testing-library/react`, `jsdom`,
`tailwindcss`, `@tailwindcss/postcss`, `autoprefixer`, `vite`, `vitest`,
`typescript`, `@types/react`, `@types/react-dom`.

- [ ] **Step 2: Write the failing tests**

Create `frontend/src/pages/RequestAccess.test.tsx`:

```tsx
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import RequestAccess from "./RequestAccess";

beforeEach(() => { vi.restoreAllMocks(); });

describe("RequestAccess", () => {
  it("shows the claim code once, with a warning to save it", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(
      JSON.stringify({ user: { id: "usr-1", status: "pending" }, claimCode: "a".repeat(32) }),
      { status: 201, headers: { "content-type": "application/json" } },
    )));

    render(<RequestAccess />);
    fireEvent.change(screen.getByLabelText(/username/i), { target: { value: "sam" } });
    fireEvent.change(screen.getByLabelText(/email/i), { target: { value: "s@x.dev" } });
    fireEvent.click(screen.getByRole("button", { name: /request access/i }));

    await waitFor(() => expect(screen.getByText("a".repeat(32))).toBeTruthy());
    // The code cannot be retrieved again, so the UI must say so plainly.
    expect(screen.getByText(/only time|save it|cannot be shown again/i)).toBeTruthy();
  });

  it("surfaces a duplicate-username error instead of failing silently", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(
      JSON.stringify({ error: "Username 'sam' already exists" }),
      { status: 409, headers: { "content-type": "application/json" } },
    )));

    render(<RequestAccess />);
    fireEvent.change(screen.getByLabelText(/username/i), { target: { value: "sam" } });
    fireEvent.change(screen.getByLabelText(/email/i), { target: { value: "s@x.dev" } });
    fireEvent.click(screen.getByRole("button", { name: /request access/i }));

    await waitFor(() => expect(screen.getByText(/already exists/i)).toBeTruthy());
  });
});
```

Create `frontend/src/pages/Claim.test.tsx`:

```tsx
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import Claim from "./Claim";

beforeEach(() => { vi.restoreAllMocks(); });

const CODES = Array.from({ length: 10 }, (_, i) => String(i).repeat(32));

describe("Claim", () => {
  it("shows all ten recovery codes and blocks continuing until they are confirmed saved", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(
      JSON.stringify({ user: { id: "usr-1", status: "active" }, recoveryCodes: CODES }),
      { status: 200, headers: { "content-type": "application/json" } },
    )));

    render(<Claim />);
    fireEvent.change(screen.getByLabelText(/email/i), { target: { value: "s@x.dev" } });
    fireEvent.change(screen.getByLabelText(/claim code/i), { target: { value: "a".repeat(32) } });
    fireEvent.change(screen.getByLabelText(/password/i), { target: { value: "correct-horse-battery-staple" } });
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(screen.getByText(CODES[0]!)).toBeTruthy());
    for (const c of CODES) expect(screen.getByText(c)).toBeTruthy();

    // Losing these means losing the account permanently — the UI must not let
    // the user click past them by reflex.
    const cont = screen.getByRole("button", { name: /continue/i }) as HTMLButtonElement;
    expect(cont.disabled).toBe(true);
    fireEvent.click(screen.getByRole("checkbox"));
    expect(cont.disabled).toBe(false);
  });

  it("reports a rejected claim code", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(
      JSON.stringify({ error: "invalid_code" }),
      { status: 400, headers: { "content-type": "application/json" } },
    )));

    render(<Claim />);
    fireEvent.change(screen.getByLabelText(/email/i), { target: { value: "s@x.dev" } });
    fireEvent.change(screen.getByLabelText(/claim code/i), { target: { value: "0".repeat(32) } });
    fireEvent.change(screen.getByLabelText(/password/i), { target: { value: "correct-horse-battery-staple" } });
    fireEvent.click(screen.getByRole("button", { name: /claim/i }));

    await waitFor(() => expect(screen.getByText(/invalid|not recognised|incorrect/i)).toBeTruthy());
  });
});
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run`
Expected: FAIL — the page modules do not exist.

- [ ] **Step 4: Implement `api.ts` and the two pages**

`api.ts` calls same-origin paths only (`/api/v1/auth/...`, `/api/apps`), always
with `credentials: "same-origin"`, and throws an `Error` carrying the server's
`error` field so the pages can render it.

`RequestAccess.tsx`: labelled `username`, `email` and optional `note` inputs; a
**Request access** button; on success replaces the form with the claim code in a
`<code>` block plus copy button and the sentence *"This is the only time this
code is shown. Save it — you will need it to finish creating your account."*

`Claim.tsx`: labelled `email`, `claim code` and `password` inputs; a **Claim**
button; on success renders the ten codes, a checkbox *"I have saved these
codes"*, and a **Continue** button disabled until it is ticked, with the plain
warning that losing both password and codes means the account cannot be
recovered by anyone.

- [ ] **Step 5: Run tests and typecheck**

Run: `cd apps/Nexus-Dashboard/frontend && npx vitest run && npx tsc --noEmit`
Expected: 4 tests PASS, tsc clean.

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Dashboard/frontend
git commit -m "feat(dashboard): request-access and claim pages

Both codes are shown exactly once, where the API returns them, because neither
can be fetched again. The claim page gates Continue behind an explicit 'I have
saved these' checkbox: losing both password and recovery codes means the
account is gone and the operator cannot rescue it, so the UI must not let
someone click past that by reflex."
```

---

### Task 4: The app grid

**Files:**
- Create: `frontend/src/pages/Grid.tsx`
- Test: `frontend/src/pages/Grid.test.tsx`

- [ ] **Step 1: Write the failing test**

```tsx
import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import Grid from "./Grid";

beforeEach(() => { vi.restoreAllMocks(); });

const APPS = [
  { id: "nexus-chat", name: "Nexus Chat", description: "Chat", url: "https://chat.tnhc.dev", health: "healthy" },
  { id: "nexus-draw", name: "Nexus Draw", description: "Whiteboard", url: "https://draw.tnhc.dev", health: "offline" },
];

describe("Grid", () => {
  it("renders a tile per app, linking to its subdomain", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ apps: APPS }),
      { status: 200, headers: { "content-type": "application/json" } })));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText("Nexus Chat")).toBeTruthy());
    expect(screen.getByRole("link", { name: /nexus chat/i }).getAttribute("href"))
      .toBe("https://chat.tnhc.dev");
  });

  it("marks a down app as unavailable rather than offering a dead link", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ apps: APPS }),
      { status: 200, headers: { "content-type": "application/json" } })));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText("Nexus Draw")).toBeTruthy());
    expect(screen.getByText(/unavailable|offline/i)).toBeTruthy();
  });

  it("says so when no apps are reachable instead of rendering an empty page", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => new Response(JSON.stringify({ apps: [] }),
      { status: 200, headers: { "content-type": "application/json" } })));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText(/no apps|nothing available/i)).toBeTruthy());
  });
});
```

- [ ] **Step 2: Run → FAIL.** `cd apps/Nexus-Dashboard/frontend && npx vitest run src/pages/Grid.test.tsx`

- [ ] **Step 3: Implement** `Grid.tsx` — fetches `/api/apps` on mount, renders a
responsive tile grid; each healthy app is an `<a href={url}>` with its name and
description; an offline app renders the same tile visually muted, not a link,
labelled *Unavailable*; an empty list renders *No apps are reachable right now.*

- [ ] **Step 4: Run tests → PASS**, `npx tsc --noEmit` clean.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Dashboard/frontend/src/pages/Grid.tsx apps/Nexus-Dashboard/frontend/src/pages/Grid.test.tsx
git commit -m "feat(dashboard): app grid rendered from the registry

One tile per reachable app, linking to its subdomain — the parent-domain
session cookie means clicking through arrives already signed in. A down app is
shown muted and unlinked rather than as a dead link, and an empty registry says
so rather than rendering a blank page."
```

---

### Task 5: Account page

**Files:** `frontend/src/pages/Account.tsx` + test.

Covers: change password, view remaining recovery-code count, regenerate codes
(shown once, same confirmation gate as Task 3), list active sessions with a
revoke button.

- [ ] **Step 1: Failing tests** — regenerating shows ten new codes behind the
  save-confirmation gate; revoking a session removes it from the list; a failed
  password change surfaces the server's reason.
- [ ] **Step 2: Run → FAIL.**
- [ ] **Step 3: Implement** against `/api/v1/auth/me`, `/api/v1/auth/sessions`,
  and the recovery endpoints.
- [ ] **Step 4: Run → PASS**, tsc clean.
- [ ] **Step 5: Commit** — `feat(dashboard): account page`.

---

### Task 6: Admin panel

**Files:** `frontend/src/pages/Admin.tsx` + test.

Visible only when `me().user.role` is `founder` or `admin` — the same two roles
that hold `users:approve`.

- [ ] **Step 1: Failing tests** — the pending queue renders each request with its
  note; approve removes it from the queue; reject removes it; minting an invite
  shows the code once; a non-admin sees nothing (the panel is not rendered at
  all rather than rendered-and-erroring).
- [ ] **Step 2: Run → FAIL.**
- [ ] **Step 3: Implement** against `/api/v1/auth/access-requests`,
  `.../approve`, `.../reject`, `/api/v1/auth/invites`.
- [ ] **Step 4: Run → PASS**, tsc clean.
- [ ] **Step 5: Commit** — `feat(dashboard): operator admin panel`.

Note: hiding the panel is presentation only. The endpoints are already guarded
by `users:approve` server-side (Phase 1, Task 7), which is what actually
enforces this.

---

### Task 7: Deploy wiring

**Files:**
- Modify: `deploy/production/deploy.sh`
- Data: register the dashboard and Draw with Cloud

- [ ] **Step 1** Build the SPA: `cd apps/Nexus-Dashboard/frontend && npm install && npm run build`.

- [ ] **Step 2** Add to `deploy.sh` after the chat block, following the existing
  `start_service` shape and the setsid detachment already in place:

```bash
    # 4c. Nexus-Dashboard — app.$DOMAIN, the ecosystem front door.
    #
    # PUBLIC, deliberately: it hosts request-access and claim, which people who
    # are not signed in must be able to reach. The apps behind it are gated;
    # this is the door to them.
    start_service "dashboard" "$ROOT/apps/Nexus-Dashboard" 3132 \
        PORT=3132 DOMAIN="$DOMAIN" \
        NEXUS_AUTH_INTERNAL_URL=http://127.0.0.1:4310 \
        NEXUS_CLOUD_URL=http://localhost:8787 \
        NEXUS_CLOUD_API_KEY="$NEXUS_CLOUD_API_KEY" \
        bun run src/server.ts
```

  Add `dashboard` to the stop and status loops, and add
  `http://localhost:3132/health` to the health-check list.

- [ ] **Step 3** Register with Cloud so the route exists and the grid is honest:

```bash
K=$(sed -n 's/^NEXUS_CLOUD_API_KEY=//p' apps/Nexus-Cloud/.env | head -1 | tr -d '\r"')

# The dashboard itself.
curl -s -X POST -H "X-API-Key: $K" -H 'content-type: application/json' \
  -d '{"id":"nexus-dashboard","name":"Nexus Dashboard","description":"Ecosystem front door","upstreamUrl":"http://127.0.0.1:3132","publicUrl":"https://app.tnhc.dev","capabilities":["dashboard","account","admin"]}' \
  http://127.0.0.1:8787/api/v1/tools

curl -s -X POST -H "X-API-Key: $K" -H 'content-type: application/json' \
  -d '{"toolId":"nexus-dashboard","kind":"website","subject":"app"}' \
  http://127.0.0.1:8787/api/v1/addresses

# Draw is a Hosting-deployed site, so it has no tool record and is missing from
# the grid. Register it with its public URL so it appears like every other app.
curl -s -X POST -H "X-API-Key: $K" -H 'content-type: application/json' \
  -d '{"id":"nexus-draw","name":"Nexus Draw","description":"Whiteboard and diagramming","upstreamUrl":"http://127.0.0.1:8090","publicUrl":"https://draw.tnhc.dev","capabilities":["whiteboard","diagramming"]}' \
  http://127.0.0.1:8787/api/v1/tools
```

- [ ] **Step 4** Restart and verify at the edge:

```bash
./deploy/production/deploy.sh bg
curl -s -o /dev/null -w '%{http_code}\n' https://app.tnhc.dev/
curl -s https://app.tnhc.dev/api/apps | head -c 300
```

Expected: 200, and a grid containing chat, cloud and draw but **not** auth.

- [ ] **Step 5** Commit — `feat(deploy): serve the dashboard at app.tnhc.dev`.

---

## Definition of done

- `apps/Nexus-Dashboard`: `bun test tests/` green, `bun run check` clean.
- `apps/Nexus-Dashboard/frontend`: `npx vitest run` green, `tsc --noEmit` clean.
- `https://app.tnhc.dev/` serves the SPA; `/api/apps` returns the live grid.
- A full manual pass: request access → approve in the admin panel → claim →
  land on the grid → click an app and arrive signed in.
- Every other host still 200s.

## Deliberately not in this plan

- Gating any app (`requiresAuth` stays false everywhere) — Phase 4, after review.
- nexus-chat consuming the identity token, port rebinding, JWT secret rotation — Phase 4.
- TOTP enrolment on the account page — its backup codes reuse the recovery table, but enrolment is later.
- The bug-reporting system. It wants this dashboard's admin surface, so it is a follow-on project with its own spec.
