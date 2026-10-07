# Privacy Zero Retention Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** No TNHC service receives, stores or logs a visitor's IP address; logs hold no personal data and live at most 24 hours; a static guard and a daily canary prove it.

**Architecture:** The ecosystem proxy strips every address-bearing header and substitutes a forgetful `X-Nexus-Client-Tag` (HMAC with an in-memory secret rotated every 24 h). Routes that bypass the proxy are moved behind it. Each service then switches from IP to the tag, drops stored IP/user-agent fields, and logs without personal data. Container and native logs are disabled or bounded.

**Tech Stack:** Bun/TypeScript (proxy, Auth, Dashboard, Hosting api-server, waitlist Worker), Rust (Chat `apps/Nexus`, Hosting `nexus-proxy`, Nexus-Email), Postgres, Caddy, Envoy, Docker Compose, systemd user units, bash.

**Spec:** `docs/superpowers/specs/2026-10-07-privacy-zero-retention-design.md`

## Global Constraints

- Header names stripped at the proxy (case-insensitive): `cf-connecting-ip`, `x-forwarded-for`, `x-real-ip`, `true-client-ip`, `cf-ipcountry`, `cf-ray`, `forwarded`, `x-client-ip`, `cf-visitor`, `cf-ew-via`, `cdn-loop`, `cf-connecting-ipv6`, `cf-pseudo-ipv4`.
- Tag header: `X-Nexus-Client-Tag`; value = first 22 chars of base64url(HMAC-SHA256(secret, address)); address absent → literal `unknown`.
- Tag secret: 32 random bytes, in memory only, replaced every 24 h; never logged or written.
- If header stripping throws, the proxy answers 500 and forwards nothing.
- Logs: technical events only — never an IP, email address, username, account ID, message content, or full URL with query string. Native logs ≤ 24 h.
- Sign-in history: event + timestamp + random device ID; 30-day retention; visible to its owner.
- Hosted-site statistics: `(site_id, path, day, views)` only.
- Documentation address ranges for tests/canary: `192.0.2.0/24`, `198.51.100.0/24`, `203.0.113.0/24`.
- Never `pgrep -f`/`pkill`; restart services only through `deploy/production/deploy.sh` (`restart` supports cloud, nexus-chat, nexus-chat-web; others: `stop_service <name>` via `bash -c 'source deploy/production/deploy.sh; stop_service <name>'` then `deploy.sh bg` with `CADDY_BIN=$HOME/.local/lib/nexus/bin/caddy NEXUS_PRODUCTION_START_ATTEMPTS=160`).
- Chat runs `apps/Nexus/target/debug/nexus`: never `cargo build` the chat workspace in place; build with `CARGO_TARGET_DIR=/run/media/zajferx/Data/dev/build-targets/nexus-chat-privacy` (data volume — the system disk is too small), then copy the binary over the live one only in the deploy step.
- Do not commit inside `/run/media/zajferx/Data/dev/The-No-hands-Company` or `projects/TNHC-Community-deployment` (their `.git` points at the Nexus-Systems remote). Changes there are live configuration, recorded in `docs/privacy/live-config.md` in Nexus-Systems.
- Container recreations and the tunnel switch are production restarts: the controller asks the user for the moment before each.
- Commit messages `type: description` + the session's Co-Authored-By trailer; push `main` fast-forward only.

---

### Task 1: Proxy front door — strip addresses, add the tag

**Files:**
- Create: `deploy/production/client-tag.ts`
- Modify: `deploy/production/proxy.ts` (HTTP forwarding block ~lines 550-585; request handler entry)
- Test: `deploy/production/tests/client-tag.test.ts`, `deploy/production/tests/proxy.test.ts`

**Interfaces:**
- Produces: `export function stripAddressHeaders(h: Headers): void`, `export function clientTag(address: string | null, now?: number): string`, `export const CLIENT_TAG_HEADER = "x-nexus-client-tag"`, `export const ADDRESS_HEADERS: readonly string[]`, `export function tagFromRequestHeaders(h: Headers): string`, `export function __rotateForTest(): void`. Every upstream request carries `x-nexus-client-tag` and none of `ADDRESS_HEADERS`.

- [ ] **Step 1: Failing unit tests** — `deploy/production/tests/client-tag.test.ts`:

```ts
import { describe, it, expect } from "bun:test";
import { stripAddressHeaders, clientTag, ADDRESS_HEADERS, __rotateForTest } from "../client-tag";

describe("client tag", () => {
  it("is stable for one address within a rotation period and 22 url-safe chars", () => {
    const a = clientTag("203.0.113.7");
    expect(a).toBe(clientTag("203.0.113.7"));
    expect(a).toMatch(/^[A-Za-z0-9_-]{22}$/);
    expect(clientTag("203.0.113.8")).not.toBe(a);
  });
  it("changes for the same address after rotation", () => {
    const before = clientTag("198.51.100.4");
    __rotateForTest();
    expect(clientTag("198.51.100.4")).not.toBe(before);
  });
  it("never contains the address and maps a missing address to 'unknown'", () => {
    expect(clientTag("192.0.2.55")).not.toContain("192.0.2.55");
    expect(clientTag(null)).toBe("unknown");
    expect(clientTag("")).toBe("unknown");
  });
  it("rotates on its own after 24 hours", () => {
    const t0 = 1_800_000_000_000;
    const a = clientTag("203.0.113.9", t0);
    expect(clientTag("203.0.113.9", t0 + 23 * 3600_000)).toBe(a);
    expect(clientTag("203.0.113.9", t0 + 24 * 3600_000 + 1)).not.toBe(a);
  });
});

describe("stripAddressHeaders", () => {
  it("removes every address- and location-bearing header", () => {
    const h = new Headers();
    for (const n of ADDRESS_HEADERS) h.set(n, "203.0.113.1");
    h.set("accept", "text/html");
    stripAddressHeaders(h);
    for (const n of ADDRESS_HEADERS) expect(h.has(n)).toBe(false);
    expect(h.get("accept")).toBe("text/html");
  });
});
```

- [ ] **Step 2: Run, expect FAIL** — `cd deploy/production && bun test tests/client-tag.test.ts` → module not found.

- [ ] **Step 3: Implement `deploy/production/client-tag.ts`**

