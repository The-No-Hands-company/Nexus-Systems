# SSO Phase 2 — The Gate Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make one login open every app — the ecosystem proxy refuses unauthenticated requests to gated hosts, and hands authenticated ones a short-lived signed identity that apps can verify locally.

**Architecture:** Nexus-Auth gains one endpoint that exchanges a session for an audience-scoped identity JWT, signed by the existing RS256 key and verifiable against the JWKS it already publishes. `deploy/production/proxy.ts` gains a gate in front of its existing route lookup: read the session cookie, exchange it (cached), attach the token, forward. Whether a host is gated is a field on the Cloud route record, so policy is data.

**Tech Stack:** Bun, TypeScript, `bun test`, RS256 via `node:crypto` (already in `apps/Nexus-Auth/src/token.ts`).

**Spec:** `docs/superpowers/specs/2026-08-11-single-sign-on-and-front-door-design.md`
**Depends on:** Phase 1 (`2026-08-11-sso-phase-1-identity.md`), merged.

## Global Constraints

- **Runner is `bun test`.** Import from `bun:test`, never `vitest`.
- **Nothing becomes publicly reachable in this phase.** The gate is built and tested but the proxy is not restarted against production until Phase 4's security review. Do not run `deploy.sh` to activate it.
- **`auth.tnhc.dev` is allowlisted in code, never by policy data.** A single bad row must not be able to make logging in require being logged in.
- **`redirect_uri` is validated against the configured domain suffix** before it is ever emitted in a `Location` header.
- **Never widen an existing route's behaviour by default.** A route with no explicit policy is treated as **public**, exactly as today, so this change cannot lock out an app by omission. Gating is opt-in per route.
- Type check with `bunx tsc --noEmit` before every commit.

---

## File Structure

| File | Responsibility |
|---|---|
| `apps/Nexus-Auth/src/identity.ts` (create) | Build identity-token claims; one pure function |
| `apps/Nexus-Auth/src/server.ts` (modify) | `POST /api/v1/auth/identity-token` |
| `apps/Nexus-Auth/tests/identity.test.ts` (create) | Claims + endpoint tests |
| `deploy/production/package.json` (create) | Makes the proxy a testable unit |
| `deploy/production/bunfig.toml` (create) | Test preload |
| `deploy/production/proxy.ts` (modify) | Export `handleRequest`; stop serving on import |
| `deploy/production/gate.ts` (create) | Session→identity exchange, two-age cache, gate decision |
| `deploy/production/tests/gate.test.ts` (create) | The security boundary — exhaustive |

`gate.ts` is separate from `proxy.ts` because the gate is the security boundary and deserves to be readable and testable without the routing, retry and streaming machinery around it. `proxy.ts` is already 306 lines doing several jobs.

---

### Task 1: Identity token claims

A pure function producing the claim set, so the shape is testable without HTTP or crypto.

**Files:**
- Create: `apps/Nexus-Auth/src/identity.ts`
- Test: `apps/Nexus-Auth/tests/identity.test.ts`

**Interfaces:**
- Consumes: `SafeUser` (Phase 1, `src/users.ts`)
- Produces:
  - `const IDENTITY_TOKEN_TTL_SECONDS = 120`
  - `buildIdentityClaims(user: SafeUser, audience: string, now?: number): IdentityClaims`
  - `type IdentityClaims = { iss: "nexus-auth"; sub: string; aud: string; email: string; username: string; role: string; typ: "identity"; iat: number; exp: number; jti: string }`

- [ ] **Step 1: Write the failing test**

Create `apps/Nexus-Auth/tests/identity.test.ts`:

```ts
import { describe, it, expect } from "bun:test";
import { buildIdentityClaims, IDENTITY_TOKEN_TTL_SECONDS } from "../src/identity";
import * as users from "../src/users";

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret

describe("identity claims", () => {
  it("carries who the user is and which app the token is for", () => {
    users.clearUsers();
    const u = users.createUser({ username: "ada", email: "ada@x.dev", password: PASSWORD });
    const c = buildIdentityClaims(u, "chat.tnhc.dev", 1_000_000);

    expect(c.iss).toBe("nexus-auth");
    expect(c.sub).toBe(u.id);
    expect(c.aud).toBe("chat.tnhc.dev");
    expect(c.email).toBe("ada@x.dev");
    expect(c.username).toBe("ada");
    expect(c.role).toBe("user");
    expect(c.typ).toBe("identity");
  });

  it("expires in two minutes", () => {
    users.clearUsers();
    const u = users.createUser({ username: "bo", email: "bo@x.dev", password: PASSWORD });
    const c = buildIdentityClaims(u, "chat.tnhc.dev", 1_000_000);
    expect(c.iat).toBe(1_000_000);
    expect(c.exp).toBe(1_000_000 + IDENTITY_TOKEN_TTL_SECONDS);
    expect(IDENTITY_TOKEN_TTL_SECONDS).toBe(120);
  });

  it("gives every token a distinct id", () => {
    users.clearUsers();
    const u = users.createUser({ username: "cy", email: "cy@x.dev", password: PASSWORD });
    const a = buildIdentityClaims(u, "chat.tnhc.dev");
    const b = buildIdentityClaims(u, "chat.tnhc.dev");
    expect(a.jti).not.toBe(b.jti);
  });

  it("never leaks a password hash into the claims", () => {
    users.clearUsers();
    const u = users.createUser({ username: "di", email: "di@x.dev", password: PASSWORD });
    const c = buildIdentityClaims(u, "chat.tnhc.dev") as Record<string, unknown>;
    expect(JSON.stringify(c)).not.toContain("passwordHash");
    expect(c.passwordHash).toBeUndefined();
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd apps/Nexus-Auth && bun test tests/identity.test.ts`
Expected: FAIL — cannot resolve `../src/identity`.

- [ ] **Step 3: Implement**

Create `apps/Nexus-Auth/src/identity.ts`:

```ts
import { randomUUID } from "node:crypto";
import type { SafeUser } from "./users";

/**
 * How long an identity token is good for.
 *
 * Short on purpose. The token is minted per request by the proxy and carried
 * one hop to the app, so it never needs to outlive that hop — and a leaked one
 * is useless almost immediately. It is deliberately longer than the proxy's 60s
 * session-cache window so a cached token cannot expire before the cache entry
 * that holds it.
 */
export const IDENTITY_TOKEN_TTL_SECONDS = 120;

export type IdentityClaims = {
  iss: "nexus-auth";
  sub: string;
  aud: string;
  email: string;
  username: string;
  role: string;
  /** Distinguishes these from the service tokens issueServiceToken mints. */
  typ: "identity";
  iat: number;
  exp: number;
  jti: string;
};

/**
 * The claim set an app receives about the signed-in user.
 *
 * `aud` is the host the token was minted for, and apps must check it: without
 * that check a token minted for one app could be replayed against another.
 */
export function buildIdentityClaims(
  user: SafeUser,
  audience: string,
  now = Math.floor(Date.now() / 1000),
): IdentityClaims {
  return {
    iss: "nexus-auth",
    sub: user.id,
    aud: audience,
    email: user.email,
    username: user.username,
    role: user.role,
    typ: "identity",
    iat: now,
    exp: now + IDENTITY_TOKEN_TTL_SECONDS,
    jti: randomUUID(),
  };
}
```

- [ ] **Step 4: Run test and typecheck**

Run: `cd apps/Nexus-Auth && bun test tests/identity.test.ts && bun run check`
Expected: 4 tests PASS, tsc clean.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Auth/src/identity.ts apps/Nexus-Auth/tests/identity.test.ts
git commit -m "feat(auth): identity token claim set

The claims an app receives about the signed-in user. Separated from signing
so the shape is testable without crypto. Two-minute TTL: the proxy mints one
per request and it travels a single hop, so it never needs to live longer,
and it outlasts the proxy's 60s session cache by design."
```

---

### Task 2: The identity-token endpoint

**Files:**
- Modify: `apps/Nexus-Auth/src/server.ts`
- Test: `apps/Nexus-Auth/tests/identity.test.ts` (extend)

**Interfaces:**
- Consumes: `buildIdentityClaims` (Task 1), `signJwt` (`src/token.ts`), `requireAuth`, `getUser`
- Produces: `POST /api/v1/auth/identity-token` — body `{ audience: string }`, session-authenticated, returns `{ token, expiresIn }`

- [ ] **Step 1: Write the failing test**

Append to `apps/Nexus-Auth/tests/identity.test.ts`:

```ts
import { handleRequest } from "../src/server";
import { validateServiceToken } from "../src/token";