```ts
// The proxy is the only place a visitor's address is ever read. It is turned
// into a tag that lets services count attempts without knowing who or where
// anyone is, and the secret behind the tag lives only in this process's memory
// and is replaced every 24 hours — after which no one, including TNHC, can map
// an old tag back to an address.
import { createHmac, randomBytes } from "node:crypto";

export const CLIENT_TAG_HEADER = "x-nexus-client-tag";
export const ADDRESS_HEADERS: readonly string[] = [
  "cf-connecting-ip", "cf-connecting-ipv6", "cf-pseudo-ipv4", "x-forwarded-for", "x-real-ip",
  "true-client-ip", "cf-ipcountry", "cf-ray", "forwarded", "x-client-ip", "cf-visitor",
  "cf-ew-via", "cdn-loop",
];
const ROTATE_MS = 24 * 60 * 60 * 1000;

let secret = randomBytes(32);
let rotatedAt = Date.now();

function current(now: number): Buffer {
  if (now - rotatedAt > ROTATE_MS) {
    secret = randomBytes(32);
    rotatedAt = now;
  }
  return secret;
}

export function clientTag(address: string | null, now: number = Date.now()): string {
  if (!address) return "unknown";
  return createHmac("sha256", current(now)).update(address).digest("base64url").slice(0, 22);
}

export function stripAddressHeaders(h: Headers): void {
  for (const name of ADDRESS_HEADERS) h.delete(name);
}

export function __rotateForTest(): void {
  secret = randomBytes(32);
  rotatedAt = Date.now();
}
```

The 24-hour test passes an explicit `now` far from `Date.now()`; the first such call rotates (the gap exceeds 24 h) and becomes the new origin, so the assertions hold without special-casing.

Also export the single place that reads the address header, so the CI guard (Task 12) can allow exactly this file:

```ts
export function tagFromRequestHeaders(h: Headers): string {
  return clientTag(h.get("cf-connecting-ip"));
}
```

- [ ] **Step 4: Unit tests pass** — `bun test tests/client-tag.test.ts` → 5 pass.

- [ ] **Step 5: Failing integration test** — append to `tests/proxy.test.ts`:

```ts
describe("address stripping at the front door", () => {
  it("forwards a tag and no address header to the upstream", async () => {
    const { handleRequest, __setRoutesForTest } = await import("../proxy");
    let seen: Headers | null = null;
    const up = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch(req) { seen = req.headers; return new Response("ok"); } });
    try {
      __setRoutesForTest({ "cloud.tnhc.dev": { upstream: `http://127.0.0.1:${up.port}`, requiresAuth: false, kind: "app" } });
      const res = await handleRequest(new Request("http://cloud.tnhc.dev/x", {
        headers: { "cf-connecting-ip": "203.0.113.77", "x-forwarded-for": "203.0.113.77", "x-real-ip": "203.0.113.77", "cf-ipcountry": "SE" },
      }));
      expect(res.status).toBe(200);
      const h = seen as unknown as Headers;
      expect(h.get("x-nexus-client-tag")).toMatch(/^[A-Za-z0-9_-]{22}$/);
      for (const n of ["cf-connecting-ip", "x-forwarded-for", "x-real-ip", "cf-ipcountry"]) expect(h.has(n)).toBe(false);
      for (const [, v] of h) expect(v).not.toContain("203.0.113.77");
    } finally { up.stop(true); }
  });
});
```

(Runs with `GATE_SKIP_AUTH=true`, set by this file's `beforeAll`.) Run → FAIL (address headers reach the upstream).

- [ ] **Step 6: Wire it into `proxy.ts`** — `import { stripAddressHeaders, tagFromRequestHeaders, CLIENT_TAG_HEADER } from "./client-tag";`. In the HTTP forwarding block, right after `const forwardHeaders = new Headers(req.headers);`:

```ts
      // Addresses stop here. Read the one Cloudflare sets, turn it into a
      // forgetful tag, and delete every header that could carry an address
      // or location before anything is forwarded.
      let tag: string;
      try {
        tag = tagFromRequestHeaders(req.headers);
        stripAddressHeaders(forwardHeaders);
        forwardHeaders.delete(CLIENT_TAG_HEADER);
        forwardHeaders.set(CLIENT_TAG_HEADER, tag);
      } catch {
        return new Response("Request refused", { status: 500 });
      }
```

Also apply the same to the email-ingress branch (it already forwards only four named headers — add `x-nexus-client-tag` is not needed there; leave it) and to the WebSocket upstream headers object near `const headers: Record<string, string> = {};` compute `tagFromRequestHeaders(req.headers)` at upgrade time, store it in `WsProxyData` as `clientTag: string` (add the field to the interface), and set `headers[CLIENT_TAG_HEADER] = data.clientTag` on the upstream socket. `proxy.ts` itself must contain no address-header name after this task.

Remove the address from any proxy `console.log` that prints a request (grep `console.log` in proxy.ts and gate.ts for `ip`, `x-forwarded`, `remote`); log lines must not include client addresses.

- [ ] **Step 7: Full proxy suite** — `cd deploy/production && bun test` → all pass except the pre-existing `terminal-public-hop` failure (fails identically on main before this task).

- [ ] **Step 8: Commit** — `git add deploy/production/client-tag.ts deploy/production/proxy.ts deploy/production/tests` ; `git commit -m "feat(proxy): strip address headers and forward a forgetful client tag"`; push.

- [ ] **Step 9: Deploy the proxy** (controller confirms the moment with the user): `bash -c 'source deploy/production/deploy.sh; stop_service proxy'` then `CADDY_BIN=$HOME/.local/lib/nexus/bin/caddy NEXUS_PRODUCTION_START_ATTEMPTS=160 bash deploy/production/deploy.sh bg`. Verify: `curl -s -o /dev/null -w '%{http_code}' 'https://auth.tnhc.dev/login?redirect_uri=https%3A%2F%2Fapp.tnhc.dev'` → 200; `https://app.tnhc.dev/` → 302.

---

### Task 2: Close the bypass routes (Supabase gateway and storage behind the proxy)

**Files:**
- Modify: `deploy/production/proxy.ts` (fixed upstreams), `deploy/production/tests/proxy.test.ts`
- Create: `docs/privacy/live-config.md`
- Live: Cloudflare tunnel ingress (API)

**Interfaces:**
- Consumes: Task 1 stripping (applies to these routes automatically).
- Produces: `auth.tnhc.dev` requests not matching the login pattern → `SUPABASE_UPSTREAM` (default `http://172.17.0.1:8000`); `storage.tnhc.dev` → `STORAGE_UPSTREAM` (default `http://127.0.0.1:9010`) with the original `Host` preserved.

- [ ] **Step 1: Failing tests** — append to `tests/proxy.test.ts`:

```ts
describe("former bypass routes", () => {
  const AUTH_LOGIN = /^\/(login$|health$|api\/v1\/(auth|account)(\/|$)|\.well-known\/openid-configuration$)/;
  it("keeps the original Host for storage so presigned URLs still verify", async () => {
    let host = ""; let path = "";
    const up = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch(req) { host = req.headers.get("host") ?? ""; path = new URL(req.url).pathname + new URL(req.url).search; return new Response("ok"); } });
    process.env.STORAGE_UPSTREAM = `http://127.0.0.1:${up.port}`;
    try {
      const { handleRequest } = await import("../proxy");
      const res = await handleRequest(new Request("http://storage.tnhc.dev/bucket/key?X-Amz-Signature=abc", { headers: { "cf-connecting-ip": "203.0.113.5" } }));
      expect(res.status).toBe(200);
      expect(host).toBe("storage.tnhc.dev");
      expect(path).toBe("/bucket/key?X-Amz-Signature=abc");
    } finally { up.stop(true); delete process.env.STORAGE_UPSTREAM; }
  });
  it("sends non-login auth.tnhc.dev paths to the Supabase gateway and login paths to Auth", async () => {
    const hits: string[] = [];
    const supa = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch(req) { hits.push("supabase " + new URL(req.url).pathname); return new Response("ok"); } });
    const auth = Bun.serve({ port: 0, hostname: "127.0.0.1", fetch(req) { hits.push("auth " + new URL(req.url).pathname); return new Response("ok"); } });
    process.env.SUPABASE_UPSTREAM = `http://127.0.0.1:${supa.port}`;
    process.env.AUTH_UPSTREAM_OVERRIDE = `http://127.0.0.1:${auth.port}`;
    try {
      const { handleRequest } = await import("../proxy");
      await handleRequest(new Request("http://auth.tnhc.dev/auth/v1/token"));
      await handleRequest(new Request("http://auth.tnhc.dev/login"));
      expect(hits).toEqual(["supabase /auth/v1/token", "auth /login"]);
      expect(AUTH_LOGIN.test("/login")).toBe(true);
    } finally { supa.stop(true); auth.stop(true); delete process.env.SUPABASE_UPSTREAM; delete process.env.AUTH_UPSTREAM_OVERRIDE; }
  });
});
```

Run → FAIL.

- [ ] **Step 2: Implement** in `proxy.ts`, next to the email-ingress branch (both upstreams read at request time so tests can set them; both must be loopback or the docker bridge `172.17.0.1`):

```ts
const AUTH_OWN_PATHS = /^\/(login$|health$|api\/v1\/(auth|account)(\/|$)|\.well-known\/openid-configuration$)/;
function supabaseUpstream(): string { return process.env.SUPABASE_UPSTREAM || "http://172.17.0.1:8000"; }
function storageUpstream(): string { return process.env.STORAGE_UPSTREAM || "http://127.0.0.1:9010"; }
function authUpstream(): string { return process.env.AUTH_UPSTREAM_OVERRIDE || FALLBACK_AUTH_UPSTREAM; }
```

In `handleRequestInner`, after the email-ingress block: for `host === "storage." + DOMAIN` set `upstreamUrl = storageUpstream()`, mark `preserveHost = true`, `kind = "site"` (public, ungated — presigned URLs carry their own authorisation); for `host === "auth." + DOMAIN` choose `authUpstream()` when `AUTH_OWN_PATHS.test(url.pathname)`, else `supabaseUpstream()` (ungated; Supabase authenticates itself). In the forwarding block, when `preserveHost` is true, do **not** delete `host` and do not set `x-forwarded-host`; set the fetch URL host to the upstream but pass `headers.set("host", host)` — Bun's fetch honours an explicit `host` header.

- [ ] **Step 3: Tests pass**; full proxy suite as in Task 1 Step 7. Commit `feat(proxy): serve auth.tnhc.dev Supabase paths and storage through the front door`; push; redeploy the proxy as in Task 1 Step 9 (controller confirms moment).

- [ ] **Step 4: Switch the tunnel** (controller confirms the moment; rollback ready). Using the CF token in `apps/Nexus-Cloud/.env`:

```bash
A=fe2435f2045c5a1cc39c0b5f78fc92c7; T=a3fc7587-49de-4792-b532-882775db6457
CF=$(sed -n 's/^CF_API_TOKEN=//p' apps/Nexus-Cloud/.env | tr -d '"\r')
curl -s -H "Authorization: Bearer $CF" https://api.cloudflare.com/client/v4/accounts/$A/cfd_tunnel/$T/configurations > /tmp/claude-ingress-before.json
jq '.result.config | .ingress |= map(if (.hostname=="auth.tnhc.dev" and .path==null) or .hostname=="storage.tnhc.dev" then .service="http://192.168.0.179:8080" else . end) | {config: .}' /tmp/claude-ingress-before.json > /tmp/claude-ingress-after.json
diff <(jq .result.config.ingress /tmp/claude-ingress-before.json) <(jq .config.ingress /tmp/claude-ingress-after.json)
curl -s -X PUT -H "Authorization: Bearer $CF" -H 'Content-Type: application/json' --data @/tmp/claude-ingress-after.json https://api.cloudflare.com/client/v4/accounts/$A/cfd_tunnel/$T/configurations | jq .success
```

Expected diff: exactly two `service` values change. Verify within 60 s: Auth login 200; a Supabase path (`https://auth.tnhc.dev/auth/v1/health` with the anon key header from the community deployment's `.env`) answers as before; a presigned MinIO upload+download via the Hosting deploy flow works (memory `nexus-hosting-site-deploy-flow`); draw.tnhc.dev still 200. Rollback on any failure: PUT `{config: .result.config}` from `/tmp/claude-ingress-before.json`.

- [ ] **Step 5: Record** — `docs/privacy/live-config.md` with a "Tunnel ingress" section: the two changed rules, date, and the rollback command. Commit `docs: record tunnel ingress change for zero retention`; push.

---

### Task 3: Auth — tag instead of IP, IP-free audit with device IDs, activity endpoint

**Files:**
- Modify: `apps/Nexus-Auth/src/server.ts` (`getClientIp` ~212, session creation ~380/468/585/695, login audit ~687/691, add route), `src/audit.ts`, `src/sessions.ts`, `src/types.ts:130-131`, `src/ratelimit.ts` (callers only)
- Create: `apps/Nexus-Auth/migrations/2026-10-07-zero-retention.sql`, `apps/Nexus-Auth/tests/privacy.test.ts`
- Modify: `deploy/production/deploy.sh` (pass `NEXUS_AUTH_AUDIT_DATABASE_URL` to auth, adopted from `apps/Nexus-Auth/.env` like other secrets)

**Interfaces:**
- Produces: `Session` has `deviceId: string`, no `ipAddress`/`userAgent`. `AuditEntry` = `{ event, userId?, actorId?, deviceId?, detail? }`. `GET /api/v1/auth/activity` (session cookie/bearer) → `{ events: [{ event, deviceId, at }] }` for the caller only, newest first, ≤ 30 days. `GET /api/v1/auth/sessions` items carry `{ id, deviceId, createdAt, expiresAt, current }`.

- [ ] **Step 1: Failing tests** — `tests/privacy.test.ts` (follow `tests/setup.ts` and `tests/auth.test.ts` for server start-up and sign-in helpers):