const BASE = "http://auth.test";

async function sessionToken(username: string): Promise<string> {
  users.createUser({ username, email: `${username}@x.dev`, password: PASSWORD });
  const res = await handleRequest(new Request(`${BASE}/api/v1/auth/login`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ username, password: PASSWORD }),
  }));
  return (await res.json() as { token: string }).token;
}

function mint(audience: unknown, headers: Record<string, string> = {}) {
  return handleRequest(new Request(`${BASE}/api/v1/auth/identity-token`, {
    method: "POST",
    headers: { "content-type": "application/json", ...headers },
    body: JSON.stringify({ audience }),
  }));
}

describe("identity-token endpoint", () => {
  it("refuses an unauthenticated caller", async () => {
    users.clearUsers();
    expect((await mint("chat.tnhc.dev")).status).toBe(401);
  });

  it("mints a token a JWKS consumer can verify, scoped to the audience", async () => {
    users.clearUsers();
    const session = await sessionToken("erin");
    const res = await mint("chat.tnhc.dev", { authorization: `Bearer ${session}` });
    expect(res.status).toBe(200);

    const { token } = await res.json() as { token: string };
    const ok = validateServiceToken(token, "chat.tnhc.dev");
    expect(ok.valid).toBe(true);
    if (ok.valid) expect(ok.payload.sub).toBe(users.findUserByUsername("erin")!.id);
  });

  it("rejects the token against a different audience", async () => {
    users.clearUsers();
    const session = await sessionToken("finn");
    const { token } = await (await mint("chat.tnhc.dev", {
      authorization: `Bearer ${session}`,
    })).json() as { token: string };

    // This is the replay case: a token minted for chat must not satisfy draw.
    expect(validateServiceToken(token, "draw.tnhc.dev").valid).toBe(false);
  });

  it("requires an audience", async () => {
    users.clearUsers();
    const session = await sessionToken("gus");
    expect((await mint("", { authorization: `Bearer ${session}` })).status).toBe(400);
    expect((await mint(42, { authorization: `Bearer ${session}` })).status).toBe(400);
  });

  it("refuses a suspended account", async () => {
    users.clearUsers();
    const session = await sessionToken("hal");
    users.setUserStatus(users.findUserByUsername("hal")!.id, "suspended");
    expect((await mint("chat.tnhc.dev", { authorization: `Bearer ${session}` })).status).toBe(403);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd apps/Nexus-Auth && bun test tests/identity.test.ts`
Expected: the five new tests FAIL with 404 (route not mounted).

- [ ] **Step 3: Implement**

In `apps/Nexus-Auth/src/server.ts`, add to the imports:

```ts
import { buildIdentityClaims, IDENTITY_TOKEN_TTL_SECONDS } from "./identity";
```

and add `signJwt` to the existing `./token` import.

Insert this route inside `handleRequest`, next to the other `/api/v1/auth/...` routes:

```ts
    // ── Session -> short-lived, audience-scoped identity token ──
    //
    // The proxy calls this with the caller's session cookie and the host the
    // request is bound for. Apps verify the result against the JWKS this
    // service already publishes, so they learn who the user is without
    // calling back here.
    if (request.method === "POST" && path === "/api/v1/auth/identity-token") {
      if (!auth) return jsonResponse({ error: "unauthenticated" }, { status: 401 });

      const body = await parseBody(request);
      const audience = typeof body.audience === "string" ? body.audience.trim() : "";
      if (!audience) {
        return jsonResponse({ error: "audience is required" }, { status: 400 });
      }

      const user = getUser(auth.userId);
      // A session can outlive the account's right to use it — suspension takes
      // effect here rather than only at next login.
      if (!user || user.status !== "active") {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }

      return jsonResponse({
        token: signJwt(buildIdentityClaims(user, audience) as unknown as Record<string, unknown>),
        expiresIn: IDENTITY_TOKEN_TTL_SECONDS,
      });
    }
```

- [ ] **Step 4: Run the whole Auth suite and typecheck**

Run: `cd apps/Nexus-Auth && bun test && bun run check`
Expected: all tests PASS (Phase 1's 47 plus the new ones), tsc clean.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Auth/src/server.ts apps/Nexus-Auth/tests/identity.test.ts
git commit -m "feat(auth): exchange a session for an audience-scoped identity token

The proxy calls this with the caller's session and the target host; apps
verify the result against the JWKS already published, so they identify the
user without calling back. Signed by the same RS256 key and kid as every
other token this service issues.

Suspension is enforced here, not just at login: a live session for a
suspended account gets 403 rather than a fresh identity."
```

---

### Task 3: Make the proxy testable

Pure scaffolding, no behaviour change — but the gate cannot be tested without it. `proxy.ts` calls `Bun.serve` at module scope, so importing it today binds port 8080 and collides with production.

**Files:**
- Create: `deploy/production/package.json`
- Create: `deploy/production/bunfig.toml`
- Create: `deploy/production/tests/setup.ts`
- Modify: `deploy/production/proxy.ts`
- Test: `deploy/production/tests/proxy.test.ts`

**Interfaces:**
- Produces: `export async function handleRequest(req: Request): Promise<Response>`; `export function startProxy(): void`; module import no longer listens

- [ ] **Step 1: Write the failing test**

Create `deploy/production/tests/proxy.test.ts`:

```ts
import { describe, it, expect } from "bun:test";

describe("proxy module", () => {
  it("can be imported without binding a port", async () => {
    // If proxy.ts still calls Bun.serve at module scope this either throws
    // EADDRINUSE against the running production proxy or, worse, silently
    // steals port 8080 from it.
    const mod = await import("../proxy");
    expect(typeof mod.handleRequest).toBe("function");
    expect(typeof mod.startProxy).toBe("function");
  });

  it("404s an unknown host", async () => {
    const { handleRequest } = await import("../proxy");
    const res = await handleRequest(new Request("http://nope.example.com/"));
    expect(res.status).toBe(404);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd deploy/production && bun test tests/proxy.test.ts`
Expected: FAIL — no `startProxy` export (and likely a port bind attempt).

- [ ] **Step 3: Add the package scaffolding**

Create `deploy/production/package.json`:

```json
{
  "name": "nexus-proxy",
  "private": true,
  "type": "module",
  "scripts": {
    "start": "bun run proxy.ts",
    "check": "bunx tsc --noEmit",
    "test": "bun test tests/"
  },
  "devDependencies": {
    "bun-types": "^1.3.11",
    "typescript": "^5.8.3"
  }
}
```

Create `deploy/production/bunfig.toml`:

```toml
[test]
preload = ["./tests/setup.ts"]
```

Create `deploy/production/tests/setup.ts`:

```ts
/**
 * Keeps the test run off the network and off production's ports.
 *
 * CLOUD_URL points at a port nothing listens on, so getRoutes() fails its
 * fetch and falls back to its cache exactly as it does when Cloud is down —
 * tests then seed the cache explicitly rather than depending on a live Cloud.
 */
process.env.CLOUD_URL = "http://127.0.0.1:1";
process.env.PROXY_PORT = "0";
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:1";
```

- [ ] **Step 4: Stop serving on import**

At the bottom of `deploy/production/proxy.ts`, replace:

```ts
const server = Bun.serve({ port: PORT, fetch: handleRequest });
```

with:

```ts
/**
 * Starts listening. Called from the entrypoint below rather than at module
 * scope so tests (and anything else) can import handleRequest without binding
 * the port — importing this file used to start a second proxy on 8080 and
 * fight the running one for it.
 */
export function startProxy() {
  const server = Bun.serve({ port: PORT, fetch: handleRequest });
  console.log(`[proxy] Listening on port ${server.port}`);
  return server;
}

// Only run when executed directly (`bun run proxy.ts`), not when imported.
if (import.meta.main) {
  startProxy();
}
```

Keep every `console.log` that already followed the old `Bun.serve` call — move them inside `startProxy` so the startup banner still prints when run for real. Add `export` to `async function handleRequest`.

- [ ] **Step 5: Run tests and typecheck**

Run: `cd deploy/production && bun test tests/ && bun run check`
Expected: 2 tests PASS, tsc clean.

- [ ] **Step 6: Verify the real proxy still starts**

The production proxy is running on 8080. Prove the entrypoint still works without disturbing it:

```bash
cd deploy/production && PROXY_PORT=8099 CLOUD_URL=http://127.0.0.1:8787 timeout 5 bun run proxy.ts
```

Expected: prints `[proxy] Listening on port 8099` and exits after 5s. **Do not** run it on 8080.

- [ ] **Step 7: Commit**

```bash
git add deploy/production/package.json deploy/production/bunfig.toml \
        deploy/production/tests/ deploy/production/proxy.ts
git commit -m "refactor(proxy): export handleRequest and only listen when run directly

Bun.serve ran at module scope, so importing proxy.ts bound port 8080 and
fought the running production proxy for it — which made the routing layer
untestable. Listening now happens in startProxy(), called under
import.meta.main. No behaviour change when run as an entrypoint."
```

---

### Task 4: Route policy — which hosts are gated

**Files:**
- Modify: `deploy/production/proxy.ts`
- Test: `deploy/production/tests/proxy.test.ts` (extend)

**Interfaces:**
- Consumes: `NexusCloudRoute` (proxy.ts)
- Produces:
  - `interface RouteTarget { upstream: string; requiresAuth: boolean }`
  - route cache becomes `Record<string, RouteTarget>`
  - `export function __setRoutesForTest(routes: Record<string, RouteTarget>): void`

- [ ] **Step 1: Write the failing test**

Append to `deploy/production/tests/proxy.test.ts`:

```ts
describe("route policy", () => {
  it("defaults to public when Cloud says nothing about auth", async () => {
    const { buildRouteMap } = await import("../proxy");
    const map = buildRouteMap([{ domain: "draw.tnhc.dev", upstream: "http://127.0.0.1:9" }] as never);
    expect(map["draw.tnhc.dev"]).toEqual({ upstream: "http://127.0.0.1:9", requiresAuth: false });
  });

  it("honours requiresAuth when Cloud sets it", async () => {
    const { buildRouteMap } = await import("../proxy");
    const map = buildRouteMap([
      { domain: "chat.tnhc.dev", upstream: "http://127.0.0.1:9", requiresAuth: true },
    ] as never);
    expect(map["chat.tnhc.dev"]!.requiresAuth).toBe(true);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd deploy/production && bun test tests/proxy.test.ts`
Expected: FAIL — `buildRouteMap` is not exported / returns strings.

- [ ] **Step 3: Implement**

In `deploy/production/proxy.ts`, extend the route interface:

```ts
// Route structure matching Nexus-Cloud's /api/v1/routes response
interface NexusCloudRoute {
  domain: string;
  upstream: string;
  /**
   * Whether this host requires a signed-in user. Absent means public.
   *
   * Defaulting to public is deliberate: a route Cloud has not been taught
   * about must keep behaving exactly as it does today. Gating is opt-in, so a
   * missing field can never lock an app's users out.
   */
  requiresAuth?: boolean;
}

export interface RouteTarget {
  upstream: string;
  requiresAuth: boolean;
}
```

Change the cache and `buildRouteMap`:

```ts
let routeCache: { timestamp: number; routes: Record<string, RouteTarget> } = {
  timestamp: 0,
  routes: {},
};

export function buildRouteMap(routes: NexusCloudRoute[]): Record<string, RouteTarget> {
  const map: Record<string, RouteTarget> = {};
  for (const route of routes) {
    if (route.domain && route.upstream) {
      map[route.domain.toLowerCase()] = {
        upstream: normalizeUpstream(route.upstream),
        requiresAuth: route.requiresAuth === true,
      };
    }
  }
  return map;
}
```

Update `getRoutes()`'s return type to `Promise<Record<string, RouteTarget>>`, and update every consumer in `handleRequest` that treated the looked-up value as a string — it is now `target.upstream`.

Add the test seam next to `buildRouteMap`:

```ts
/** Seeds the route cache directly. Tests only — there is no live Cloud there. */
export function __setRoutesForTest(routes: Record<string, RouteTarget>): void {
  routeCache = { timestamp: Date.now(), routes };
}
```

- [ ] **Step 4: Run tests and typecheck**

Run: `cd deploy/production && bun test tests/ && bun run check`
Expected: 4 tests PASS, tsc clean.

- [ ] **Step 5: Commit**

```bash
git add deploy/production/proxy.ts deploy/production/tests/proxy.test.ts
git commit -m "feat(proxy): carry a requiresAuth policy on each route

The route map becomes domain -> {upstream, requiresAuth} so the gate can ask
whether a host needs a session. Absent means public, so every route Cloud has
not been taught about keeps behaving exactly as it does now — gating is
opt-in and a missing field cannot lock anyone out."
```

---

### Task 5: The gate

The security boundary. Exhaustively tested.

**Files:**
- Create: `deploy/production/gate.ts`
- Modify: `deploy/production/proxy.ts`
- Test: `deploy/production/tests/gate.test.ts`

**Interfaces:**
- Consumes: `RouteTarget` (Task 4); Auth's `POST /api/v1/auth/identity-token` (Task 2)
- Produces:
  - `const AUTH_HOST: string` — allowlisted, never gated
  - `isRedirectAllowed(target: string, domain: string): boolean`
  - `loginRedirect(originalUrl: string): Response`
  - `resolveIdentity(cookie: string | null, audience: string): Promise<string | null>`
  - `gate(req: Request, target: RouteTarget): Promise<{ allow: true; identityToken: string | null } | { allow: false; response: Response }>`
  - `__resetGateForTest(): void`

- [ ] **Step 1: Write the failing test**

Create `deploy/production/tests/gate.test.ts`:

```ts
import { describe, it, expect, beforeEach } from "bun:test";
import { isRedirectAllowed, loginRedirect, gate, AUTH_HOST, __resetGateForTest } from "../gate";

const PUBLIC = { upstream: "http://127.0.0.1:9", requiresAuth: false };
const GATED = { upstream: "http://127.0.0.1:9", requiresAuth: true };

function req(url: string, headers: Record<string, string> = {}) {
  return new Request(url, { headers });
}

describe("redirect validation", () => {
  it("allows a return to any host on the configured domain", () => {
    expect(isRedirectAllowed("https://chat.tnhc.dev/rooms/1", "tnhc.dev")).toBe(true);
    expect(isRedirectAllowed("https://tnhc.dev/", "tnhc.dev")).toBe(true);
  });

  it("refuses anything off-domain — this is the phishing case", () => {
    for (const bad of [
      "https://evil.example.com/",
      "https://tnhc.dev.evil.example.com/",   // suffix trick
      "https://eviltnhc.dev/",                // no dot boundary
      "//evil.example.com",                   // protocol-relative
      "http://chat.tnhc.dev/",                // downgrade to plaintext
      "javascript:alert(1)",
      "not a url",
    ]) {
      expect(isRedirectAllowed(bad, "tnhc.dev")).toBe(false);
    }
  });

  it("never emits an unvalidated Location", () => {
    const res = loginRedirect("https://evil.example.com/steal");
    expect(res.headers.get("location") ?? "").not.toContain("evil.example.com");
  });
});

describe("the gate", () => {
  beforeEach(() => __resetGateForTest());

  it("lets a public route through untouched", async () => {
    const r = await gate(req("https://draw.tnhc.dev/"), PUBLIC);
    expect(r.allow).toBe(true);
    if (r.allow) expect(r.identityToken).toBeNull();
  });

  it("redirects a gated route with no cookie to the login page", async () => {
    const r = await gate(req("https://chat.tnhc.dev/rooms/1"), GATED);
    expect(r.allow).toBe(false);
    if (!r.allow) {
      expect(r.response.status).toBe(302);
      const loc = r.response.headers.get("location")!;
      expect(loc).toContain(AUTH_HOST);
      expect(loc).toContain(encodeURIComponent("https://chat.tnhc.dev/rooms/1"));
    }
  });

  it("redirects a gated route whose session Auth rejects", async () => {
    // The preload points Auth at a dead port, so the exchange fails.
    const r = await gate(req("https://chat.tnhc.dev/", { cookie: "nexus_session=bogus" }), GATED);
    expect(r.allow).toBe(false);
  });

  it("never gates the auth host, whatever the policy says", async () => {
    const r = await gate(req(`https://${AUTH_HOST}/login`), GATED);
    expect(r.allow).toBe(true);
  });

  it("never gates the auth host's API either", async () => {
    const r = await gate(req(`https://${AUTH_HOST}/api/v1/auth/login`), GATED);
    expect(r.allow).toBe(true);
  });
});
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cd deploy/production && bun test tests/gate.test.ts`
Expected: FAIL — cannot resolve `../gate`.

- [ ] **Step 3: Implement**

Create `deploy/production/gate.ts`:

```ts
import type { RouteTarget } from "./proxy";