```ts
import { describe, it, expect } from "bun:test";
// Use the same helpers auth.test.ts uses to start the server and sign in.
import { startTestServer, signIn } from "./setup";

describe("zero retention in Auth", () => {
  it("never stores an address or user agent on a session", async () => {
    const srv = await startTestServer();
    const res = await signIn(srv, { headers: { "x-forwarded-for": "203.0.113.20", "user-agent": "Canary/1.0", "x-nexus-client-tag": "tagAAAAAAAAAAAAAAAAAAA" } });
    const list = await (await fetch(`${srv.url}/api/v1/auth/sessions`, { headers: res.authHeaders })).json();
    const text = JSON.stringify(list);
    expect(text).not.toContain("203.0.113.20");
    expect(text).not.toContain("Canary/1.0");
    expect(list.sessions[0].deviceId).toMatch(/^dev_[A-Za-z0-9_-]{16,}$/);
  });
  it("rate-limits by tag + account, not by address", async () => {
    const srv = await startTestServer();
    for (let i = 0; i < 10; i++) await signIn(srv, { password: "wrong", expectFail: true, headers: { "x-nexus-client-tag": "tagBBBBBBBBBBBBBBBBBBB" } }); // pragma: allowlist secret
    const blocked = await signIn(srv, { password: "wrong", expectFail: true, headers: { "x-nexus-client-tag": "tagBBBBBBBBBBBBBBBBBBB" } }); // pragma: allowlist secret
    expect(blocked.status).toBe(429);
  });
  it("shows a user only their own recent activity, without addresses", async () => {
    const srv = await startTestServer();
    const me = await signIn(srv, { headers: { "x-forwarded-for": "198.51.100.9" } });
    const body = await (await fetch(`${srv.url}/api/v1/auth/activity`, { headers: me.authHeaders })).json();
    expect(Array.isArray(body.events)).toBe(true);
    expect(JSON.stringify(body)).not.toContain("198.51.100.9");
  });
});
```

If `tests/setup.ts` exports different helper names, adapt the imports to the existing helpers and keep the assertions unchanged. Run `cd apps/Nexus-Auth && bun test tests/privacy.test.ts` → FAIL.

- [ ] **Step 2: Implement**
  - Replace `getClientIp` with `function clientTag(request: Request): string { return request.headers.get("x-nexus-client-tag") || "unknown"; }`; update every call site (`checkRateLimit`, `recordFailure`, `clearFailures` keys keep their shape: `${tag}:${username}`).
  - `sessions.ts`: `createSession({ userId, expiresInHours })` mints `deviceId = "dev_" + randomBytes(16).toString("base64url")`; remove `ipAddress`/`userAgent` from the input, the stored object and `types.ts`.
  - `audit.ts`: remove `ip`/`userAgent` from `AuditEntry`; add `deviceId?`; INSERT `(event, user_id, actor_id, device_id, detail)`; add `export async function recentActivity(userId: string): Promise<{event: string; deviceId: string | null; at: string}[]>` (`SELECT event, device_id, created_at FROM auth_audit_log WHERE user_id = $1 AND created_at > now() - interval '30 days' ORDER BY created_at DESC LIMIT 200`) and `export async function purgeOld(): Promise<void>` (`DELETE FROM auth_audit_log WHERE created_at < now() - interval '30 days'`), called once at start-up and every 6 hours with `setInterval(...).unref()`.
  - `server.ts`: pass `deviceId: session.deviceId` into audit calls made after sign-in; add `GET /api/v1/auth/activity`; sessions list maps to `{ id, deviceId, createdAt, expiresAt, current }`.
  - Migration `migrations/2026-10-07-zero-retention.sql`:

```sql
UPDATE auth_audit_log SET ip = NULL, user_agent = NULL;
ALTER TABLE auth_audit_log DROP COLUMN IF EXISTS ip;
ALTER TABLE auth_audit_log DROP COLUMN IF EXISTS user_agent;
ALTER TABLE auth_audit_log ADD COLUMN IF NOT EXISTS device_id text;
DELETE FROM auth_audit_log WHERE created_at < now() - interval '30 days';
```

Apply: `docker exec -i nexus-systems-postgres-1 psql -U nexus -d nexus < apps/Nexus-Auth/migrations/2026-10-07-zero-retention.sql`.
  - `deploy.sh`: adopt `NEXUS_AUTH_AUDIT_DATABASE_URL` from `apps/Nexus-Auth/.env` (same `sed` pattern as `NEXUS_EMAIL_DATABASE_URL`) and export it only around `start_service "auth"`. Add the variable to `apps/Nexus-Auth/.env` as `postgres://nexus:<password from repo-root .env POSTGRES_PASSWORD>@127.0.0.1:5432/nexus` (gitignored).

- [ ] **Step 3: Tests pass** — `bun test` in apps/Nexus-Auth (full suite), `bunx tsc --noEmit`.
- [ ] **Step 4: Commit** `feat(auth): device IDs and IP-free audit with 30-day retention`; push.
- [ ] **Step 5: Deploy** (controller confirms): `stop_service auth` then `deploy.sh bg` (Global Constraints). Verify sign-in works end to end and `docker exec nexus-systems-postgres-1 psql -U nexus -d nexus -Atc "select column_name from information_schema.columns where table_name='auth_audit_log'"` lists no `ip`/`user_agent`.

---

### Task 4: Dashboard — "Your recent activity" and device-based sessions

**Files:**
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/Account.tsx` (~185-195 session rows), `src/api.ts`
- Test: `apps/Nexus-Dashboard/frontend/src/pages/Account.test.tsx`

**Interfaces:**
- Consumes: Task 3 `GET /api/v1/auth/activity`, sessions `{ id, deviceId, createdAt, expiresAt, current }`, existing revoke route `/api/v1/auth/sessions/:id/revoke`.

- [ ] **Step 1: Failing test** — in `Account.test.tsx`, following its existing fetch-stub pattern, stub sessions `[{id:"s1",deviceId:"dev_abc",createdAt:"2026-10-07T10:00:00Z",expiresAt:"2026-10-08T10:00:00Z",current:true}]` and activity `{events:[{event:"login_success",deviceId:"dev_abc",at:"2026-10-07T10:00:00Z"}]}`, then assert: a heading "Your recent activity" exists; text "Signed in" is shown; the device label "Device dev_abc" (first 8 chars after `dev_`) is shown; no element text matches `/\d+\.\d+\.\d+\.\d+/`; a "Sign this device out" button exists for a non-current session.
- [ ] **Step 2: Run** `npx vitest run src/pages/Account.test.tsx` → FAIL.
- [ ] **Step 3: Implement** — `api.ts`: `export async function myActivity(): Promise<{event:string;deviceId:string|null;at:string}[]>` (GET `/ipa/v1/auth/activity` — match the existing `/ipa` prefix convention in api.ts). `Account.tsx`: session rows show `Device {deviceId.slice(4, 12)}`, created time, and "This device" for `current`; replace the `ipAddress`/`userAgent` lines; add a "Your recent activity" section listing events with human labels (`login_success`→"Signed in", `login_failure`→"Wrong password", `logout`→"Signed out", `password_change`→"Password changed", `session_revoked`→"Device signed out", others → the raw event name) and relative time; "Sign this device out" calls the revoke route.
- [ ] **Step 4: Full dashboard suite** `npx vitest run` → all pass.
- [ ] **Step 5: Commit** `feat(dashboard): recent activity and device-based sessions without addresses`; push; deploy with `bun run build` once (this checkout serves dist live); verify `curl -s http://127.0.0.1:3132/ | grep -o 'assets/index-[A-Za-z0-9_-]*\.js'` equals `dist/index.html`'s.