/**
 * The login gate.
 *
 * Every hostname reaches the ecosystem through this proxy, so this is the one
 * place a "you must be signed in" rule can be enforced without every app
 * implementing login. Apps stay unaware of sessions; they receive a signed
 * identity and verify it against Auth's JWKS.
 */

const DOMAIN = process.env.DOMAIN || "tnhc.dev";

/**
 * The identity provider's host. Allowlisted in code, never by policy data: if
 * a bad route row ever marked this host as gated, signing in would require
 * already being signed in and nobody could recover.
 */
export const AUTH_HOST = process.env.NEXUS_AUTH_HOST || `auth.${DOMAIN}`;

/** Where the proxy reaches Auth internally. Never a public URL — that would
 *  send the request back out through Cloudflare and into this proxy again. */
const AUTH_INTERNAL_URL = process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310";

const SESSION_COOKIE = "nexus_session";

/** Normal freshness. Beyond this an entry is revalidated against Auth. */
const FRESH_MS = 60_000;
/** How long a stale entry may still be served while Auth is unreachable. */
const STALE_MS = 15 * 60_000;

type CacheEntry = { token: string; fetchedAt: number };
const identityCache = new Map<string, CacheEntry>();

export function __resetGateForTest(): void {
  identityCache.clear();
}

function readCookie(req: Request, name: string): string | null {
  const header = req.headers.get("cookie");
  if (!header) return null;
  for (const part of header.split(";")) {
    const eq = part.indexOf("=");
    if (eq === -1) continue;
    if (part.slice(0, eq).trim() === name) return decodeURIComponent(part.slice(eq + 1).trim());
  }
  return null;
}

/**
 * True only for https URLs on the configured domain.
 *
 * An unvalidated redirect target turns the login page into a phishing tool:
 * the victim really does authenticate, then gets forwarded to the attacker.
 * Host equality or a dot-boundary suffix — never a bare `endsWith`, which
 * would accept `eviltnhc.dev` and `tnhc.dev.evil.com`.
 */
export function isRedirectAllowed(target: string, domain = DOMAIN): boolean {
  let url: URL;
  try {
    url = new URL(target);
  } catch {
    return false;
  }
  if (url.protocol !== "https:") return false;
  const host = url.hostname.toLowerCase();
  const d = domain.toLowerCase();
  return host === d || host.endsWith(`.${d}`);
}

/** 302 to the login page, carrying a validated return address. */
export function loginRedirect(originalUrl: string): Response {
  const safeReturn = isRedirectAllowed(originalUrl) ? originalUrl : `https://${DOMAIN}/`;
  const location = `https://${AUTH_HOST}/login?redirect_uri=${encodeURIComponent(safeReturn)}`;
  return new Response(null, { status: 302, headers: { location } });
}

/**
 * Exchanges a session cookie for an identity token scoped to `audience`.
 *
 * Cached per (cookie, audience) so a browsing session costs at most one call
 * to Auth per minute rather than one per request. When Auth is unreachable a
 * stale entry is served for up to STALE_MS: a restart of Auth should not sign
 * everyone out mid-session. Nobody new gets in either way, since a cache miss
 * with Auth down simply fails.
 */