---

### Task 5: Chat — tag instead of IP; drop stored addresses

**Files (submodule `apps/Nexus`, its own repo):**
- Modify: `crates/nexus-api/src/middleware.rs:400-420` (`extract_client_ip`), `crates/nexus-db/src/repository/sessions.rs:35` (refresh_tokens insert), `crates/nexus-db/src/repository/audit_log.rs:202`, every caller passing `ip_address`/`user_agent` into those
- Create: `crates/nexus-db/migrations/20261007000001_zero_retention.sql`, `crates/nexus-db/migrations-lite/20261007000001_zero_retention.sql`
- Test: `crates/nexus-api/src/middleware.rs` (unit test module), `crates/nexus-db/tests/` (follow existing migration tests)

**Interfaces:**
- Produces: `pub fn extract_client_tag(headers: &HeaderMap) -> String` (returns `x-nexus-client-tag` or `"unknown"`); `extract_client_ip` removed, all callers switched. `refresh_tokens`, `instance_audit_log`, `push_subscriptions` have no `ip_address`/`user_agent` columns.

- [ ] **Step 1: Failing unit test** in `middleware.rs` tests:

```rust
#[test]
fn client_tag_comes_from_the_proxy_header_never_from_address_headers() {
    let mut h = axum::http::HeaderMap::new();
    h.insert("x-forwarded-for", "203.0.113.30".parse().unwrap());
    h.insert("x-real-ip", "203.0.113.30".parse().unwrap());
    assert_eq!(extract_client_tag(&h), "unknown");
    h.insert("x-nexus-client-tag", "tagCCCCCCCCCCCCCCCCCCC".parse().unwrap());
    assert_eq!(extract_client_tag(&h), "tagCCCCCCCCCCCCCCCCCCC");
}
```

Run `CARGO_TARGET_DIR=/run/media/zajferx/Data/dev/build-targets/nexus-chat-privacy cargo test -p nexus-api client_tag_comes_from` → FAIL.

- [ ] **Step 2: Implement** `extract_client_tag`; replace every `extract_client_ip(` call with `extract_client_tag(` (rate-limit keys become `rl:<scope>:tag:<tag>`); delete `extract_client_ip`. Remove `ip_address`/`user_agent` parameters from the refresh-token and audit-log repository functions and their callers. Migrations (Postgres):

```sql
UPDATE refresh_tokens SET ip_address = NULL, user_agent = NULL;
ALTER TABLE refresh_tokens DROP COLUMN IF EXISTS ip_address;
ALTER TABLE refresh_tokens DROP COLUMN IF EXISTS user_agent;
UPDATE instance_audit_log SET ip_address = NULL, user_agent = NULL;
ALTER TABLE instance_audit_log DROP COLUMN IF EXISTS ip_address;
ALTER TABLE instance_audit_log DROP COLUMN IF EXISTS user_agent;
ALTER TABLE push_subscriptions DROP COLUMN IF EXISTS user_agent;
```

SQLite-lite variant: SQLite ≥ 3.35 supports `ALTER TABLE … DROP COLUMN`; use the same statements without `IF EXISTS` on DROP COLUMN (check the lite schema actually has each column first and only drop those that exist). Follow the repo's AnyPool conventions (memory `nexus-chat-never-compiled`).
- [ ] **Step 3: Tests** — the new unit test passes; `cargo test --workspace` with the separate target dir and `NEXUS_*TEST_DATABASE_URL` the chat tests already use; no new failures versus main (record the before/after counts).
- [ ] **Step 4: Commit in `apps/Nexus`** `feat: rate-limit by proxy client tag; drop stored addresses`; push its main; in Nexus-Systems commit the bumped submodule pointer.
- [ ] **Step 5: Deploy** (controller confirms): build `cargo build -p nexus-server` (or the binary target named in `deploy.sh`'s nexus-chat block) with the separate target dir; `bash deploy/production/deploy.sh restart nexus-chat` after copying the new binary to the path `deploy.sh` runs (read `start_nexus_chat_service` for it); migrations run at start-up per the app's existing migrator — confirm with `docker exec nexus-systems-postgres-1 psql -U nexus -d nexus_chat -Atc "select count(*) from information_schema.columns where column_name in ('ip_address','user_agent')"` → 0. Verify chat.tnhc.dev sign-in and sending a message.

---

### Task 6: Hosting api-server — page-view counts only; no addresses anywhere

**Files (submodule `apps/Nexus-Hosting`, own repo):**
- Modify: `artifacts/api-server/src/middleware/hostRouter.ts` (`recordHit` ~83), `src/routes/analytics.ts` (SSE + queries), `src/lib/analyticsFlush.ts`, `src/lib/retentionCleanup.ts`, `src/lib/auditLog.ts`, `src/middleware/ipBan.ts`, `src/middleware/rateLimiter.ts`, `src/routes/abuse.ts`, `src/routes/forms.ts`, `src/routes/access.ts`, `src/routes/admin.ts`, `src/lib/geoRouting.ts`, `src/app.ts`, `artifacts/federated-hosting/src/lib/apiHooks.ts`, `artifacts/cli/src/commands/analytics.ts`
- Modify: `lib/db/src/schema/analytics.ts` (+ other schema files declaring `ip_hash`, `ip_address`, `ip`, `reporter_ip`, `unique_ips`, `top_referrers`, `user_agent`)
- Create: migration under the repo's existing drizzle migrations directory; test files beside existing api-server tests

**Interfaces:**
- Produces: table `site_page_views(site_id int references sites(id) on delete cascade, path text, day date, views int not null default 0, primary key(site_id, path, day))`; `recordPageView(siteId: number, path: string): void` (upsert `views = views + 1`); analytics API returns `{ days: [{ day, path, views }] }`; no SSE hit stream. IP bans become tag bans held in memory for ≤ 24 h (`banTag(tag)`, `isTagBanned(tag)`), admin-only.

- [ ] **Step 1: Failing tests** (vitest, `artifacts/api-server`): (a) serving a hosted page with `x-forwarded-for: 203.0.113.40`, `referer: https://example.com/` increments `site_page_views` for `(site, path, today)` and no table/row anywhere contains `203.0.113.40` or `example.com` (query every text column of every table touched by the request); (b) `GET` analytics returns `{days:[{day,path,views}]}` only; (c) the SSE live-hit endpoint is gone (404); (d) a form submission and an abuse report store no address or hash; (e) rate limiter keys on `x-nexus-client-tag`. Run → FAIL.
- [ ] **Step 2: Implement** — replace `recordHit` with `recordPageView`; delete `analytics_buffer` writes, flush, SSE broadcast; replace `site_analytics` reads with `site_page_views`; switch `req.ip`/`x-forwarded-for` reads to `req.headers["x-nexus-client-tag"]` in rateLimiter/ipBan/abuse/forms/access/admin/auditLog; geoRouting stops using the client address (nearest-node choice falls back to the default node); drop the address/hash/referrer/user-agent columns via migration:

```sql
CREATE TABLE IF NOT EXISTS site_page_views (site_id integer NOT NULL REFERENCES sites(id) ON DELETE CASCADE, path text NOT NULL, day date NOT NULL, views integer NOT NULL DEFAULT 0, PRIMARY KEY (site_id, path, day));
DROP TABLE IF EXISTS analytics_buffer;
DROP TABLE IF EXISTS site_analytics;
ALTER TABLE admin_audit_log DROP COLUMN IF EXISTS ip_address, DROP COLUMN IF EXISTS ip, DROP COLUMN IF EXISTS user_agent;
ALTER TABLE form_submissions DROP COLUMN IF EXISTS ip_hash, DROP COLUMN IF EXISTS user_agent;
ALTER TABLE abuse_reports DROP COLUMN IF EXISTS reporter_ip;
DROP TABLE IF EXISTS ip_bans;
```

Update the dashboard UI (`artifacts/federated-hosting`) and CLI to render views per page per day.
- [ ] **Step 3: Tests** — full `pnpm --filter api-server test` (or the repo's `vitest run`), type-check per `CLAUDE.md` of that repo (memory `checks-that-dont-check`: its `npx tsc` placeholder fakes success — use the real `tsc -b`).
- [ ] **Step 4: Commit in `apps/Nexus-Hosting`** `feat: page-view counts only; no visitor addresses stored`; push; bump pointer in Nexus-Systems.

---

### Task 7: Hosting Rust proxy — count views, no addresses; rebuild and redeploy Hosting

**Files:** `apps/Nexus-Hosting/crates/nexus-proxy/src/handler.rs` (~183), `src/db.rs:169-186`, `src/main.rs` docs

**Interfaces:** Consumes Task 6 `site_page_views`. Produces: the proxy upserts `(site_id, path, current_date)` and never reads `x-forwarded-for`/remote address.

- [ ] **Step 1: Failing test** — a unit test in `db.rs` (or the crate's tests dir) that the SQL issued by `record_page_view(site_id, path)` is `INSERT INTO site_page_views (site_id, path, day, views) VALUES ($1, $2, current_date, 1) ON CONFLICT (site_id, path, day) DO UPDATE SET views = site_page_views.views + 1` and takes exactly two parameters. Run → FAIL.
- [ ] **Step 2: Implement** — replace `record_hit(site_id, path, referrer, ip_hash, bytes)` with `record_page_view(site_id, path)`; remove IP/referrer extraction in `handler.rs`.
- [ ] **Step 3: Tests** — `cargo test -p nexus-proxy` (in `apps/Nexus-Hosting`, `CARGO_TARGET_DIR=/run/media/zajferx/Data/dev/build-targets/nexus-hosting`).
- [ ] **Step 4: Commit + push** in `apps/Nexus-Hosting`; bump pointer in Nexus-Systems.
- [ ] **Step 5: Rebuild and redeploy Hosting** (controller confirms) per memory `nexus-hosting-build` (its documented rebuild command, `SHELL_ORIGINS` explicit); apply the Task 6 migration in `nexus-hosting-db-1`; verify `https://draw.tnhc.dev/` → 200 with `x-served-by: nexus-proxy/rust`, a view row appears for today, and `docker exec nexus-hosting-db-1 sh -c 'psql -U nexus -d nexus -Atc "select count(*) from information_schema.columns where column_name ~ \"ip|referrer|user_agent\""'` → 0.

---

### Task 8: Email — no addresses in logs; prove no client address in submitted mail

**Files:** `apps/Nexus-Email/crates/nexus-mailout/src/client.rs:118,224`, `src/worker.rs:165,220,226`, `crates/nexus-mailsmtp/src/policy.rs:39`; tests in the same crates

- [ ] **Step 1: Failing tests** — (a) in `nexus-mailsmtp`, an integration test (follow existing submission tests) that submits a message over the submission listener from `127.0.0.1` and asserts the stored/queued RFC 5322 bytes contain no `127.0.0.1` and no `Received:` line naming the client; (b) a tracing-capture test (use `tracing-subscriber` test writer, already a dependency) asserting the delivery log lines for a send to `someone@example.com` do not contain `someone@example.com`. Run → (a) likely PASS already (document), (b) FAIL.
- [ ] **Step 2: Implement** — remove `%recipient`, `%address`, `from = envelope_from` fields from those log macros; log a queue item id instead (`item.id`).
- [ ] **Step 3: Tests** — `bash check.sh` in apps/Nexus-Email (uses the test DB) → PASS.
- [ ] **Step 4: Commit + push**; rebuild release (`cargo build --release -p nexus-mailapi -p nexus-mailsmtp`); restart via `stop_service mailsmtpd; stop_service mailapi` then `deploy.sh bg` (controller confirms); verify a test send to `delivered@resend.dev` reaches `delivered`.

---

### Task 9: Supabase — no addresses in the auth log, gateway access log off, dev log collection stopped

**Files (live, not committed — Global Constraints):** `projects/TNHC-Community-deployment/volumes/api/envoy/lds.template.yaml`, its `docker-compose.yml`; record in Nexus-Systems `docs/privacy/live-config.md`
**Create (committed):** `docs/privacy/supabase-zero-retention.sql` in Nexus-Systems

- [ ] **Step 1: Write the SQL** `docs/privacy/supabase-zero-retention.sql`:

```sql
UPDATE auth.audit_log_entries SET ip_address = '' WHERE ip_address <> '';
CREATE OR REPLACE FUNCTION public.tnhc_blank_audit_ip() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN NEW.ip_address := ''; RETURN NEW; END $$;
DROP TRIGGER IF EXISTS tnhc_blank_audit_ip ON auth.audit_log_entries;
CREATE TRIGGER tnhc_blank_audit_ip BEFORE INSERT OR UPDATE ON auth.audit_log_entries
  FOR EACH ROW EXECUTE FUNCTION public.tnhc_blank_audit_ip();
```

- [ ] **Step 2: Test before applying** — in a throwaway: `docker exec supabase-db psql -U postgres -c "BEGIN; <file contents>; INSERT INTO auth.audit_log_entries (instance_id, id, payload, created_at, ip_address) VALUES (gen_random_uuid(), gen_random_uuid(), '{}', now(), '203.0.113.60'); SELECT ip_address FROM auth.audit_log_entries WHERE ip_address = '203.0.113.60'; ROLLBACK;"` → zero rows selected. Then apply for real: `docker exec -i supabase-db psql -U postgres < docs/privacy/supabase-zero-retention.sql`; verify the count of non-empty `ip_address` is 0 and that a real sign-up/sign-in from the Community app still works.
- [ ] **Step 3: Gateway access log** — delete the `access_log:` block (lines 26-33 in `volumes/api/envoy/lds.template.yaml`); in its `docker-compose.yml` add to the `envoy` service `logging: { driver: "none" }`; recreate only envoy (controller confirms): `docker compose -f docker-compose.yml up -d --no-deps --force-recreate envoy` from that directory. Verify Supabase paths answer through the proxy (Task 2 Step 4 checks).
- [ ] **Step 4: Dev copy** — stop and disable log collection of the local dev stack: `docker update --restart=no supabase_analytics_tnhc-community-local supabase_vector_tnhc-community-local && docker stop supabase_analytics_tnhc-community-local supabase_vector_tnhc-community-local`.
- [ ] **Step 5: Record** all of the above in `docs/privacy/live-config.md`; commit the SQL + doc; push.

---

### Task 10: Logs — 24-hour native logs, Caddy access logs off, container log drivers

**Files:**
- Create: `deploy/production/log-rotate.sh`, `deploy/production/systemd/nexus-log-rotate.service`, `deploy/production/systemd/nexus-log-rotate.timer`, `deploy/production/tests/log-rotate.test.sh`
- Modify: `deploy/production/deploy.sh` (install + enable the user timer in `cmd_start`), `deploy/production/nexus-chat.Caddyfile`, `nexus-draw.Caddyfile`, `nexus-calendar.Caddyfile` (remove access `log` blocks), `deploy/production/tests/caddy-logging.test.ts` (assert no access logging), `deploy/production/cloudflared.compose.yml`, `docker-compose.yml` (infra), `apps/Nexus-Hosting/docker-compose.production.yml` (in its repo)

- [ ] **Step 1: Failing test** `tests/log-rotate.test.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail
d=$(mktemp -d); trap 'rm -rf "$d"' EXIT
printf 'old\n' > "$d/a.log"; printf 'older\n' > "$d/a.log.1"
LOG_DIR="$d" bash "$(dirname "$0")/../log-rotate.sh"
[ "$(cat "$d/a.log.1")" = "old" ] || { echo "FAIL: previous not kept"; exit 1; }
[ ! -s "$d/a.log" ] || { echo "FAIL: current not truncated"; exit 1; }
! grep -q older "$d"/* || { echo "FAIL: older than one period survived"; exit 1; }
echo PASS
```

Run → FAIL (script missing).
- [ ] **Step 2: Implement** `log-rotate.sh`:

```bash
#!/usr/bin/env bash
# Keeps native service logs to at most 24 hours: run every 12 hours, keep the
# current file and exactly one previous period. copy-truncate, because the
# services hold their log file open (deploy.sh appends with >>).
set -euo pipefail
LOG_DIR="${LOG_DIR:-/tmp/nexus-production}"
shopt -s nullglob
for f in "$LOG_DIR"/*.log; do
  cp -- "$f" "$f.1"
  : > "$f"
done
```

Units (`systemd/nexus-log-rotate.service`: `Type=oneshot`, `ExecStart=%h/.local/lib/nexus/log-rotate.sh`; `.timer`: `OnBootSec=1h`, `OnUnitActiveSec=12h`, `Persistent=true`, `WantedBy=timers.target`). `deploy.sh cmd_start`: copy the script to `~/.local/lib/nexus/`, the units to `~/.config/systemd/user/`, `systemctl --user daemon-reload && systemctl --user enable --now nexus-log-rotate.timer` (warn, don't fail, if the user manager is unavailable).
- [ ] **Step 3: Caddy** — remove the access `log { … }` blocks from the three Caddyfiles (error logging stays via the global default); update `caddy-logging.test.ts` to assert no `log` directive with request logging; run `bun test tests/caddy-logging.test.ts`.
- [ ] **Step 4: Containers** — add to `cloudflared.compose.yml` service: `logging: { driver: "none" }`; to every service in `docker-compose.yml` (postgres, redis, minio): `logging: { driver: "local", options: { max-size: "5m", max-file: "1" } }`; same for every service in `apps/Nexus-Hosting/docker-compose.production.yml` (its repo; commit + push there). Recreating applies them (controller confirms the moment; each is a short restart): `docker compose -f deploy/production/cloudflared.compose.yml up -d --force-recreate` (brief public outage — do it last and verify all hosts after), infra `docker compose up -d --force-recreate postgres redis minio` (stateful containers keep their volumes), Hosting per its rebuild doc.
- [ ] **Step 5: Backups** — list candidates: `find /run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/Backups /run/media/zajferx/Data/dev -maxdepth 4 \( -name '*.sql' -o -name '*.dump' -o -name '*.sql.gz' \) -newermt 2026-01-01 2>/dev/null`; grep each for IPv4 patterns in IP-bearing tables; report the list to the controller (deletion is the user's call — `Backups/` is a do-not-touch area).
- [ ] **Step 6: Tests + commit** — `bash tests/log-rotate.test.sh` → PASS; proxy suite; commit `feat(deploy): 24-hour native logs, no access logs, bounded container logs`; push. Verify after deploy: `systemctl --user list-timers | grep nexus-log-rotate`; `docker inspect cloudflared --format '{{.HostConfig.LogConfig.Type}}'` → `none`.

---

### Task 11: Waitlist — delete an address once invited

**Files (repo tnhc.dev):** `workers/waitlist/src/index.*` (routes near lines 81, 93, 125), its test (follow `workers/waitlist` test setup if present, else `tests/` pattern)

- [ ] **Step 1: Failing test** — `POST /api/admin/signups/<id>/invited` with the admin bearer token deletes that row (subsequent admin list no longer contains it; count drops by one); without the token → 401.
- [ ] **Step 2: Implement** the route: verify `Authorization: Bearer ${env.ADMIN_API_TOKEN}` exactly as the existing admin list does; `DELETE FROM waitlist WHERE id = ?`; return `{ deleted: true }`.
- [ ] **Step 3: Tests pass; commit** `feat(waitlist): forget an address once invited`; push; deploy per the worker's README (`npx wrangler deploy` from `workers/waitlist` with the existing CF token; if the token lacks scope, report).

---

### Task 12: Static privacy guard in CI

**Files:** Create `scripts/check-privacy.sh`, `scripts/tests/check-privacy.test.sh`; modify `.github/workflows/ci.yml` (job `edge`, after the "Licences" step)

- [ ] **Step 1: Failing test** `scripts/tests/check-privacy.test.sh`: create a temp tree containing `apps/X/src/a.ts` with `req.headers.get("x-forwarded-for")` and `apps/Y/migrations/1.sql` with `ALTER TABLE t ADD COLUMN ip_address inet`; run `ROOT=<tmp> scripts/check-privacy.sh` → expect exit 1 naming both files; run on a clean temp tree → exit 0 printing `PASS`.
- [ ] **Step 2: Implement** `scripts/check-privacy.sh`:

```bash
#!/usr/bin/env bash
# Fails when code outside the front door reads address headers, or when a
# migration adds an address or user-agent column. The proxy's stripping module
# is the one place allowed to touch these headers.
set -euo pipefail
ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT"
HEADERS='cf-connecting-ip|x-forwarded-for|x-real-ip|true-client-ip|cf-ipcountry|x-client-ip'
ALLOW='^(deploy/production/client-tag\.ts|deploy/production/tests/|scripts/check-privacy\.sh|scripts/tests/|docs/)'
fail=0
hits=$(git ls-files 2>/dev/null || find . -type f | sed 's#^\./##')
code=$(printf '%s\n' "$hits" | grep -E '\.(ts|tsx|js|mjs|rs|py|go)$' | grep -vE "$ALLOW" | grep -vE '(^|/)(node_modules|target|dist|build|tests?|__tests__)/' || true)
if [ -n "$code" ]; then
  bad=$(printf '%s\n' "$code" | xargs -r grep -liE "[\"'](${HEADERS})[\"']" 2>/dev/null || true)
  [ -n "$bad" ] && { echo "FAIL: address header read outside the front door:"; echo "$bad"; fail=1; }
fi
sql=$(printf '%s\n' "$hits" | grep -E '\.sql$' | grep -vE "$ALLOW" || true)
if [ -n "$sql" ]; then
  badsql=$(printf '%s\n' "$sql" | xargs -r grep -liE 'add column (if not exists )?(ip|ip_address|ip_hash|user_agent|reporter_ip|remote_addr)\b|\b(ip|ip_address|ip_hash|user_agent|reporter_ip)\s+(inet|text|varchar)' 2>/dev/null || true)
  [ -n "$badsql" ] && { echo "FAIL: migration stores an address or user agent:"; echo "$badsql"; fail=1; }
fi
[ "$fail" = 0 ] && echo PASS || exit 1
```

Existing historical migrations that *created* these columns (and are now dropped by later migrations) are listed explicitly in an `ALLOW_FILES` array at the top of the script, each with a comment naming the migration that drops the column; the test covers that a listed file is skipped.
- [ ] **Step 3: Run on the real repo** after Tasks 1–8 → PASS (fix any remaining reader it finds, or report if one is outside this plan's scope).
- [ ] **Step 4: CI** — add `- name: Privacy` / `run: bash scripts/check-privacy.sh` to job `edge`. Commit `ci: privacy guard`; push.

---

### Task 13: Daily canary

**Files:** Create `scripts/privacy-canary.sh`, `deploy/production/systemd/nexus-privacy-canary.service`, `.timer`; modify `deploy.sh` (install like Task 10); create `docs/privacy/canary.md`

- [ ] **Step 1: Implement** `scripts/privacy-canary.sh`:

```bash
#!/usr/bin/env bash
# Sends requests through the front door carrying a unique documentation-range
# address, then searches every place data could land for it. Exit 1 names any
# location where the address was found.
set -uo pipefail
ROOT="${ROOT:-$(cd "$(dirname "$0")/.." && pwd)}"
ADDR="203.0.113.$(( (RANDOM % 250) + 2 ))"
MARK="canary-$(date +%s)-$RANDOM"
OUT="${CANARY_OUT:-/tmp/nexus-production/privacy-canary.json}"
for host in auth.tnhc.dev app.tnhc.dev cloud.tnhc.dev chat.tnhc.dev hosting.tnhc.dev storage.tnhc.dev draw.tnhc.dev; do
  curl -s -o /dev/null -m 10 -H "Host: $host" -H "CF-Connecting-IP: $ADDR" -H "X-Forwarded-For: $ADDR" \
    -H "User-Agent: $MARK" "http://127.0.0.1:8080/?$MARK" || true
  curl -s -o /dev/null -m 10 -X POST -H "Host: $host" -H "CF-Connecting-IP: $ADDR" -H "User-Agent: $MARK" \
    -H 'Content-Type: application/json' --data '{"username":"canary","password":"canary"}' "http://127.0.0.1:8080/api/v1/auth/login" || true # pragma: allowlist secret
done
sleep 5
found=()
grep -rlF -e "$ADDR" -e "$MARK" /tmp/nexus-production/*.log 2>/dev/null | while read -r f; do echo "log:$f"; done > /tmp/claude-canary-hits
for c in $(docker ps --format '{{.Names}}'); do
  docker logs --since 5m "$c" 2>&1 | grep -qF -e "$ADDR" -e "$MARK" && echo "container:$c" >> /tmp/claude-canary-hits
done
for spec in "nexus-systems-postgres-1 nexus nexus" "nexus-systems-postgres-1 nexus nexus_chat" "nexus-systems-postgres-1 nexus nexus_email" "nexus-hosting-db-1 nexus nexus" "supabase-db postgres postgres"; do
  set -- $spec
  docker exec "$1" pg_dump -U "$2" -d "$3" --data-only 2>/dev/null | grep -qF -e "$ADDR" -e "$MARK" && echo "db:$1/$3" >> /tmp/claude-canary-hits
done
mapfile -t found < /tmp/claude-canary-hits; rm -f /tmp/claude-canary-hits
status=$([ ${#found[@]} -eq 0 ] && echo pass || echo fail)
printf '{"at":"%s","address":"%s","status":"%s","found":%s}\n' "$(date -u +%FT%TZ)" "$ADDR" "$status" \
  "$(printf '%s\n' "${found[@]:-}" | jq -R . | jq -s 'map(select(length>0))')" > "$OUT"
cat "$OUT"
[ "$status" = pass ]
```

The `MARK` user-agent catches user-agent storage; the query string catches URL logging. Database names: confirm the email DB name from `apps/Nexus-Email/.env` and the Hosting DB credentials from `nexus-hosting-db-1`'s env before first run, and fix the spec lines accordingly.
- [ ] **Step 2: Run once by hand** after all earlier tasks are deployed → `"status":"pass"`. If anything is found, report the location (that is a finding for the task that owns it).
- [ ] **Step 3: Timer** — `nexus-privacy-canary.service` (`Type=oneshot`, `ExecStart=%h/.local/lib/nexus/privacy-canary.sh`, `Environment=ROOT=<repo path>`), `.timer` `OnCalendar=daily`, `Persistent=true`; install via `deploy.sh` like Task 10. `docs/privacy/canary.md` explains what it does and where the latest result lives (`/tmp/nexus-production/privacy-canary.json`), for Part 2 to publish.
- [ ] **Step 4: Commit** `feat: daily privacy canary`; push.

---

### Task 14: Final verification

- [ ] `bash scripts/check-privacy.sh` → PASS.
- [ ] `bash scripts/privacy-canary.sh` → status pass.
- [ ] Every public host answers as before: auth login 200, app/cloud/chat/calendar 302, draw 200, a hosted-site presigned deploy works, Community-app sign-in works, a Nexus Email test send is delivered and an inbound test lands.
- [ ] `docker inspect` log types: cloudflared + supabase envoy `none`; others `local`.
- [ ] `systemctl --user list-timers` shows both timers.
- [ ] Report: what changed per service, live-config doc path, the backups list from Task 10 Step 5 awaiting the user's decision, and anything the canary found.