export async function resolveIdentity(cookie: string | null, audience: string): Promise<string | null> {
  if (!cookie) return null;

  const key = `${audience} ${cookie}`;
  const hit = identityCache.get(key);
  const age = hit ? Date.now() - hit.fetchedAt : Infinity;
  if (hit && age < FRESH_MS) return hit.token;

  try {
    const res = await fetch(`${AUTH_INTERNAL_URL}/api/v1/auth/identity-token`, {
      method: "POST",
      headers: { "content-type": "application/json", authorization: `Bearer ${cookie}` },
      body: JSON.stringify({ audience }),
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) {
      // A definitive "no" from Auth — drop any cached yes so a revoked or
      // suspended session stops working rather than lingering until STALE_MS.
      identityCache.delete(key);
      return null;
    }
    const { token } = await res.json() as { token: string };
    identityCache.set(key, { token, fetchedAt: Date.now() });
    return token;
  } catch {
    // Auth unreachable — distinct from Auth saying no.
    if (hit && age < STALE_MS) return hit.token;
    return null;
  }
}

/** Decides whether a request may proceed, and with what identity. */
export async function gate(
  req: Request,
  target: RouteTarget,
): Promise<{ allow: true; identityToken: string | null } | { allow: false; response: Response }> {
  const url = new URL(req.url);
  const host = url.hostname.toLowerCase();

  // Structural allowlist — checked before policy, so no route row can gate it.
  if (host === AUTH_HOST) return { allow: true, identityToken: null };

  if (!target.requiresAuth) return { allow: true, identityToken: null };

  const identityToken = await resolveIdentity(readCookie(req, SESSION_COOKIE), host);
  if (!identityToken) return { allow: false, response: loginRedirect(req.url) };

  return { allow: true, identityToken };
}
```

- [ ] **Step 4: Wire it into the proxy**

In `deploy/production/proxy.ts`, import the gate:

```ts
import { gate } from "./gate";
```

In `handleRequest`, immediately after the route target is resolved and before the upstream request is built, add:

```ts
  // Login gate. Runs before anything is forwarded, so an app can never see a
  // request from someone the ecosystem has not authenticated.
  const decision = await gate(req, target);
  if (!decision.allow) return decision.response;
```

Then, where the outgoing headers are assembled for the upstream fetch, attach the identity:

```ts
  if (decision.identityToken) {
    headers.set("x-nexus-identity", decision.identityToken);
  } else {
    // Never forward a client-supplied one — otherwise anybody could claim to
    // be anyone by setting the header themselves.
    headers.delete("x-nexus-identity");
  }
```

The `headers.delete` is not optional: without it the header is attacker-controlled on every public route.

- [ ] **Step 5: Run everything and typecheck**

Run: `cd deploy/production && bun test tests/ && bun run check`
Expected: all tests PASS, tsc clean.

Then confirm Auth is still green: `cd apps/Nexus-Auth && bun test`

- [ ] **Step 6: Commit**

```bash
git add deploy/production/gate.ts deploy/production/proxy.ts deploy/production/tests/gate.test.ts
git commit -m "feat(proxy): login gate with audience-scoped identity forwarding

Gated hosts redirect to the login page when there is no valid session, and
forward a short-lived signed identity when there is. Apps verify it against
Auth's JWKS, so they never call back here.

Three things the tests pin down. The auth host is allowlisted structurally,
before policy is consulted, so no route row can make signing in require being
signed in. redirect_uri is host-matched on a dot boundary, so eviltnhc.dev
and tnhc.dev.evil.com are refused along with plaintext downgrades. And any
client-supplied x-nexus-identity is stripped before forwarding — without that
the header is attacker-controlled on every public route.

Sessions are cached per (cookie, audience) for 60s, and served stale for up
to 15 minutes only while Auth is unreachable, so an Auth restart does not
sign everyone out. A definitive rejection from Auth evicts immediately, so
suspension is not delayed by the stale window."
```

---

## Definition of done

- `deploy/production`: `bun test tests/` green, `bunx tsc --noEmit` clean.
- `apps/Nexus-Auth`: full suite still green.
- `bun run proxy.ts` on a spare port still starts and serves.
- Production untouched: the running proxy on 8080 is **not** restarted, so no host becomes gated yet.

## Deliberately not in this plan

- Turning gating on for any real route. No Cloud route gets `requiresAuth: true` here — that is a data change, made deliberately at Phase 4 after the security review.
- The dashboard and any UI (Phase 3).
- nexus-chat verifying the token, port rebinding, JWT secret rotation (Phase 4).
- Caching JWKS in apps — that belongs with the first app integration, in Phase 4.
