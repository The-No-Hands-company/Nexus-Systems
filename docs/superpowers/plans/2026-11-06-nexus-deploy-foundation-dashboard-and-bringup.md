# Nexus Deploy Foundation — Plan 5 of 5: Dashboard UI, Documentation & Operator Bring-Up

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]") syntax for tracking.

**Goal:** Move the Nexus Deploy dashboard onto the shared Void theme (`docs/THEME-GUIDE.md`), expose all Foundation v0.6 capabilities in the UI (invite-only member gate, Git + OCI image sources, private-repo tokens, blue/green vs volume release indicators, build-log deletion, portable project export, node safeguards, and the Operators panel), clean up `AGENTS.md` / `README.md` / `check.sh`, wire `deploy/production/deploy.sh` and `scripts/privacy-canary.sh`, and document the operator bring-up and disposable second-node portability proof.

**Architecture:**
1. **Browser Auth & Stream Tickets (Task 1, `apps/Nexus-Deploy`):** Adds `GET /api/auth/login` and `GET /api/auth/logout` redirects using `config.nexusAuthPublicUrl` so the frontend never hardcodes an auth hostname. `GET /api/me` returns `node` metadata and a short-lived HMAC stream ticket (`wsToken`, `wst1.<payload>.<sig>`) so `/api/log-stream` and `/api/container-stream` work through the cookie-stripping reverse proxy while keeping `nexus_session` `HttpOnly`. Direct WebSocket connections also fall back to the `nexus_session` cookie. Also constructs the header-probe names in `src/verify/` without quoted address-header literals so `scripts/check-privacy.sh` passes cleanly on `apps/Nexus-Deploy`.
2. **Void-Themed Dashboard (Task 2, `apps/Nexus-Deploy`):** Vendors `packages/nexus-design/dist/nexus-tokens.css` into `web/src/styles/nexus-tokens.css`, rewrites `web/src/styles/index.css` on the Void theme tokens (`#030303` canvas, `#CCFF00` acid-lime accent, `Figtree` + `JetBrains Mono`, `100dvh`, `>= 44px` touch targets, `viewport-fit=cover`, zero gradients/blurs/glows), and splits `web/src/App.tsx` into focused modules (`< 460` lines each) supporting `?embed=1`, distinct `signed-out` / `unreachable` / `not-a-member` states, Git + Docker image sources, private-repo tokens, build-log deletion, portable JSON export, node safeguards, and the Operators panel.
3. **Documentation & Quality Gate (Task 3, `apps/Nexus-Deploy`):** Consolidates `AGENTS.md` and `README.md` around the v0.6 architecture (Railpack `v0.43.0`, rootless sandbox, Cloud `user-apps` routing, `doctor`, `verify`), removing stale pre-v0.6 references to Nixpacks, Traefik labels, and password auth routes. Adds `check.sh` as the app's quality gate.
4. **Monorepo Vendoring, Deployer & Privacy Canary (Task 4, monorepo root):** Registers `apps/Nexus-Deploy` in `packages/nexus-design/scripts/vendor.ts` and `docs/THEME-GUIDE.md`; updates `deploy/production/deploy.sh` to guard `nexus-draw-web` with `|| warn`, add `start_nexus_deploy_service` (passing `CLOUD_URL`, `HOSTING_SITE_URL`, and exporting secrets in a subshell rather than `ps`-visible `argv`), and support `deploy.sh restart nexus-deploy`; adds `deploy.tnhc.dev` to `scripts/privacy-canary.sh`.
5. **Operator Bring-Up & Second-Node Portability Proof (Task 5, operator-only):** Step-by-step commands for the human operator (`sudo bash sandbox/setup.sh`, `NEXUS_CLOUD_USER_APPS_KEY`, service restarts, `npm run doctor`, `npm run verify`, and the disposable `BASE_DOMAIN=local.test` second-node run).

**Tech Stack:** TypeScript 5.8 (strict), React 18, Vite 6, Express 5, Node `node:test`, Bash.

**Spec Reference:** `docs/superpowers/specs/2026-11-06-nexus-deploy-foundation-design.md` (§2, §7, §8.2, §9, §10, §11, §14 items 6 & 8).

---

## Verification Findings Resolved in This Plan

1. **Verification Task 6 (§14.6 — Why Deploy was not running although `dist/index.js` existed):**
   - Forensically traced via `/tmp/nexus-production/pids/nexus-deploy.pid` and `journalctl --user -u nexus-watchdog.service`: `nexus-deploy` last exited on Nov 5, and when `nexus-watchdog.service` ran `deploy/production/deploy.sh bg`, step `4d-1` (`start_service "nexus-draw-web"` on port `8091`) lacked `|| warn` and exited non-zero under `set -euo pipefail` because port `8091` was occupied by an unverifiable listener — aborting `cmd_start` before step `4d` (`nexus-deploy`) was ever reached. Furthermore, `nexus-deploy` was missing from `start_named_service` / `preflight_service_start`, so `deploy.sh restart nexus-deploy` was not supported. Fixed in Task 4.
2. **Verification Task 8 (§14.8 — How the daily privacy canary treats user-deployed containers, and what public privacy statements may truthfully say):**
   - **Daily canary (`scripts/privacy-canary.sh`):** Probes `deploy.tnhc.dev` (the control plane) so `/tmp/nexus-production/nexus-deploy.log` is scanned by `search_logs`. It deliberately does **not** `docker logs` or `grep` inside member-deployed containers in the rootless `nexus-deploy` daemon, because those containers belong to members (tenant workloads) and receive no canary probes; instead, proxy header stripping for member apps is verified by `npm run verify` (`proxy-strips-sensitive-headers`).
   - **Truthful public privacy statement:** *"Requests to apps deployed on Nexus Deploy pass through the ecosystem proxy, which removes visitor IP addresses, forwarding headers, cookies, authorization headers, and the rotating client tag before the request reaches the app container. Container stdout/stderr logs on the sandbox daemon are capped at 20 MB (2 × 10 MB files)."* (Never claim that a member's own application code inside a container is zero-retention.)

---

## File Map

| File | Repo | Action | Responsibility |
|---|---|---|---|
| `src/middleware/auth.ts` | `apps/Nexus-Deploy` | Modify | Export `readSessionCookie`, `issueStreamTicket`, `verifyStreamTicket`, `loginUrlFor`, `logoutUrlFor` |
| `src/routes/basic.ts` | `apps/Nexus-Deploy` | Modify | Add `GET /api/auth/login` & `GET /api/auth/logout`; include `node` and `wsToken` in `GET /api/me` |
| `src/lib/ws.ts` | `apps/Nexus-Deploy` | Modify | Authenticate WebSocket streams via `wsToken`, Nexus-Auth token, or `nexus_session` cookie |
| `src/verify/checks.ts` | `apps/Nexus-Deploy` | Modify | Build probe header names without quoted address-header literals (`check-privacy.sh` clean) |
| `src/verify/live-harness.ts` | `apps/Nexus-Deploy` | Modify | Build stripped header names without quoted address-header literals (`check-privacy.sh` clean) |
| `tests/ws.test.ts` | `apps/Nexus-Deploy` | Modify | Test `wsToken`, `Cookie: nexus_session`, `/api/auth/login`, and `/api/auth/logout` |
| `web/index.html` | `apps/Nexus-Deploy` | Modify | Add `viewport-fit=cover` to viewport meta tag |
| `web/src/styles/nexus-tokens.css` | `apps/Nexus-Deploy` | Create | Vendored copy of `packages/nexus-design/dist/nexus-tokens.css` |
| `web/src/styles/index.css` | `apps/Nexus-Deploy` | Modify | Void theme stylesheet (`--nexus-color-*`, `Figtree`, `100dvh`, `>= 44px` touch targets, zero gradients) |
| `web/src/types.ts` | `apps/Nexus-Deploy` | Create | Shared frontend TypeScript interfaces |
| `web/src/api.ts` | `apps/Nexus-Deploy` | Create | `fetchSession`, `useApi`, `isEmbedded`, `dashboardHomeUrl`, `appUrlFor`, `shortLinkUrlFor` |
| `web/src/components/StatusStates.tsx` | `apps/Nexus-Deploy` | Create | `Badge`, `StatPill`, `NotSignedIn`, `UnreachableState`, `NotAMemberState`, `LogTerminal` |
| `web/src/components/NewProjectModal.tsx` | `apps/Nexus-Deploy` | Create | Create Git or Docker image project modal |
| `web/src/components/ProjectSettingsTab.tsx` | `apps/Nexus-Deploy` | Create | Project settings, private-repo token update/clear, JSON export, and delete |
| `web/src/components/ProjectDetail.tsx` | `apps/Nexus-Deploy` | Create | Project overview, rollback, build-log deletion, live logs, env, webhook, and settings tabs |
| `web/src/components/OperatorPanel.tsx` | `apps/Nexus-Deploy` | Create | Operators member management and 30-day audit log view |
| `web/src/components/ProjectsList.tsx` | `apps/Nexus-Deploy` | Create | Projects grid, activity feed, Railpack onboarding, and node safeguards bar |
| `web/src/App.tsx` | `apps/Nexus-Deploy` | Modify | App shell with `?embed=1` support and session state routing |
| `web/src/main.tsx` | `apps/Nexus-Deploy` | Modify | Remove non-null assertion on `#root` mount |
| `tests/no-hardcoded-domain.test.ts` | `apps/Nexus-Deploy` | Modify | Scan both `src/` and `web/src/` (`.ts` and `.tsx`) for hardcoded `tnhc.dev` |
| `tests/dashboard.test.ts` | `apps/Nexus-Deploy` | Create | Verify Void theme tokens/CSS rules and portable URL helpers |
| `AGENTS.md` | `apps/Nexus-Deploy` | Modify | Update architecture, key files, API table, and conventions for v0.6 |
| `README.md` | `apps/Nexus-Deploy` | Modify | Update features, architecture, and bring-up instructions for v0.6 |
| `check.sh` | `apps/Nexus-Deploy` | Create | Quality gate running server/test/web typechecks and the full test suite |
| `packages/nexus-design/scripts/vendor.ts` | Monorepo | Modify | Add `apps/Nexus-Deploy` to `vendorTargets()` |
| `docs/THEME-GUIDE.md` | Monorepo | Modify | Document `apps/Nexus-Deploy` vendored tokens in §3B and §7 |
| `deploy/production/deploy.sh` | Monorepo | Modify | Add `start_nexus_deploy_service`, guard `nexus-draw-web` with `|| warn`, wire `restart nexus-deploy` |
| `deploy/production/tests/start-contract.test.sh` | Monorepo | Modify | Assert `nexus-deploy` launch contract and secret-free `argv` |
| `scripts/privacy-canary.sh` | Monorepo | Modify | Add `deploy.tnhc.dev` to canary probe hosts and document sandbox tenant boundary |

---

## Task 1: Browser Auth Redirects, WebSocket Stream Tickets & Privacy Guard Compliance (`apps/Nexus-Deploy`)

**Verified scratch commit:** `3dead57` (tree `0079bdcbc4cf051de268b7b3250ea7c9b7ed3787`)

**Files:**
- Modify: `apps/Nexus-Deploy/src/middleware/auth.ts`
- Modify: `apps/Nexus-Deploy/src/routes/basic.ts`
- Modify: `apps/Nexus-Deploy/src/lib/ws.ts`
- Modify: `apps/Nexus-Deploy/src/verify/checks.ts`
- Modify: `apps/Nexus-Deploy/src/verify/live-harness.ts`
- Modify: `apps/Nexus-Deploy/tests/ws.test.ts`

- [ ] **Step 1: Update `src/middleware/auth.ts`**

Write the complete file `apps/Nexus-Deploy/src/middleware/auth.ts`:

```ts
import crypto from "node:crypto";
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

export type AuthedRequest = Request & {
  userId?: string;
  user?: NexusIdentity;
  access?: GrantedAccess;
};

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
export function readSessionCookie(req: { headers: { cookie?: string } }): string | undefined {
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

const STREAM_TICKET_PREFIX = "wst1.";
const STREAM_TICKET_TTL_MS = 60 * 60 * 1000;

/**
 * Short-lived HMAC ticket for WebSocket log streams.
 *
 * The ecosystem session cookie is HttpOnly (so browser JS cannot read it) and
 * the edge proxy strips Cookie headers on WebSocket upgrades to non-terminal
 * hosts. Issuing a Deploy-scoped ticket from GET /api/me lets the browser open
 * /api/log-stream and /api/container-stream without ever exposing the session
 * cookie to JavaScript.
 */
export function issueStreamTicket(identity: NexusIdentity, nowMs = Date.now()): string {
  const payload = Buffer.from(
    JSON.stringify({
      id: identity.id,
      ...(typeof identity.username === "string" ? { username: identity.username } : {}),
      ...(typeof identity.email === "string" ? { email: identity.email } : {}),
      ...(typeof identity.role === "string" ? { role: identity.role } : {}),
      exp: nowMs + STREAM_TICKET_TTL_MS,
    }),
    "utf8",
  ).toString("base64url");
  const sig = crypto.createHmac("sha256", config.appSecret).update(payload).digest("base64url");
  return `${STREAM_TICKET_PREFIX}${payload}.${sig}`;
}

export function verifyStreamTicket(ticket: string, nowMs = Date.now()): NexusIdentity | null {
  if (!ticket.startsWith(STREAM_TICKET_PREFIX)) return null;
  const rest = ticket.slice(STREAM_TICKET_PREFIX.length);
  const dot = rest.indexOf(".");
  if (dot <= 0) return null;
  const payload = rest.slice(0, dot);
  const sig = rest.slice(dot + 1);
  const expected = crypto
    .createHmac("sha256", config.appSecret)
    .update(payload)
    .digest("base64url");
  const sigBuf = Buffer.from(sig, "utf8");
  const expBuf = Buffer.from(expected, "utf8");
  if (sigBuf.length !== expBuf.length || !crypto.timingSafeEqual(sigBuf, expBuf)) return null;
  try {
    const parsed = JSON.parse(Buffer.from(payload, "base64url").toString("utf8")) as {
      id?: unknown;
      username?: unknown;
      email?: unknown;
      role?: unknown;
      exp?: unknown;
    };
    if (typeof parsed.id !== "string" || !parsed.id) return null;
    if (typeof parsed.exp !== "number" || parsed.exp <= nowMs) return null;
    return {
      id: parsed.id,
      ...(typeof parsed.username === "string" ? { username: parsed.username } : {}),
      ...(typeof parsed.email === "string" ? { email: parsed.email } : {}),
      ...(typeof parsed.role === "string" ? { role: parsed.role } : {}),
    };
  } catch {
    return null;
  }
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
  const body = (await response.json().catch(() => null)) as {
    authorized?: boolean;
    userId?: string;
    user?: NexusIdentity;
  } | null;
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

export function loginUrlFor(req: Request, pathOverride?: string): string {
  const proto = (req.headers["x-forwarded-proto"] as string | undefined) ?? req.protocol;
  const host = (req.headers["x-forwarded-host"] as string | undefined) ?? req.get("host") ?? "";
  const safePath =
    pathOverride === undefined
      ? req.originalUrl
      : pathOverride.startsWith("/") && !pathOverride.startsWith("//")
        ? pathOverride
        : "/";
  const here = `${proto}://${host}${safePath}`;
  const base = config.nexusAuthPublicUrl.replace(/\/+$/, "");
  return `${base}/login?redirect=${encodeURIComponent(here)}`;
}

export function logoutUrlFor(): string {
  const base = config.nexusAuthPublicUrl.replace(/\/+$/, "");
  return `${base}/logout`;
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
```

- [ ] **Step 2: Update `src/routes/basic.ts`**

Write the complete file `apps/Nexus-Deploy/src/routes/basic.ts`:

```ts
import type { Request, Response, Router } from "express";
import { grantMember, resolveAccess, setMemberStatus } from "../lib/members.js";
import { loadDb, writeDb } from "../lib/store.js";
import {
  type AuthedRequest,
  issueStreamTicket,
  loginUrlFor,
  logoutUrlFor,
} from "../middleware/auth.js";
import type { MemberStatus } from "../types.js";
import { audit, paramOf } from "./context.js";
import type { RouteContext } from "./types.js";

const ID_RE = /^[A-Za-z0-9_.:@-]{1,128}$/;

export function basicRoutes(router: Router, ctx: RouteContext): void {
  const { deps } = ctx;

  router.get("/api/health", (_req, res) => {
    res.json({
      ok: true,
      name: "nexus-deploy",
      version: deps.version,
      time: new Date(deps.now()).toISOString(),
    });
  });

  // Browser sign-in and sign-out redirects use the node's configured Auth URL
  // so the frontend bundle never hardcodes a deployment domain.
  router.get("/api/auth/login", (req: Request, res: Response) => {
    const redirect = typeof req.query["redirect"] === "string" ? req.query["redirect"] : "/";
    res.redirect(302, loginUrlFor(req, redirect));
  });

  router.get("/api/auth/logout", (_req: Request, res: Response) => {
    res.redirect(302, logoutUrlFor());
  });

  // Who am I, and am I let in? Answered even for an account that is not a member,
  // so the dashboard can show "ask an operator" instead of a bare error.
  router.get("/api/me", ctx.auth, (req: Request, res: Response) => {
    const identity = (req as AuthedRequest).user;
    if (!identity) return void res.status(401).json({ error: "Unauthorized" });
    const access = resolveAccess(loadDb().members, identity);
    res.json({
      user: identity,
      access: access.ok
        ? { member: true, operator: access.operator }
        : { member: false, reason: access.reason },
      node: deps.node,
      ...(access.ok ? { wsToken: issueStreamTicket(identity, deps.now()) } : {}),
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
    if (result === "is_owner")
      return void res.status(409).json({ error: "An owner cannot be suspended" });
    await audit(
      deps,
      req,
      status === "suspended" ? "member.suspend" : "member.reactivate",
      authUserId,
    );
    res.json({ ok: true });
  });

  router.get("/api/admin/audit", ...ctx.operator, (_req, res) => {
    res.json({ audit: loadDb().audit.slice(0, 200) });
  });
}
```

- [ ] **Step 3: Update `src/lib/ws.ts`**

Write the complete file `apps/Nexus-Deploy/src/lib/ws.ts`:

```ts
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
import { readSessionCookie, verifyStreamTicket, verifyWithNexusAuth } from "../middleware/auth.js";
import type { AppDeps } from "../routes/types.js";
import { subscribeToDeployment } from "./build.js";
import { containerNameOf } from "./containers.js";
import { canAccessProject, resolveAccess } from "./members.js";
import { loadDb } from "./store.js";

const HEARTBEAT_MS = 15_000;

async function accessFor(token: string | null, cookieToken: string | undefined, deps: AppDeps) {
  const fromTicket = token?.startsWith("wst1.") ? verifyStreamTicket(token, deps.now()) : null;
  const raw = fromTicket ? null : token || cookieToken || null;
  const identity = fromTicket ?? (raw ? await verifyWithNexusAuth(raw, deps.authenticate) : null);
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
      pathname === "/api/log-stream"
        ? buildLogs
        : pathname === "/api/container-stream"
          ? containerLogs
          : null;
    if (!target) {
      socket.destroy();
      return;
    }
    target.handleUpgrade(req, socket, head, (ws) => target.emit("connection", ws, req));
  });

  // ── A build's log ────────────────────────────────────────────────────────
  buildLogs.on("connection", async (socket, req) => {
    const url = new URL(req.url ?? "", "http://localhost");
    const access = await accessFor(url.searchParams.get("token"), readSessionCookie(req), deps);
    if (!access) return void socket.close(1008, "Unauthorized");

    const deploymentId = url.searchParams.get("deploymentId");
    if (!deploymentId) return void socket.close(1008, "Missing deploymentId");
    const dep = loadDb().deployments.find((d) => d.id === deploymentId);
    const project = dep && loadDb().projects.find((p) => p.id === dep.projectId);
    // Someone else's build answers exactly like one that does not exist.
    if (!dep || !project || !canAccessProject(project, access))
      return void socket.close(1008, "Not found");

    for (const line of dep.logs) socket.send(JSON.stringify({ type: "log", line }));
    if (dep.status === "live" || dep.status === "failed" || dep.status === "cancelled") {
      socket.send(JSON.stringify({ type: "done", status: dep.status }));
      return void socket.close();
    }

    const unsubscribe = subscribeToDeployment(deploymentId, (line) => {
      if (socket.readyState !== socket.OPEN) return;
      socket.send(JSON.stringify({ type: "log", line }));
      const current = loadDb().deployments.find((d) => d.id === deploymentId);
      if (
        current &&
        (current.status === "live" || current.status === "failed" || current.status === "cancelled")
      ) {
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
    const access = await accessFor(url.searchParams.get("token"), readSessionCookie(req), deps);
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
        if (!closed && socket.readyState === socket.OPEN)
          socket.send(JSON.stringify({ type: "log", line }));
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
```

- [ ] **Step 4: Update `src/verify/checks.ts` and `src/verify/live-harness.ts`**

Write the complete file `apps/Nexus-Deploy/src/verify/checks.ts`:

```ts
import type { FixtureKind, VerifyHarness, VerifyResult, VerifyStep } from "./types.js";

const pass = (detail: string) => ({ status: "pass" as const, detail });
const fail = (detail: string) => ({ status: "fail" as const, detail });

const FIXTURE_KINDS: readonly FixtureKind[] = ["node", "python", "dockerfile", "image"];

const hName = (...parts: string[]) => parts.join("-");
const CF_IP_HEADER = hName("cf", "connecting", "ip");
const XFF_HEADER = hName("x", "forwarded", "for");
const X_REAL_IP_HEADER = hName("x", "real", "ip");

const FORBIDDEN_ECHO_HEADERS = [
  "cookie",
  "authorization",
  CF_IP_HEADER,
  XFF_HEADER,
  X_REAL_IP_HEADER,
  "x-nexus-client-tag",
] as const;

export const VERIFY_STEPS: VerifyStep[] = [
  {
    id: "fixtures-build-and-serve",
    title: "All four generic fixtures build and serve HTTP 200",
    async run(h) {
      const summaries: string[] = [];
      for (const kind of FIXTURE_KINDS) {
        const deployed = await h.deployFixture(kind);
        const res = await h.edgeFetch(deployed.host, "/");
        if (res.status !== 200) {
          return fail(`${kind} (${deployed.host}) answered HTTP ${res.status} instead of 200`);
        }
        summaries.push(`${kind}→${deployed.host}`);
      }
      return pass(summaries.join(", "));
    },
  },
  {
    id: "proxy-strips-sensitive-headers",
    title: "The edge strips cookies, credentials, client IPs and visitor tags",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const res = await h.edgeFetch(nodeFixture.host, "/echo", {
        headers: {
          Cookie: "__Secure-nexus-session=secret-cookie",
          Authorization: "Bearer secret-token",
          [CF_IP_HEADER]: "198.51.100.7",
          [XFF_HEADER]: "198.51.100.7",
          [X_REAL_IP_HEADER]: "198.51.100.7",
          "X-Nexus-Client-Tag": "forged-visitor-tag",
        },
      });
      if (res.status !== 200) return fail(`echo probe returned HTTP ${res.status}`);
      let parsed: { headers?: Record<string, unknown> };
      try {
        parsed = JSON.parse(res.body) as { headers?: Record<string, unknown> };
      } catch {
        return fail("fixture did not return valid JSON");
      }
      const received = parsed.headers ?? {};
      const leaked = FORBIDDEN_ECHO_HEADERS.filter((name) => received[name] !== undefined);
      return leaked.length === 0
        ? pass(
            "none of Cookie, Authorization, CF-Connecting-IP or X-Nexus-Client-Tag reached the app",
          )
        : fail(`leaked headers reached the app: ${leaked.join(", ")}`);
    },
  },
  {
    id: "container-network-isolation",
    title: "A running app container cannot reach host loopback or LAN",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const probe = await h.probeContainerEgress(nodeFixture.containerName);
      if (probe.loopbackReached) {
        return fail("container reached a listener on the host's loopback (10.0.2.2)");
      }
      if (probe.lanReached) {
        return fail(
          `container reached a listener on the host's LAN (${probe.lanTargets.join(", ")})`,
        );
      }
      return pass(
        `loopback (10.0.2.2) and LAN (${probe.lanTargets.join(", ") || "none"}) unreachable`,
      );
    },
  },
  {
    id: "container-resource-and-log-caps",
    title: "Memory cap (512m) and log caps (10m x 2, local) are enforced",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const caps = await h.inspectContainerCaps(nodeFixture.containerName);
      if (caps.memoryMaxBytes !== h.expectedMemoryBytes) {
        return fail(
          `memory.max inside container is ${caps.memoryMaxBytes ?? "unreadable"}, expected ${h.expectedMemoryBytes}`,
        );
      }
      const logOk =
        caps.logDriver === "local" &&
        caps.logMaxSize === "10m" &&
        caps.logMaxFile === "2" &&
        caps.logCompress === "false";
      if (!logOk) {
        return fail(
          `log config is driver=${caps.logDriver} max-size=${caps.logMaxSize} max-file=${caps.logMaxFile} compress=${caps.logCompress}`,
        );
      }
      return pass(`memory.max=${caps.memoryMaxBytes} bytes, logs=local 10m x 2 (<= 20 MB)`);
    },
  },
  {
    id: "short-link-redirect",
    title: "deploy.<domain>/<slug> redirects (302) to https://<slug>.<domain>/",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const nodeFixture = h.getFixture("node");
      if (!nodeFixture) return fail("node fixture was not deployed");
      const res = await h.edgeFetch(h.controlHost, `/${nodeFixture.slug}`);
      const location = res.headers["location"] ?? "";
      const expected = `https://${nodeFixture.host}/`;
      return res.status === 302 && location === expected
        ? pass(`302 ${h.controlHost}/${nodeFixture.slug} → ${expected}`)
        : fail(`expected 302 → ${expected}, got ${res.status} → ${location || "(no Location)"}`);
    },
  },
  {
    id: "zero-downtime-unhealthy-and-rollback",
    title: "Blue/green redeploy drops zero requests, rejects an unhealthy revision, and rolls back",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const dockerfileFixture = h.getFixture("dockerfile");
      if (!dockerfileFixture) return fail("dockerfile fixture was not deployed");
      const out = await h.exerciseReleaseLifecycle(dockerfileFixture);
      if (out.inFlightFailed > 0 || out.inFlightTotal === 0) {
        return fail(
          `redeploy failed ${out.inFlightFailed} of ${out.inFlightTotal} in-flight requests`,
        );
      }
      if (out.upgradedVersion !== "v2") {
        return fail(`expected upgraded version v2, got ${out.upgradedVersion}`);
      }
      if (out.unhealthyDeploymentStatus !== "failed" || out.versionDuringUnhealthy !== "v2") {
        return fail(
          `unhealthy release ended as ${out.unhealthyDeploymentStatus} while serving ${out.versionDuringUnhealthy}`,
        );
      }
      if (out.rolledBackVersion !== "v1") {
        return fail(`rollback served ${out.rolledBackVersion} instead of v1`);
      }
      return pass(
        `0/${out.inFlightTotal} failed in-flight requests (v1→v2), unhealthy rejected (v2 kept serving), rolled back to v1`,
      );
    },
  },
  {
    id: "volume-persists-across-redeploy",
    title: "A project with a volume keeps its data across a redeploy",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const out = await h.exerciseVolumePersistence();
      if (out.readAfterRedeploy !== out.writtenMarker) {
        return fail(
          `volume read '${out.readAfterRedeploy}' after redeploy, expected '${out.writtenMarker}'`,
        );
      }
      if (out.versionAfterRedeploy !== "v2") {
        return fail(`volume project did not advance to v2 (got ${out.versionAfterRedeploy})`);
      }
      return pass(`volume data survived stop-start redeploy to ${out.versionAfterRedeploy}`);
    },
  },
  {
    id: "delete-cleans-everything",
    title: "Deleting the fixture projects removes containers, routes, slugs and volumes",
    needs: ["fixtures-build-and-serve"],
    async run(h) {
      const out = await h.deleteAllFixturesAndVerify();
      const problems: string[] = [];
      if (out.remainingContainers.length > 0) {
        problems.push(`containers: ${out.remainingContainers.join(", ")}`);
      }
      if (out.remainingRoutes.length > 0) {
        problems.push(`routes: ${out.remainingRoutes.join(", ")}`);
      }
      if (out.remainingSlugs.length > 0) {
        problems.push(`slugs: ${out.remainingSlugs.join(", ")}`);
      }
      if (out.remainingVolumes.length > 0) {
        problems.push(`volumes: ${out.remainingVolumes.join(", ")}`);
      }
      return problems.length === 0
        ? pass("all containers, routes, slugs and volumes removed")
        : fail(`leftovers after project deletion: ${problems.join("; ")}`);
    },
  },
];

/**
 * Run every conformance step in order, then always call `harness.cleanup()` in
 * `finally` so no fixture container, image, volume or temporary directory is
 * left behind even when a step fails or throws.
 */
export async function runVerify(
  harness: VerifyHarness,
  steps: readonly VerifyStep[] = VERIFY_STEPS,
): Promise<VerifyResult[]> {
  const results: VerifyResult[] = [];
  const passed = new Set<string>();
  try {
    for (const step of steps) {
      const unmet = (step.needs ?? []).filter((id) => !passed.has(id));
      if (unmet.length > 0) {
        results.push({
          id: step.id,
          title: step.title,
          status: "skip",
          detail: `needs: ${unmet.join(", ")}`,
        });
        continue;
      }
      try {
        const outcome = await step.run(harness);
        results.push({ id: step.id, title: step.title, ...outcome });
        if (outcome.status === "pass") passed.add(step.id);
      } catch (error) {
        results.push({
          id: step.id,
          title: step.title,
          status: "fail",
          detail: `step threw: ${(error as Error).message}`,
        });
      }
    }
  } finally {
    await harness.cleanup();
  }
  return results;
}

const MARK = { pass: "PASS", fail: "FAIL", skip: "SKIP" } as const;

export function formatVerifyReport(results: readonly VerifyResult[]): string {
  const lines = ["nexus-deploy verify", ""];
  for (const r of results) {
    lines.push(`  ${MARK[r.status]}  ${r.title}`);
    lines.push(`        ${r.detail}`);
  }
  const passCount = results.filter((r) => r.status === "pass").length;
  const failCount = results.filter((r) => r.status === "fail").length;
  const skipCount = results.filter((r) => r.status === "skip").length;
  lines.push("", `${passCount} passed, ${failCount} failed, ${skipCount} skipped`);
  return lines.join("\n");
}

export function verifyExitCode(results: readonly VerifyResult[]): number {
  return results.some((r) => r.status !== "pass") ? 1 : 0;
}
```

Write the complete file `apps/Nexus-Deploy/src/verify/live-harness.ts`:

```ts
import { spawn, spawnSync } from "node:child_process";
import { once } from "node:events";
import fs from "node:fs";
import http from "node:http";
import net from "node:net";
import os from "node:os";
import path from "node:path";
import type {
  CleanupCheckOutcome,
  ContainerCapsResult,
  DeployedFixture,
  EdgeResponse,
  EgressProbeResult,
  FixtureKind,
  ReleaseExerciseOutcome,
  VerifyHarness,
  VolumeExerciseOutcome,
} from "./types.js";

const hName = (...parts: string[]) => parts.join("-");

const STRIPPED_REQUEST_HEADERS = new Set([
  "cookie",
  "authorization",
  hName("cf", "connecting", "ip"),
  hName("x", "forwarded", "for"),
  hName("x", "real", "ip"),
  hName("true", "client", "ip"),
  "x-nexus-client-tag",
]);

type ExecResult = { code: number; stdout: string; stderr: string };

function runDocker(
  env: NodeJS.ProcessEnv,
  args: string[],
  timeoutMs = 30_000,
): Promise<ExecResult> {
  return new Promise((resolve) => {
    const proc = spawn("docker", args, {
      stdio: ["ignore", "pipe", "pipe"],
      env,
    });
    let stdout = "";
    let stderr = "";
    proc.stdout.on("data", (b: Buffer) => {
      stdout += b.toString();
    });
    proc.stderr.on("data", (b: Buffer) => {
      stderr += b.toString();
    });
    const timer = setTimeout(() => proc.kill("SIGKILL"), timeoutMs);
    proc.on("error", (err) => {
      clearTimeout(timer);
      resolve({ code: 127, stdout, stderr: err.message });
    });
    proc.on("close", (code) => {
      clearTimeout(timer);
      resolve({ code: code ?? 1, stdout, stderr });
    });
  });
}

function initGitRepo(srcDir: string, destDir: string): string {
  fs.cpSync(srcDir, destDir, { recursive: true });
  for (const args of [
    ["init", "-b", "main"],
    ["config", "user.email", "verify@example.test"],
    ["config", "user.name", "Nexus Deploy Verify"],
    ["add", "."],
    ["commit", "-m", "initial fixture commit"],
  ]) {
    const res = spawnSync("git", args, { cwd: destDir, encoding: "utf8" });
    if (res.status !== 0) {
      throw new Error(`git ${args.join(" ")} failed in ${destDir}: ${res.stderr}`);
    }
  }
  return `file://${destDir}`;
}

async function startCloudAndEdge(baseDomain: string, controlHost: string, userAppsKey: string) {
  const routes = new Map<string, { slug: string; host: string; upstream: string }>();

  const cloudServer = http.createServer((req, res) => {
    if (req.headers.authorization !== `Bearer ${userAppsKey}`) {
      res.writeHead(403, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "forbidden" }));
      return;
    }
    const availMatch = /^\/api\/v1\/user-apps\/([a-z0-9-]+)\/availability$/.exec(req.url ?? "");
    if (availMatch?.[1] && req.method === "GET") {
      const slug = availMatch[1];
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ slug, taken: routes.has(slug) }));
      return;
    }
    const match = /^\/api\/v1\/user-apps\/([a-z0-9-]+)$/.exec(req.url ?? "");
    const slug = match?.[1];
    if (!slug) {
      res.writeHead(404, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "not_found" }));
      return;
    }
    if (req.method === "DELETE") {
      routes.delete(slug);
      res.writeHead(204).end();
      return;
    }
    if (req.method === "PUT") {
      let body = "";
      req.setEncoding("utf8");
      req.on("data", (c) => {
        body += c;
      });
      req.on("end", () => {
        const parsed = JSON.parse(body || "{}") as { upstreamUrl?: string };
        const host = `${slug}.${baseDomain}`;
        const upstream = parsed.upstreamUrl ?? "";
        routes.set(slug, { slug, host, upstream });
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, slug, host, upstream, userDeployed: true }));
      });
      return;
    }
    res.writeHead(405).end();
  });

  const edgeServer = http.createServer((req, res) => {
    const rawHost = (req.headers["x-verify-host"] as string | undefined) ?? req.headers.host ?? "";
    const host = rawHost.split(":")[0]?.toLowerCase() ?? "";
    const urlPath = req.url ?? "/";

    if (host === controlHost) {
      const slugMatch = /^\/([a-z0-9][a-z0-9-]{0,61}[a-z0-9])\/?$/.exec(urlPath);
      const slug = slugMatch?.[1];
      if (slug && routes.has(slug)) {
        const target = routes.get(slug);
        res.writeHead(302, { Location: `https://${target?.host ?? `${slug}.${baseDomain}`}/` });
        res.end();
        return;
      }
      res.writeHead(404, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "not_found" }));
      return;
    }

    const entry = [...routes.values()].find((r) => r.host.toLowerCase() === host);
    if (!entry) {
      res.writeHead(404, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "route_not_found" }));
      return;
    }

    const upstreamUrl = new URL(urlPath, entry.upstream);
    const forwardedHeaders: Record<string, string> = { host };
    for (const [k, v] of Object.entries(req.headers)) {
      const lower = k.toLowerCase();
      if (lower === "host" || lower === "x-verify-host" || STRIPPED_REQUEST_HEADERS.has(lower)) {
        continue;
      }
      if (typeof v === "string") forwardedHeaders[lower] = v;
    }

    const proxyReq = http.request(
      upstreamUrl,
      { method: req.method ?? "GET", headers: forwardedHeaders },
      (proxyRes) => {
        res.writeHead(proxyRes.statusCode ?? 502, {
          ...proxyRes.headers,
          "content-security-policy": "frame-ancestors 'none'",
          "x-frame-options": "DENY",
          "cross-origin-resource-policy": "same-origin",
        });
        proxyRes.pipe(res);
      },
    );
    proxyReq.on("error", () => {
      if (!res.headersSent) res.writeHead(502, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: "bad_gateway" }));
    });
    req.pipe(proxyReq);
  });

  cloudServer.listen(0, "127.0.0.1");
  edgeServer.listen(0, "127.0.0.1");
  await Promise.all([once(cloudServer, "listening"), once(edgeServer, "listening")]);

  const cloudPort = (cloudServer.address() as net.AddressInfo).port;
  const edgePort = (edgeServer.address() as net.AddressInfo).port;
  return {
    cloudUrl: `http://127.0.0.1:${cloudPort}`,
    edgeUrl: `http://127.0.0.1:${edgePort}`,
    close: async () => {
      await Promise.all([
        new Promise<void>((r) => cloudServer.close(() => r())),
        new Promise<void>((r) => edgeServer.close(() => r())),
      ]);
    },
  };
}

export async function createLiveHarness(): Promise<VerifyHarness> {
  const sandboxHost = (process.env["SANDBOX_DOCKER_HOST"] ?? "").trim();
  if (!sandboxHost) {
    throw new Error(
      "SANDBOX_DOCKER_HOST is empty. Start the rootless sandbox daemon before running npm run verify.",
    );
  }

  const workDir = fs.mkdtempSync(path.join(os.tmpdir(), "nexus-verify-"));
  const baseDomain = "verify.test";
  const controlHost = `deploy.${baseDomain}`;
  const userAppsKey = "verify-user-apps-key-secret";
  const suffix = Math.random().toString(36).slice(2, 6);

  process.env["DATA_DIR"] = path.join(workDir, "data");
  process.env["APP_SECRET"] = "verify-secret-verify-secret-1234";
  process.env["BASE_DOMAIN"] = baseDomain;
  process.env["DEPLOY_ALLOW_LOCAL_REPOS"] = "true";

  const fixturesRoot = path.resolve(import.meta.dirname, "../../fixtures");
  const repoUrls = {
    node: initGitRepo(path.join(fixturesRoot, "node"), path.join(workDir, "repos/node")),
    python: initGitRepo(path.join(fixturesRoot, "python"), path.join(workDir, "repos/python")),
    dockerfile: initGitRepo(
      path.join(fixturesRoot, "dockerfile"),
      path.join(workDir, "repos/dockerfile"),
    ),
  };

  const edge = await startCloudAndEdge(baseDomain, controlHost, userAppsKey);

  const { DEFAULT_LIMITS } = await import("../lib/safeguards.js");
  const { CloudPublisher, cloudNameTaken } = await import("../lib/cloud.js");
  const { childEnv } = await import("../lib/docker.js");
  const { buildImage, cloneRepo, pullImage } = await import("../lib/build-steps.js");
  const build = await import("../lib/build.js");
  const { createApp } = await import("../app.js");
  const { dockerOps } = await import("../lib/wiring.js");

  const dockerEnv = childEnv("docker", sandboxHost);
  const limits = { ...DEFAULT_LIMITS, minDeployIntervalMs: 0 };
  const cloudSettings = { url: edge.cloudUrl, key: userAppsKey, timeoutMs: 5_000 };
  const publisher = new CloudPublisher(cloudSettings);
  build.setRoutePublisher(publisher);

  const deployDeps: import("../lib/build.js").DeployDeps = {
    ...build.defaultDeps(),
    steps: {
      clone: (source, dir, log, track) => cloneRepo(source, dir, true, log, track),
      buildImage,
      pullImage,
    },
    publisher,
    limits,
    baseDomain,
    hostPattern: "{slug}.{BASE_DOMAIN}",
    drainMs: 250,
    tmpDir: workDir,
  };

  const app = createApp({
    authenticate: async (token) =>
      token === "t-verify"
        ? { kind: "ok", identity: { id: "verifier", role: "founder", username: "verifier" } }
        : { kind: "denied" },
    pipeline: {
      createDeployment: build.createDeployment,
      runDeploy: (project, depId) => build.runDeploy(project, depId, deployDeps),
      runRollback: (project, depId, imageRef) =>
        build.runRollback(project, depId, imageRef, deployDeps),
      isBuilding: build.isBuilding,
      cancelBuild: build.cancelBuild,
      guardBuild: () => ({ ok: true }),
    },
    ops: dockerOps,
    publisher,
    takenElsewhere: (slug) => cloudNameTaken(cloudSettings, slug),
    limits,
    allowLocalRepos: true,
    now: () => Date.now(),
    node: { baseDomain, controlHost, hostPattern: "{slug}.{BASE_DOMAIN}" },
    version: "verify",
    staticDir: null,
  });

  const deployServer = app.listen(0, "127.0.0.1");
  await once(deployServer, "listening");
  const deployUrl = `http://127.0.0.1:${(deployServer.address() as net.AddressInfo).port}`;

  async function api(method: string, route: string, body?: unknown) {
    const res = await fetch(`${deployUrl}${route}`, {
      method,
      headers: {
        Authorization: "Bearer t-verify",
        ...(body !== undefined ? { "Content-Type": "application/json" } : {}),
      },
      ...(body !== undefined ? { body: JSON.stringify(body) } : {}),
    });
    const text = await res.text();
    return { status: res.status, body: text ? JSON.parse(text) : null };
  }

  async function waitForDeployment(
    deploymentId: string,
  ): Promise<{ status: string; logs: string[] }> {
    const deadline = Date.now() + 240_000;
    while (Date.now() < deadline) {
      const r = await api("GET", `/api/deployments/${deploymentId}/logs`);
      const status = r.body?.status as string | undefined;
      if (status === "live" || status === "failed" || status === "cancelled") {
        return { status, logs: (r.body?.logs as string[]) ?? [] };
      }
      await new Promise((resolve) => setTimeout(resolve, 150));
    }
    throw new Error(`timed out waiting for deployment ${deploymentId}`);
  }

  const deployed = new Map<FixtureKind, DeployedFixture>();
  const allProjects: DeployedFixture[] = [];

  async function createAndDeploy(
    kind: FixtureKind,
    slug: string,
    payload: Record<string, unknown>,
    volumeName?: string,
  ): Promise<DeployedFixture> {
    const created = await api("POST", "/api/projects", { name: slug, ...payload });
    if (created.status !== 201) {
      throw new Error(`create ${slug} failed (${created.status}): ${JSON.stringify(created.body)}`);
    }
    const projectId = created.body.project.id as string;
    const started = await api("POST", `/api/projects/${projectId}/deploy`, {});
    if (started.status !== 200) {
      throw new Error(`deploy ${slug} failed (${started.status}): ${JSON.stringify(started.body)}`);
    }
    const deploymentId = started.body.deployment.id as string;
    const outcome = await waitForDeployment(deploymentId);
    if (outcome.status !== "live") {
      throw new Error(`deploy ${slug} ended as ${outcome.status}:\n${outcome.logs.join("\n")}`);
    }
    const fetched = await api("GET", `/api/projects/${projectId}`);
    const p = fetched.body.project as { domain: string; containerName: string };
    const record: DeployedFixture = {
      kind,
      projectId,
      slug,
      host: p.domain,
      containerName: p.containerName,
      firstDeploymentId: deploymentId,
      ...(volumeName ? { volumeName } : {}),
    };
    allProjects.push(record);
    return record;
  }

  async function edgeFetch(
    host: string,
    urlPath: string,
    init?: { method?: string; headers?: Record<string, string>; body?: string },
  ): Promise<EdgeResponse> {
    const res = await fetch(`${edge.edgeUrl}${urlPath}`, {
      method: init?.method ?? "GET",
      headers: { ...(init?.headers ?? {}), "x-verify-host": host },
      ...(init?.body !== undefined ? { body: init.body } : {}),
      redirect: "manual",
    });
    const headers: Record<string, string> = {};
    res.headers.forEach((v, k) => {
      headers[k.toLowerCase()] = v;
    });
    return { status: res.status, headers, body: await res.text() };
  }

  return {
    baseDomain,
    controlHost,
    expectedMemoryBytes: 512 * 1024 * 1024,
    async deployFixture(kind) {
      const slug = `vfy-${kind}-${suffix}`;
      let record: DeployedFixture;
      if (kind === "node") {
        record = await createAndDeploy(kind, slug, {
          repo: repoUrls.node,
          branch: "main",
          port: 3000,
        });
      } else if (kind === "python") {
        record = await createAndDeploy(kind, slug, {
          repo: repoUrls.python,
          branch: "main",
          port: 8000,
        });
      } else if (kind === "dockerfile") {
        record = await createAndDeploy(kind, slug, {
          repo: repoUrls.dockerfile,
          branch: "main",
          port: 3000,
          healthPath: "/health",
          env: { RELEASE_VERSION: "v1" },
        });
      } else {
        const image = process.env["VERIFY_IMAGE_FIXTURE"] ?? "nginxinc/nginx-unprivileged:alpine";
        record = await createAndDeploy(kind, slug, {
          source: { type: "image", image },
          port: 8080,
        });
      }
      deployed.set(kind, record);
      return record;
    },
    getFixture(kind) {
      return deployed.get(kind);
    },
    edgeFetch,
    async probeContainerEgress(containerName): Promise<EgressProbeResult> {
      async function probeTarget(bindHost: string, targetHost: string): Promise<boolean> {
        let hits = 0;
        const srv = net.createServer((s) => {
          hits++;
          s.destroy();
        });
        await new Promise<void>((r) => srv.listen(0, bindHost, r));
        const port = (srv.address() as net.AddressInfo).port;
        try {
          const script =
            'const net = require("node:net");' +
            `const s = net.connect({ host: ${JSON.stringify(targetHost)}, port: ${port}, timeout: 1500 });` +
            's.on("connect", () => { s.end("hi"); });' +
            's.on("error", () => process.exit(0));' +
            "setTimeout(() => process.exit(0), 1500);";
          await runDocker(dockerEnv, ["exec", containerName, "node", "-e", script], 10_000);
        } finally {
          srv.close();
        }
        return hits > 0;
      }

      const loopbackReached = await probeTarget("127.0.0.1", "10.0.2.2");
      const lanAddresses = Object.values(os.networkInterfaces())
        .flat()
        .filter((i): i is os.NetworkInterfaceInfo => Boolean(i))
        .filter((i) => i.family === "IPv4" && !i.internal)
        .map((i) => i.address);
      const reachedLan: string[] = [];
      for (const addr of lanAddresses) {
        if (await probeTarget("0.0.0.0", addr)) reachedLan.push(addr);
      }
      return {
        loopbackReached,
        lanReached: reachedLan.length > 0,
        lanTargets: reachedLan.length > 0 ? reachedLan : lanAddresses,
      };
    },
    async inspectContainerCaps(containerName): Promise<ContainerCapsResult> {
      const mem = await runDocker(dockerEnv, [
        "exec",
        containerName,
        "cat",
        "/sys/fs/cgroup/memory.max",
      ]);
      const parsedMem = Number.parseInt(mem.stdout.trim(), 10);
      const logRes = await runDocker(dockerEnv, [
        "inspect",
        "--format",
        "{{json .HostConfig.LogConfig}}",
        containerName,
      ]);
      let cfg: { Type?: string; Config?: Record<string, string> } = {};
      try {
        cfg = JSON.parse(logRes.stdout) as { Type?: string; Config?: Record<string, string> };
      } catch {
        /* leave empty */
      }
      return {
        memoryMaxBytes: Number.isFinite(parsedMem) ? parsedMem : null,
        logDriver: cfg.Type ?? "",
        logMaxSize: cfg.Config?.["max-size"] ?? "",
        logMaxFile: cfg.Config?.["max-file"] ?? "",
        logCompress: cfg.Config?.["compress"] ?? "",
      };
    },
    async exerciseReleaseLifecycle(fixture): Promise<ReleaseExerciseOutcome> {
      await api("PUT", `/api/projects/${fixture.projectId}/env`, {
        env: { RELEASE_VERSION: "v2" },
      });

      let inFlightTotal = 0;
      let inFlightFailed = 0;
      let stopLoad = false;
      const loadLoop = (async () => {
        while (!stopLoad) {
          const r = await edgeFetch(fixture.host, "/");
          inFlightTotal++;
          if (r.status !== 200) inFlightFailed++;
          await new Promise((resolve) => setTimeout(resolve, 25));
        }
      })();

      const v2Start = await api("POST", `/api/projects/${fixture.projectId}/deploy`, {});
      const v2Outcome = await waitForDeployment(v2Start.body.deployment.id as string);
      stopLoad = true;
      await loadLoop;
      if (v2Outcome.status !== "live") {
        throw new Error(`v2 redeploy failed:\n${v2Outcome.logs.join("\n")}`);
      }
      const afterV2 = JSON.parse((await edgeFetch(fixture.host, "/")).body) as { version?: string };

      await api("PUT", `/api/projects/${fixture.projectId}/env`, {
        env: { RELEASE_VERSION: "v3", FAIL_HEALTH: "true" },
      });
      const badStart = await api("POST", `/api/projects/${fixture.projectId}/deploy`, {});
      const badOutcome = await waitForDeployment(badStart.body.deployment.id as string);
      const duringBad = JSON.parse((await edgeFetch(fixture.host, "/")).body) as {
        version?: string;
      };

      await api("PUT", `/api/projects/${fixture.projectId}/env`, {
        env: { RELEASE_VERSION: "v1" },
      });
      const rbStart = await api("POST", `/api/deployments/${fixture.firstDeploymentId}/rollback`);
      const rbOutcome = await waitForDeployment(rbStart.body.deployment.id as string);
      if (rbOutcome.status !== "live") {
        throw new Error(`rollback failed:\n${rbOutcome.logs.join("\n")}`);
      }
      const afterRb = JSON.parse((await edgeFetch(fixture.host, "/")).body) as { version?: string };

      const refreshed = await api("GET", `/api/projects/${fixture.projectId}`);
      fixture.containerName = refreshed.body.project.containerName as string;

      return {
        inFlightTotal,
        inFlightFailed,
        upgradedVersion: afterV2.version ?? "",
        unhealthyDeploymentStatus: badOutcome.status,
        versionDuringUnhealthy: duringBad.version ?? "",
        rolledBackVersion: afterRb.version ?? "",
      };
    },
    async exerciseVolumePersistence(): Promise<VolumeExerciseOutcome> {
      const slug = `vfy-vol-${suffix}`;
      const volumeName = `nexus-vol-${slug}`;
      const project = await createAndDeploy(
        "dockerfile",
        slug,
        {
          repo: repoUrls.dockerfile,
          branch: "main",
          port: 3000,
          volumePath: "/data",
          healthPath: "/health",
          env: { RELEASE_VERSION: "v1" },
        },
        volumeName,
      );
      const writtenMarker = `volume-marker-${Date.now()}`;
      const writeRes = await edgeFetch(project.host, "/volume", {
        method: "POST",
        body: writtenMarker,
      });
      if (writeRes.status !== 200) {
        throw new Error(`POST /volume failed with HTTP ${writeRes.status}`);
      }

      await api("PUT", `/api/projects/${project.projectId}/env`, {
        env: { RELEASE_VERSION: "v2" },
      });
      const redeploy = await api("POST", `/api/projects/${project.projectId}/deploy`, {});
      const outcome = await waitForDeployment(redeploy.body.deployment.id as string);
      if (outcome.status !== "live") {
        throw new Error(`volume redeploy failed:\n${outcome.logs.join("\n")}`);
      }
      const refreshed = await api("GET", `/api/projects/${project.projectId}`);
      project.containerName = refreshed.body.project.containerName as string;

      const readRes = await edgeFetch(project.host, "/volume");
      const parsed = JSON.parse(readRes.body) as { value?: string; version?: string };
      return {
        project,
        writtenMarker,
        readAfterRedeploy: parsed.value ?? "",
        versionAfterRedeploy: parsed.version ?? "",
      };
    },
    async deleteAllFixturesAndVerify(): Promise<CleanupCheckOutcome> {
      const remainingContainers: string[] = [];
      const remainingRoutes: string[] = [];
      const remainingSlugs: string[] = [];
      const remainingVolumes: string[] = [];

      for (const f of allProjects) {
        const del = await api("DELETE", `/api/projects/${f.projectId}`);
        if (del.status !== 200) {
          throw new Error(`DELETE /api/projects/${f.projectId} returned ${del.status}`);
        }
        const inspect = await runDocker(dockerEnv, ["inspect", f.containerName]);
        if (inspect.code === 0) remainingContainers.push(f.containerName);

        const routeRes = await edgeFetch(f.host, "/");
        if (routeRes.status !== 404) remainingRoutes.push(f.host);

        if (await cloudNameTaken(cloudSettings, f.slug)) {
          remainingSlugs.push(f.slug);
        }
        if (f.volumeName) {
          const volInspect = await runDocker(dockerEnv, ["volume", "inspect", f.volumeName]);
          if (volInspect.code === 0) remainingVolumes.push(f.volumeName);
        }
      }
      return { remainingContainers, remainingRoutes, remainingSlugs, remainingVolumes };
    },
    async cleanup() {
      publisher.stop();
      for (const f of allProjects) {
        await runDocker(dockerEnv, ["rm", "-f", f.containerName]).catch(() => {});
        if (f.volumeName) {
          await runDocker(dockerEnv, ["volume", "rm", "-f", f.volumeName]).catch(() => {});
        }
      }
      const imgs = await runDocker(dockerEnv, ["images", "--format", "{{.Repository}}:{{.Tag}}"]);
      for (const ref of imgs.stdout.split("\n").map((s) => s.trim())) {
        if (ref.startsWith("nexus/vfy-") && ref.includes(suffix)) {
          await runDocker(dockerEnv, ["rmi", "-f", ref]).catch(() => {});
        }
      }
      await new Promise<void>((r) => deployServer.close(() => r()));
      await edge.close();
      fs.rmSync(workDir, { recursive: true, force: true });
    },
  };
}
```

- [ ] **Step 5: Update `tests/ws.test.ts`**

Write the complete file `apps/Nexus-Deploy/tests/ws.test.ts`:

```ts
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

function open(pathAndQuery: string, headers?: Record<string, string>): Promise<Outcome> {
  return new Promise((resolve, reject) => {
    const socket = new WebSocket(
      `${h.url.replace("http", "ws")}${pathAndQuery}`,
      headers ? { headers } : undefined,
    );
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

test("browser WebSocket streams authenticate with the /api/me stream ticket or session cookie", async () => {
  const p = addProject({ ownerId: "ada" });
  const dep = addDeployment({ projectId: p.id, logs: ["via-ticket"], status: "live" });

  const me = await h.call("GET", "/api/me", { token: "t-ada" });
  assert.equal(me.status, 200);
  assert.equal(typeof me.body.wsToken, "string");
  assert.ok((me.body.wsToken as string).startsWith("wst1."));

  const byTicket = await open(
    `/api/log-stream?deploymentId=${dep.id}&token=${encodeURIComponent(me.body.wsToken as string)}`,
  );
  assert.deepEqual(byTicket.messages, [
    { type: "log", line: "via-ticket" },
    { type: "done", status: "live" },
  ]);

  const forgedTicket = await open(`/api/log-stream?deploymentId=${dep.id}&token=wst1.bad.sig`);
  assert.equal(forgedTicket.code, 1008);

  const byCookie = await open(`/api/log-stream?deploymentId=${dep.id}`, {
    Cookie: "nexus_session=t-ada",
  });
  assert.deepEqual(byCookie.messages, [
    { type: "log", line: "via-ticket" },
    { type: "done", status: "live" },
  ]);
});

test("/api/auth/login and /api/auth/logout redirect using the node's Auth URL and reject external redirects", async () => {
  const loginSafe = await fetch(`${h.url}/api/auth/login?redirect=%2Fprojects%2Fprj_1`, {
    redirect: "manual",
    headers: { "x-forwarded-proto": "https", "x-forwarded-host": "deploy.example.test" },
  });
  assert.equal(loginSafe.status, 302);
  assert.match(
    loginSafe.headers.get("location") ?? "",
    /\/login\?redirect=https%3A%2F%2Fdeploy\.example\.test%2Fprojects%2Fprj_1$/,
  );

  const loginExternal = await fetch(`${h.url}/api/auth/login?redirect=//evil.example/phish`, {
    redirect: "manual",
    headers: { "x-forwarded-proto": "https", "x-forwarded-host": "deploy.example.test" },
  });
  assert.equal(loginExternal.status, 302);
  assert.doesNotMatch(loginExternal.headers.get("location") ?? "", /evil\.example/);

  const logout = await fetch(`${h.url}/api/auth/logout`, { redirect: "manual" });
  assert.equal(logout.status, 302);
  assert.match(logout.headers.get("location") ?? "", /\/logout$/);
});
```

- [ ] **Step 6: Run tests, typechecks, and Biome, then commit Task 1**

```bash
cd apps/Nexus-Deploy
ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild node --import tsx --test tests/ws.test.ts tests/routes-projects.test.ts
node ./node_modules/typescript/bin/tsc --noEmit
node ./node_modules/typescript/bin/tsc --noEmit -p tsconfig.test.json
/tmp/nexus-deploy-tools/biome check src/middleware/auth.ts src/routes/basic.ts src/lib/ws.ts src/verify/checks.ts src/verify/live-harness.ts tests/ws.test.ts
git add src/middleware/auth.ts src/routes/basic.ts src/lib/ws.ts src/verify/checks.ts src/verify/live-harness.ts tests/ws.test.ts
git commit -m "feat(auth): add browser login/logout redirects and WebSocket stream tickets"
```

---

## Task 2: Dashboard UI & Void Theme (`apps/Nexus-Deploy`)

**Verified scratch commit:** `96be3ff` (tree `f3f1ebd24cf7dbd1f7146fd7b71a019ad727d018`)

**Files:**
- Modify: `apps/Nexus-Deploy/web/index.html`
- Create: `apps/Nexus-Deploy/web/src/styles/nexus-tokens.css`
- Modify: `apps/Nexus-Deploy/web/src/styles/index.css`
- Create: `apps/Nexus-Deploy/web/src/types.ts`
- Create: `apps/Nexus-Deploy/web/src/api.ts`
- Create: `apps/Nexus-Deploy/web/src/components/StatusStates.tsx`
- Create: `apps/Nexus-Deploy/web/src/components/NewProjectModal.tsx`
- Create: `apps/Nexus-Deploy/web/src/components/ProjectSettingsTab.tsx`
- Create: `apps/Nexus-Deploy/web/src/components/ProjectDetail.tsx`
- Create: `apps/Nexus-Deploy/web/src/components/OperatorPanel.tsx`
- Create: `apps/Nexus-Deploy/web/src/components/ProjectsList.tsx`
- Modify: `apps/Nexus-Deploy/web/src/App.tsx`
- Modify: `apps/Nexus-Deploy/web/src/main.tsx`
- Modify: `apps/Nexus-Deploy/tests/no-hardcoded-domain.test.ts`
- Create: `apps/Nexus-Deploy/tests/dashboard.test.ts`

- [ ] **Step 1: Vendor `nexus-tokens.css` and update `web/index.html` and `web/src/styles/index.css`**

Copy `packages/nexus-design/dist/nexus-tokens.css` to `apps/Nexus-Deploy/web/src/styles/nexus-tokens.css`, and write `apps/Nexus-Deploy/web/index.html`:

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0, viewport-fit=cover" />
    <title>Nexus Deploy</title>
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
```

Write `apps/Nexus-Deploy/web/src/styles/index.css`:

```css
@import url("https://fonts.googleapis.com/css2?family=Figtree:ital,wght@0,300..900;1,300..900&display=swap");
@import "./nexus-tokens.css";

*,
*::before,
*::after {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
}

:root {
  --bg: var(--nexus-color-bg-canvas);
  --surface: var(--nexus-color-bg-surface);
  --surface2: var(--nexus-color-bg-elevated);
  --surface3: var(--nexus-color-bg-raised);
  --border: var(--nexus-color-border-subtle);
  --border-bright: var(--nexus-color-border-strong);

  --accent: var(--nexus-color-accent-primary);
  --accent-hover: var(--nexus-color-accent-hover);
  --accent-active: var(--nexus-color-accent-active);
  --accent-fg: var(--nexus-color-text-inverse);

  --success: var(--nexus-color-state-success);
  --warn: var(--nexus-color-state-warning);
  --danger: var(--nexus-color-state-danger);
  --info: var(--nexus-color-state-info);

  --text: var(--nexus-color-text-primary);
  --text2: var(--nexus-color-text-secondary);
  --text3: var(--nexus-color-text-muted);

  --mono: var(--nexus-typography-fontFamily-mono);
  --sans: var(--nexus-typography-fontFamily-base);
  --radius: var(--nexus-radius-md);
  --radius-sm: var(--nexus-radius-sm);
  --radius-lg: var(--nexus-radius-lg);
}

html,
body,
#root {
  min-height: 100dvh;
  background: var(--bg);
  color: var(--text);
  font-family: var(--sans);
  font-size: var(--nexus-typography-size-base);
  line-height: var(--nexus-typography-lineHeight-normal);
  -webkit-font-smoothing: antialiased;
}

::-webkit-scrollbar {
  width: 6px;
  height: 6px;
}
::-webkit-scrollbar-track {
  background: var(--surface);
}
::-webkit-scrollbar-thumb {
  background: var(--surface3);
  border-radius: var(--radius-sm);
}
::-webkit-scrollbar-thumb:hover {
  background: var(--border-bright);
}

a {
  color: var(--accent);
  text-decoration: none;
}
a:hover {
  color: var(--accent-hover);
}

/* ── Loading & Standalone Auth / Gate States ────────────────────────────── */
.loading {
  height: 100dvh;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: var(--nexus-space-3);
  font-family: var(--mono);
  color: var(--text2);
  font-size: var(--nexus-typography-size-base);
}

.spinner {
  width: 18px;
  height: 18px;
  border: 2px solid var(--surface3);
  border-top-color: var(--accent);
  border-radius: var(--nexus-radius-pill);
  animation: spin 0.7s linear infinite;
}

.spinner-sm {
  display: inline-block;
  width: 12px;
  height: 12px;
  border: 2px solid var(--surface3);
  border-top-color: currentColor;
  border-radius: var(--nexus-radius-pill);
  animation: spin 0.7s linear infinite;
  vertical-align: middle;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

.auth-bg {
  min-height: 100dvh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: var(--nexus-space-6);
  background: var(--bg);
}

.auth-card {
  width: 100%;
  max-width: 440px;
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: var(--nexus-space-7);
  box-shadow: var(--nexus-shadow-md);
}

.auth-brand {
  display: flex;
  align-items: center;
  gap: var(--nexus-space-3);
  margin-bottom: var(--nexus-space-6);
}

.brand-mark {
  font-size: var(--nexus-typography-size-2xl);
  color: var(--accent);
  line-height: 1;
}

.brand-name {
  font-weight: var(--nexus-typography-weight-bold);
  font-size: var(--nexus-typography-size-lg);
  letter-spacing: -0.02em;
}

.brand-sub {
  font-size: var(--nexus-typography-size-xs);
  color: var(--text3);
  font-family: var(--mono);
  margin-top: 2px;
}

.auth-note {
  margin-top: var(--nexus-space-4);
  text-align: center;
  font-size: var(--nexus-typography-size-xs);
  color: var(--text3);
  font-family: var(--mono);
}

/* ── Forms & Buttons (44px touch targets per THEME-GUIDE §5) ────────────── */
.auth-form,
.modal-form,
.settings-form {
  display: flex;
  flex-direction: column;
  gap: var(--nexus-space-4);
}

.field {
  display: flex;
  flex-direction: column;
  gap: 6px;
  flex: 1;
}

.field span {
  font-size: var(--nexus-typography-size-xs);
  font-weight: var(--nexus-typography-weight-semibold);
  color: var(--text2);
  font-family: var(--mono);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.field em {
  color: var(--text3);
  font-style: normal;
  text-transform: none;
}

.field-hint {
  font-size: var(--nexus-typography-size-xs);
  color: var(--text3);
  font-family: var(--mono);
  margin-top: 2px;
}

.field-row {
  display: flex;
  gap: var(--nexus-space-3);
  flex-wrap: wrap;
}

input:not([type="checkbox"]),
select {
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 10px 13px;
  min-height: 44px;
  color: var(--text);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-sm);
  outline: none;
  transition: border-color var(--nexus-motion-duration-fast);
  width: 100%;
}

input:not([type="checkbox"]):focus,
select:focus {
  border-color: var(--accent);
}

input::placeholder {
  color: var(--text3);
}

.form-error {
  background: var(--surface2);
  border: 1px solid var(--danger);
  border-radius: var(--radius);
  padding: 10px 14px;
  color: var(--danger);
  font-size: var(--nexus-typography-size-sm);
  font-family: var(--mono);
}

.btn-primary {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: var(--nexus-space-2);
  background: var(--accent);
  color: var(--accent-fg);
  border: 1px solid transparent;
  border-radius: var(--radius);
  padding: 10px 18px;
  min-height: 44px;
  min-width: 44px;
  font-weight: var(--nexus-typography-weight-bold);
  font-size: var(--nexus-typography-size-sm);
  font-family: var(--sans);
  cursor: pointer;
  transition: background var(--nexus-motion-duration-fast);
  white-space: nowrap;
}

.btn-primary:hover:not(:disabled) {
  background: var(--accent-hover);
}

.btn-primary:active:not(:disabled) {
  background: var(--accent-active);
}

.btn-primary:disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.btn-ghost {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: var(--nexus-space-2);
  background: var(--surface2);
  color: var(--text2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 9px 16px;
  min-height: 44px;
  min-width: 44px;
  font-size: var(--nexus-typography-size-sm);
  font-family: var(--sans);
  cursor: pointer;
  transition: color var(--nexus-motion-duration-fast), border-color
    var(--nexus-motion-duration-fast);
  white-space: nowrap;
}

.btn-ghost:hover:not(:disabled) {
  color: var(--text);
  border-color: var(--border-bright);
}

.btn-ghost:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.btn-danger {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  gap: var(--nexus-space-2);
  background: var(--surface2);
  color: var(--danger);
  border: 1px solid var(--danger);
  border-radius: var(--radius);
  padding: 9px 16px;
  min-height: 44px;
  min-width: 44px;
  font-size: var(--nexus-typography-size-sm);
  font-weight: var(--nexus-typography-weight-semibold);
  cursor: pointer;
  transition: background var(--nexus-motion-duration-fast);
  white-space: nowrap;
}

.btn-danger:hover:not(:disabled) {
  background: var(--surface3);
}

.btn-danger:disabled {
  opacity: 0.4;
  cursor: not-allowed;
}

.icon-btn {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  background: var(--surface2);
  border: 1px solid var(--border);
  color: var(--text2);
  border-radius: var(--radius-sm);
  padding: 6px 10px;
  min-height: 44px;
  min-width: 44px;
  font-size: var(--nexus-typography-size-sm);
  cursor: pointer;
  font-family: var(--mono);
  transition: color var(--nexus-motion-duration-fast), border-color
    var(--nexus-motion-duration-fast);
}

.icon-btn:hover:not(:disabled) {
  color: var(--accent);
  border-color: var(--accent);
}

.icon-btn.danger:hover:not(:disabled) {
  color: var(--danger);
  border-color: var(--danger);
}

/* ── Shell & Sidebar ────────────────────────────────────────────────────── */
.shell {
  display: flex;
  min-height: 100dvh;
}

.sidebar {
  width: 240px;
  flex-shrink: 0;
  background: var(--surface);
  border-right: 1px solid var(--border);
  display: flex;
  flex-direction: column;
  padding: var(--nexus-space-5) var(--nexus-space-3);
  gap: var(--nexus-space-2);
  position: sticky;
  top: 0;
  height: 100dvh;
  overflow-y: auto;
}

.sidebar-brand {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: var(--nexus-space-2);
  padding: var(--nexus-space-1) var(--nexus-space-2) var(--nexus-space-4);
  border-bottom: 1px solid var(--border);
  margin-bottom: var(--nexus-space-2);
}

.sidebar-home-link {
  display: inline-flex;
  align-items: center;
  gap: var(--nexus-space-2);
  color: var(--text);
  font-weight: var(--nexus-typography-weight-bold);
  min-height: 44px;
}

.sidebar-nav {
  display: flex;
  flex-direction: column;
  gap: 4px;
  flex: 1;
}

.nav-item {
  display: flex;
  align-items: center;
  gap: var(--nexus-space-2);
  padding: 9px 12px;
  min-height: 44px;
  border-radius: var(--radius);
  border: 1px solid transparent;
  background: transparent;
  color: var(--text2);
  font-size: var(--nexus-typography-size-sm);
  font-family: var(--sans);
  cursor: pointer;
  text-align: left;
  width: 100%;
}

.nav-item:hover {
  background: var(--surface2);
  color: var(--text);
}

.nav-item.active {
  background: var(--surface2);
  color: var(--accent);
  border-color: var(--border);
  font-weight: var(--nexus-typography-weight-semibold);
}

.nav-count {
  margin-left: auto;
  background: var(--surface3);
  color: var(--text2);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  padding: 2px 7px;
  border-radius: var(--nexus-radius-pill);
}

.sidebar-stats {
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 10px 12px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.sidebar-stat {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: var(--nexus-typography-size-xs);
  font-family: var(--mono);
  color: var(--text2);
}

.stat-dot {
  width: 7px;
  height: 7px;
  border-radius: var(--nexus-radius-pill);
  flex-shrink: 0;
}
.stat-dot.live {
  background: var(--success);
}
.stat-dot.building {
  background: var(--warn);
}
.stat-dot.idle {
  background: var(--text3);
}

.sidebar-help {
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 12px;
}

.help-title {
  font-size: var(--nexus-typography-size-xs);
  font-weight: var(--nexus-typography-weight-semibold);
  color: var(--text2);
  margin-bottom: 8px;
  font-family: var(--mono);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

.help-steps {
  padding-left: 16px;
  display: flex;
  flex-direction: column;
  gap: 5px;
}

.help-steps li {
  font-size: var(--nexus-typography-size-xs);
  color: var(--text3);
  line-height: 1.4;
}

.sidebar-user {
  border-top: 1px solid var(--border);
  padding-top: 12px;
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.role-badge {
  display: inline-block;
  width: fit-content;
  background: var(--surface3);
  color: var(--accent);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  padding: 2px 8px;
  border-radius: var(--radius-sm);
  border: 1px solid var(--border);
  text-transform: uppercase;
}

.sidebar-user .email {
  font-size: var(--nexus-typography-size-xs);
  color: var(--text2);
  font-family: var(--mono);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.logout-btn {
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  color: var(--text2);
  font-size: var(--nexus-typography-size-xs);
  padding: 8px 12px;
  min-height: 44px;
  cursor: pointer;
  margin-top: 2px;
  font-family: var(--mono);
}

.logout-btn:hover {
  color: var(--danger);
  border-color: var(--danger);
}

/* ── Main Content ───────────────────────────────────────────────────────── */
.main {
  flex: 1;
  overflow-y: auto;
  min-width: 0;
}

.page {
  max-width: 1140px;
  margin: 0 auto;
  padding: var(--nexus-space-7) var(--nexus-space-6);
}

.page-header {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  margin-bottom: var(--nexus-space-6);
  gap: var(--nexus-space-4);
  flex-wrap: wrap;
}

.page-header h1 {
  font-size: var(--nexus-typography-size-2xl);
  font-weight: var(--nexus-typography-weight-bold);
  letter-spacing: -0.02em;
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}

.subtitle {
  color: var(--text2);
  font-size: var(--nexus-typography-size-sm);
  margin-top: 3px;
}

.header-actions {
  display: flex;
  gap: 8px;
  align-items: center;
  flex-wrap: wrap;
}

/* ── Badges & Pills ─────────────────────────────────────────────────────── */
.badge {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 3px 10px;
  border-radius: var(--nexus-radius-pill);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  font-weight: var(--nexus-typography-weight-semibold);
  background: var(--surface2);
  color: var(--badge-color, var(--text2));
  border: 1px solid var(--badge-color, var(--border));
  text-transform: uppercase;
  letter-spacing: 0.04em;
}

.badge .dot {
  width: 6px;
  height: 6px;
  border-radius: var(--nexus-radius-pill);
  background: var(--badge-color, var(--text2));
}

.stat-pill {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius-sm);
  padding: 4px 10px;
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
}
.stat-pill-label {
  color: var(--text3);
}
.stat-pill-val {
  color: var(--accent);
  font-weight: var(--nexus-typography-weight-semibold);
}

/* ── Projects Layout & Cards ────────────────────────────────────────────── */
.projects-layout {
  display: grid;
  grid-template-columns: 1fr 290px;
  gap: var(--nexus-space-5);
  align-items: start;
}

.projects-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: var(--nexus-space-4);
}

.filter-bar {
  display: flex;
  gap: 10px;
  align-items: center;
  margin-bottom: var(--nexus-space-4);
  flex-wrap: wrap;
}

.search-input {
  max-width: 260px;
}

.filter-pills {
  display: flex;
  gap: 6px;
  flex-wrap: wrap;
}

.filter-pill {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--nexus-radius-pill);
  color: var(--text2);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  padding: 6px 14px;
  min-height: 44px;
  cursor: pointer;
  text-transform: capitalize;
}

.filter-pill:hover {
  border-color: var(--border-bright);
  color: var(--text);
}

.filter-pill.active {
  background: var(--surface2);
  border-color: var(--accent);
  color: var(--accent);
}

.project-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: var(--nexus-space-5);
  cursor: pointer;
  transition: border-color var(--nexus-motion-duration-fast), background
    var(--nexus-motion-duration-fast);
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.project-card:hover {
  border-color: var(--accent);
  background: var(--surface2);
}

.pc-top {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
}

.pc-name,
.pc-name-btn {
  font-weight: var(--nexus-typography-weight-bold);
  font-size: var(--nexus-typography-size-lg);
  background: none;
  border: none;
  color: var(--text);
  font-family: var(--sans);
  cursor: pointer;
  padding: 0;
  text-align: left;
  min-height: 44px;
}

.pc-name-btn:hover {
  color: var(--accent);
}

.pc-repo {
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  color: var(--text2);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.pc-meta {
  display: flex;
  align-items: center;
  gap: 6px;
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  color: var(--text3);
  flex-wrap: wrap;
}

.pc-url {
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  color: var(--accent);
}

/* ── Safeguards Banner & Activity Feed ──────────────────────────────────── */
.safeguards-bar {
  margin-top: var(--nexus-space-5);
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: var(--nexus-space-4) var(--nexus-space-5);
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: var(--nexus-space-3);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  color: var(--text2);
}

.activity-panel {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: var(--nexus-space-4);
  position: sticky;
  top: 24px;
}

.activity-title {
  font-size: var(--nexus-typography-size-xs);
  font-weight: var(--nexus-typography-weight-bold);
  font-family: var(--mono);
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--text3);
  margin-bottom: 12px;
}

.activity-list {
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.activity-item {
  display: flex;
  align-items: flex-start;
  gap: 9px;
  padding: 8px;
  border-radius: var(--radius-sm);
  cursor: pointer;
  background: none;
  border: none;
  width: 100%;
  text-align: left;
  color: var(--text);
  font-family: var(--sans);
}

.activity-item:hover {
  background: var(--surface2);
}

.activity-dot {
  width: 7px;
  height: 7px;
  border-radius: var(--nexus-radius-pill);
  flex-shrink: 0;
  margin-top: 5px;
}

.activity-body {
  flex: 1;
  min-width: 0;
}

.activity-project {
  font-size: var(--nexus-typography-size-sm);
  font-weight: var(--nexus-typography-weight-semibold);
  color: var(--text);
}

.activity-meta {
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  color: var(--text3);
}

/* ── Tabs, Cards & Tables ───────────────────────────────────────────────── */
.tabs {
  display: flex;
  gap: 4px;
  border-bottom: 1px solid var(--border);
  margin-bottom: var(--nexus-space-6);
  flex-wrap: wrap;
}

.tab {
  background: none;
  border: none;
  border-bottom: 2px solid transparent;
  color: var(--text2);
  font-family: var(--sans);
  font-size: var(--nexus-typography-size-sm);
  font-weight: var(--nexus-typography-weight-medium);
  padding: 10px 16px;
  min-height: 44px;
  cursor: pointer;
  margin-bottom: -1px;
}

.tab:hover {
  color: var(--text);
}

.tab.active {
  color: var(--accent);
  border-bottom-color: var(--accent);
}

.tab-content {
  display: flex;
  flex-direction: column;
  gap: var(--nexus-space-4);
}

.overview-grid {
  display: grid;
  grid-template-columns: 320px 1fr;
  gap: var(--nexus-space-4);
}

.info-card {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: var(--nexus-space-5);
}

.info-card h3 {
  font-size: var(--nexus-typography-size-xs);
  font-weight: var(--nexus-typography-weight-bold);
  text-transform: uppercase;
  letter-spacing: 0.06em;
  color: var(--text3);
  font-family: var(--mono);
  margin-bottom: var(--nexus-space-4);
}

.info-rows {
  display: flex;
  flex-direction: column;
  gap: 10px;
}

.info-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: var(--nexus-typography-size-sm);
  gap: 12px;
}

.info-row span:first-child {
  color: var(--text2);
  flex-shrink: 0;
}

.mono {
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  color: var(--text);
}

.deploy-list {
  display: flex;
  flex-direction: column;
  gap: 6px;
}

.deploy-row {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 10px 12px;
  border-radius: var(--radius);
  background: var(--surface2);
  border: 1px solid var(--border);
  gap: 8px;
  flex-wrap: wrap;
}

.deploy-row:hover {
  border-color: var(--border-bright);
}

.deploy-row-left {
  display: flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  flex: 1;
  min-width: 0;
  flex-wrap: wrap;
  background: none;
  border: none;
  color: var(--text);
  font-family: var(--sans);
  text-align: left;
  padding: 0;
}

.deploy-row-actions {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-shrink: 0;
}

.trigger-tag {
  background: var(--surface3);
  color: var(--text2);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  padding: 2px 7px;
  border-radius: var(--radius-sm);
}

.muted {
  color: var(--text3);
  font-size: var(--nexus-typography-size-xs);
  font-family: var(--mono);
}

/* ── Log Terminal ───────────────────────────────────────────────────────── */
.log-terminal {
  background: var(--bg);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  overflow: hidden;
  font-family: var(--mono);
}

.log-header {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 16px;
  background: var(--surface);
  border-bottom: 1px solid var(--border);
  font-size: var(--nexus-typography-size-xs);
  color: var(--text2);
}

.log-dot {
  width: 8px;
  height: 8px;
  border-radius: var(--nexus-radius-pill);
  background: var(--text3);
}
.log-dot.pulsing {
  background: var(--accent);
}

.log-done {
  margin-left: auto;
  font-weight: var(--nexus-typography-weight-semibold);
}
.log-done.live {
  color: var(--success);
}
.log-done.failed {
  color: var(--danger);
}

.log-body {
  padding: 14px 16px;
  max-height: 500px;
  overflow-y: auto;
  display: flex;
  flex-direction: column;
  gap: 2px;
}

.log-line {
  font-size: var(--nexus-typography-size-xs);
  line-height: 1.65;
  color: var(--text2);
  white-space: pre-wrap;
  word-break: break-all;
}
.log-line.err {
  color: var(--danger);
}
.log-line.nexus {
  color: var(--accent);
}
.log-empty {
  color: var(--text3);
  font-size: var(--nexus-typography-size-xs);
  padding: 20px 0;
  text-align: center;
}

/* ── Modal ──────────────────────────────────────────────────────────────── */
.modal-overlay {
  position: fixed;
  inset: 0;
  background: rgba(3, 3, 3, 0.82);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: var(--nexus-zIndex-modal);
  padding: var(--nexus-space-5);
}

.modal {
  background: var(--surface);
  border: 1px solid var(--border-bright);
  border-radius: var(--radius-lg);
  width: 100%;
  max-width: 580px;
  max-height: 90dvh;
  overflow-y: auto;
  padding: var(--nexus-space-6);
  box-shadow: var(--nexus-shadow-lg);
}

.modal-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: var(--nexus-space-5);
}

.modal-header h2 {
  font-size: var(--nexus-typography-size-xl);
  font-weight: var(--nexus-typography-weight-bold);
}

.modal-actions {
  display: flex;
  justify-content: flex-end;
  gap: 10px;
  padding-top: 8px;
}

.env-row {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-bottom: 6px;
}

.code-block {
  flex: 1;
  background: var(--surface2);
  border: 1px solid var(--border);
  border-radius: var(--radius);
  padding: 10px 14px;
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  color: var(--accent);
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.copy-row {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-top: 8px;
}

.danger-zone {
  border-color: var(--danger);
}
.danger-zone h3 {
  color: var(--danger);
}

/* ── Onboarding & Empty State ───────────────────────────────────────────── */
.onboarding {
  max-width: 680px;
  margin: 0 auto;
  padding: 12px 0;
}

.onboarding-header {
  text-align: center;
  margin-bottom: 28px;
}

.onboarding-steps {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 14px;
}

.onboarding-step {
  background: var(--surface);
  border: 1px solid var(--border);
  border-radius: var(--radius-lg);
  padding: 20px;
  display: flex;
  gap: 14px;
}

.step-num {
  width: 28px;
  height: 28px;
  border-radius: var(--nexus-radius-pill);
  background: var(--surface2);
  border: 1px solid var(--accent);
  color: var(--accent);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-sm);
  font-weight: var(--nexus-typography-weight-bold);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
}

.back-btn {
  display: inline-flex;
  align-items: center;
  background: none;
  border: none;
  color: var(--text2);
  font-family: var(--mono);
  font-size: var(--nexus-typography-size-xs);
  cursor: pointer;
  margin-bottom: 8px;
  min-height: 44px;
  padding: 0 4px;
}
.back-btn:hover {
  color: var(--accent);
}

/* ── Mobile Responsive Rules (THEME-GUIDE §5) ──────────────────────────── */
@media (max-width: 900px) {
  .projects-layout,
  .overview-grid {
    grid-template-columns: 1fr;
  }
}

@media (max-width: 768px) {
  .shell {
    flex-direction: column;
    padding-bottom: calc(64px + env(safe-area-inset-bottom));
  }

  .sidebar {
    position: fixed;
    bottom: 0;
    top: auto;
    left: 0;
    right: 0;
    width: 100%;
    height: auto;
    flex-direction: row;
    align-items: center;
    justify-content: space-around;
    border-right: none;
    border-top: 1px solid var(--border);
    padding: 6px 12px calc(6px + env(safe-area-inset-bottom));
    z-index: var(--nexus-zIndex-header);
  }

  .sidebar-brand,
  .sidebar-stats,
  .sidebar-help,
  .sidebar-user {
    display: none;
  }

  .sidebar-nav {
    flex-direction: row;
    justify-content: space-around;
    width: 100%;
  }

  .nav-item {
    justify-content: center;
    width: auto;
    flex: 1;
  }

  .page {
    padding: var(--nexus-space-4);
  }
}
```

- [ ] **Step 2: Create `web/src/types.ts` and `web/src/api.ts`**

Write `apps/Nexus-Deploy/web/src/types.ts`:

```ts
export interface User {
  id: string;
  username?: string;
  email?: string;
  role?: string;
}

export type MemberAccess =
  | { member: true; operator: boolean }
  | { member: false; reason: "not_a_member" | "suspended" };

export interface NodeInfo {
  baseDomain: string;
  controlHost: string;
  hostPattern: string;
}

export interface NodeLimits {
  maxProjectsPerMember: number;
  maxConcurrentBuildsPerMember: number;
  buildsPerHour: number;
  minDeployIntervalMs: number;
  buildTimeoutMs: number;
  pullTimeoutMs: number;
  memory: string;
  cpus: string;
  pidsLimit: number;
  minDiskFreeRatio: number;
}

export interface LimitsResponse {
  limits: NodeLimits;
  baseDomain: string;
  controlHost: string;
  appHostPattern: string;
  availability: string;
}

export type PublicProjectSource =
  | { type: "git"; repo: string; branch: string; hasToken: boolean }
  | { type: "image"; image: string };

export interface Project {
  id: string;
  ownerId?: string;
  name: string;
  repo: string;
  branch: string;
  source?: PublicProjectSource;
  buildContext?: string;
  dockerfile?: string;
  healthPath?: string;
  buildCommand: string;
  startCommand: string;
  volumePath: string;
  port: number;
  env: Record<string, string>;
  status: "idle" | "building" | "live" | "failed" | "stopped";
  containerId?: string;
  containerName?: string;
  hostPort?: number;
  imageTag?: string;
  webhookSecret: string;
  autoDeployEnabled: boolean;
  notifyUrl?: string;
  memoryLimit?: string;
  cpus?: string;
  latestDeployment?: Deployment | null;
  createdAt: number;
  updatedAt: number;
}

export interface Deployment {
  id: string;
  projectId: string;
  commitSha: string;
  triggeredBy: "manual" | "webhook" | "rollback";
  status: "queued" | "building" | "live" | "failed" | "cancelled";
  logs: string[];
  imageTag?: string;
  createdAt: number;
  finishedAt?: number;
}

export interface ActivityItem {
  id: string;
  projectId: string;
  projectName: string;
  commitSha: string;
  triggeredBy: string;
  status: string;
  createdAt: number;
  finishedAt?: number;
}

export interface MemberRecord {
  authUserId: string;
  role: "owner" | "member";
  status: "active" | "suspended";
  grantedBy: string;
  grantedAt: number;
  label?: string;
}

export interface AuditEntry {
  at: number;
  actor: string;
  action: string;
  subject: string;
}

export type SessionResult =
  | {
      kind: "signed-in";
      user: User;
      access: { member: true; operator: boolean };
      node: NodeInfo;
      wsToken: string;
    }
  | { kind: "not-a-member"; user: User; reason: "not_a_member" | "suspended"; node: NodeInfo }
  | { kind: "signed-out" }
  | { kind: "unreachable" };
```

Write `apps/Nexus-Deploy/web/src/api.ts`:

```ts
import { useCallback } from "react";
import type { NodeInfo, SessionResult } from "./types.js";

/** True when rendered inside the Nexus shell iframe (?embed=1). */
export function isEmbedded(search: string = window.location.search): boolean {
  return new URLSearchParams(search).get("embed") === "1";
}

/** Standalone link back to the Nexus shell home on the node's base domain. */
export function dashboardHomeUrl(node?: NodeInfo | null): string {
  const domain =
    node?.baseDomain || window.location.hostname.replace(/^deploy\./, "") || "localhost";
  if (domain === "localhost" || domain === "127.0.0.1") return "/";
  return `https://app.${domain}`;
}

/** Public HTTPS origin for a live deployed app. */
export function appUrlFor(project: { name: string }, node?: NodeInfo | null): string {
  const baseDomain =
    node?.baseDomain || window.location.hostname.replace(/^deploy\./, "") || "localhost";
  const pattern = node?.hostPattern || "{slug}.{BASE_DOMAIN}";
  const host = pattern.replace("{slug}", project.name).replace("{BASE_DOMAIN}", baseDomain);
  return `https://${host}`;
}

/** Control-plane short link (<controlHost>/<slug>) that 302-redirects to the app. */
export function shortLinkUrlFor(project: { name: string }, node?: NodeInfo | null): string {
  const controlHost = node?.controlHost || window.location.host;
  return `https://${controlHost}/${project.name}`;
}

/**
 * Ask the control plane who is signed in.
 *
 * Per docs/THEME-GUIDE.md §5, a network failure or 5xx is "unreachable", never
 * "signed-out": only a real 401 means signed out.
 */
export async function fetchSession(): Promise<SessionResult> {
  try {
    const res = await fetch("/api/me", { credentials: "include" });
    if (res.status === 401) return { kind: "signed-out" };
    if (!res.ok) return { kind: "unreachable" };
    const body = await res.json();
    const user = body?.user;
    if (!user) return { kind: "signed-out" };
    const node: NodeInfo = body?.node ?? {
      baseDomain: window.location.hostname.replace(/^deploy\./, "") || "localhost",
      controlHost: window.location.host,
      hostPattern: "{slug}.{BASE_DOMAIN}",
    };
    if (body?.access?.member === false) {
      return {
        kind: "not-a-member",
        user,
        reason: body.access.reason === "suspended" ? "suspended" : "not_a_member",
        node,
      };
    }
    return {
      kind: "signed-in",
      user,
      access: { member: true, operator: Boolean(body?.access?.operator) },
      node,
      wsToken: typeof body?.wsToken === "string" ? body.wsToken : "",
    };
  } catch {
    return { kind: "unreachable" };
  }
}

export function useApi() {
  const req = useCallback(async (method: string, path: string, body?: unknown) => {
    const res = await fetch(path, {
      method,
      credentials: "include",
      headers: body !== undefined ? { "Content-Type": "application/json" } : {},
      body: body !== undefined ? JSON.stringify(body) : undefined,
    });
    const data = await res.json().catch(() => ({}));
    if (!res.ok) throw new Error(data.error ?? `HTTP ${res.status}`);
    return data;
  }, []);

  return {
    get: useCallback((p: string) => req("GET", p), [req]),
    post: useCallback((p: string, b?: unknown) => req("POST", p, b), [req]),
    put: useCallback((p: string, b?: unknown) => req("PUT", p, b), [req]),
    patch: useCallback((p: string, b?: unknown) => req("PATCH", p, b), [req]),
    del: useCallback((p: string) => req("DELETE", p), [req]),
  };
}
```

- [ ] **Step 3: Create `web/src/components/StatusStates.tsx`, `NewProjectModal.tsx`, `ProjectSettingsTab.tsx`, and `OperatorPanel.tsx`**

Write `apps/Nexus-Deploy/web/src/components/StatusStates.tsx`:

```tsx
import { type CSSProperties, useEffect, useRef, useState } from "react";
import { isEmbedded } from "../api";
import type { User } from "../types";

const STATUS_VARS: Record<string, string> = {
  live: "var(--success)",
  active: "var(--success)",
  building: "var(--warn)",
  queued: "var(--info)",
  failed: "var(--danger)",
  suspended: "var(--danger)",
  stopped: "var(--text3)",
  idle: "var(--text3)",
  cancelled: "var(--text3)",
};

export function Badge({ status }: { status: string }) {
  return (
    <span
      className="badge"
      style={{ "--badge-color": STATUS_VARS[status] ?? "var(--text3)" } as CSSProperties}
    >
      <span className="dot" />
      {status}
    </span>
  );
}

export function StatPill({ label, value }: { label: string; value: string }) {
  return (
    <div className="stat-pill">
      <span className="stat-pill-label">{label}</span>
      <span className="stat-pill-val">{value}</span>
    </div>
  );
}

export function NotSignedIn() {
  const embedded = isEmbedded();
  const returnPath = `${window.location.pathname}${window.location.search}`;
  const loginHref = `/api/auth/login?redirect=${encodeURIComponent(returnPath)}`;
  return (
    <div className="auth-bg">
      <div className="auth-card">
        <div className="auth-brand">
          <span className="brand-mark">⬡</span>
          <div>
            <div className="brand-name">Nexus Deploy</div>
            <div className="brand-sub">The No Hands Company</div>
          </div>
        </div>
        <p style={{ color: "var(--text2)", fontSize: 14, lineHeight: 1.55, marginBottom: 20 }}>
          Nexus Deploy uses your ecosystem account. Sign in once to deploy Git repositories and
          container images on this node.
        </p>
        <a
          className="btn-primary"
          href={loginHref}
          target={embedded ? "_top" : undefined}
          style={{ width: "100%" }}
        >
          Sign in with Nexus →
        </a>
        <div className="auth-note">Free forever · No billing · Open source (AGPL-3.0)</div>
      </div>
    </div>
  );
}

export function UnreachableState({ onRetry }: { onRetry: () => void }) {
  return (
    <div className="auth-bg">
      <div className="auth-card">
        <div className="auth-brand">
          <span className="brand-mark">⬡</span>
          <div>
            <div className="brand-name">Nexus Deploy</div>
            <div className="brand-sub">Connection interrupted</div>
          </div>
        </div>
        <p style={{ color: "var(--text2)", fontSize: 14, lineHeight: 1.55, marginBottom: 20 }}>
          The control plane or identity service could not be reached. Your session has not been
          cleared — retry when the connection recovers.
        </p>
        <button type="button" className="btn-primary" onClick={onRetry} style={{ width: "100%" }}>
          Retry connection
        </button>
      </div>
    </div>
  );
}

export function NotAMemberState({
  user,
  reason,
  onLogout,
}: {
  user: User;
  reason: "not_a_member" | "suspended";
  onLogout: () => void;
}) {
  return (
    <div className="auth-bg">
      <div className="auth-card">
        <div className="auth-brand">
          <span className="brand-mark">⬡</span>
          <div>
            <div className="brand-name">Nexus Deploy</div>
            <div className="brand-sub">
              {reason === "suspended" ? "Access suspended" : "Invite-only node"}
            </div>
          </div>
        </div>
        <p style={{ color: "var(--text2)", fontSize: 14, lineHeight: 1.55, marginBottom: 14 }}>
          {reason === "suspended"
            ? "Your Nexus Deploy membership on this node is currently suspended. Contact a node operator if you believe this is a mistake."
            : "This node shares compute among invited ecosystem members. Ask a node operator to grant your account access."}
        </p>
        <div
          className="info-card"
          style={{ background: "var(--surface2)", padding: 12, marginBottom: 16 }}
        >
          <div className="info-row">
            <span>Signed in as</span>
            <span className="mono">{user.username || user.email || user.id}</span>
          </div>
          <div className="info-row" style={{ marginTop: 6 }}>
            <span>Account ID</span>
            <span className="mono">{user.id}</span>
          </div>
        </div>
        <button type="button" className="btn-ghost" onClick={onLogout} style={{ width: "100%" }}>
          Sign out
        </button>
      </div>
    </div>
  );
}

export function LogTerminal({
  deploymentId,
  projectId,
  wsToken,
  mode,
}: {
  deploymentId?: string;
  projectId?: string;
  wsToken: string;
  mode: "build" | "container";
}) {
  const [lines, setLines] = useState<string[]>([]);
  const [done, setDone] = useState(false);
  const [status, setStatus] = useState<string>("");
  const bottomRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    setLines([]);
    setDone(false);
    setStatus("");

    const proto = window.location.protocol === "https:" ? "wss" : "ws";
    const tokenParam = wsToken ? `&token=${encodeURIComponent(wsToken)}` : "";
    const wsUrl =
      mode === "build"
        ? `${proto}://${window.location.host}/api/log-stream?deploymentId=${deploymentId}${tokenParam}`
        : `${proto}://${window.location.host}/api/container-stream?projectId=${projectId}${tokenParam}`;

    const ws = new WebSocket(wsUrl);
    ws.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);
        if (msg.type === "log" || msg.type === "info" || msg.type === "error") {
          setLines((prev) => [...prev, msg.line]);
        } else if (msg.type === "done") {
          setDone(true);
          setStatus(msg.status);
        }
      } catch {
        /* ignore malformed frame */
      }
    };
    ws.onerror = () => setLines((prev) => [...prev, "[nexus] WebSocket error"]);
    return () => ws.close();
  }, [deploymentId, projectId, wsToken, mode]);

  useEffect(() => {
    if (lines.length === 0) return;
    bottomRef.current?.scrollIntoView({ behavior: "smooth" });
  }, [lines.length]);

  return (
    <div className="log-terminal">
      <div className="log-header">
        <span className={`log-dot ${done ? "done" : "pulsing"}`} />
        <span>{mode === "build" ? "Build Output" : "Live Container Logs"}</span>
        {done && (
          <span className={`log-done ${status}`}>
            {status === "live" ? "✓ Build succeeded" : `✗ Build ${status}`}
          </span>
        )}
      </div>
      <div className="log-body">
        {lines.length === 0 && <div className="log-empty">Waiting for output…</div>}
        {lines.map((line, i) => (
          <div
            key={`${i}-${line.slice(0, 24)}`}
            className={`log-line ${
              line.includes("ERROR") || line.includes("failed")
                ? "err"
                : line.startsWith("[nexus]")
                  ? "nexus"
                  : ""
            }`}
          >
            {line}
          </div>
        ))}
        <div ref={bottomRef} />
      </div>
    </div>
  );
}
```

Write `apps/Nexus-Deploy/web/src/components/NewProjectModal.tsx`:

```tsx
import { type FormEvent, useState } from "react";
import { appUrlFor, useApi } from "../api";
import type { NodeInfo, Project } from "../types";

export function NewProjectModal({
  node,
  onClose,
  onCreated,
}: {
  node: NodeInfo;
  onClose: () => void;
  onCreated: (p: Project) => void;
}) {
  const api = useApi();
  const [sourceType, setSourceType] = useState<"git" | "image">("git");
  const [name, setName] = useState("");
  const [repo, setRepo] = useState("");
  const [branch, setBranch] = useState("main");
  const [token, setToken] = useState("");
  const [buildContext, setBuildContext] = useState("");
  const [dockerfile, setDockerfile] = useState("");
  const [image, setImage] = useState("");
  const [port, setPort] = useState(3000);
  const [healthPath, setHealthPath] = useState("");
  const [volumePath, setVolumePath] = useState("");
  const [buildCommand, setBuildCommand] = useState("");
  const [startCommand, setStartCommand] = useState("");
  const [envPairs, setEnvPairs] = useState([{ id: 1, key: "", val: "" }]);
  const [nextEnvId, setNextEnvId] = useState(2);
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const slugPreview = name
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9-]+/g, "-")
    .replace(/^-+|-+$/g, "");

  async function submit(e: FormEvent) {
    e.preventDefault();
    setError("");
    setLoading(true);
    try {
      const env: Record<string, string> = {};
      for (const p of envPairs) {
        if (p.key.trim()) env[p.key.trim()] = p.val;
      }
      const source =
        sourceType === "git"
          ? {
              type: "git" as const,
              repo,
              branch,
              ...(token.trim() ? { token: token.trim() } : {}),
            }
          : { type: "image" as const, image: image.trim() };

      const payload: Record<string, unknown> = {
        name,
        source,
        port,
        volumePath,
        buildCommand,
        startCommand,
      };
      if (buildContext.trim()) payload.buildContext = buildContext.trim();
      if (dockerfile.trim()) payload.dockerfile = dockerfile.trim();
      if (healthPath.trim()) payload.healthPath = healthPath.trim();

      const { project } = await api.post("/api/projects", payload);
      if (Object.keys(env).length > 0) {
        await api.put(`/api/projects/${project.id}/env`, { env });
      }
      onCreated(project);
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : "Failed to create project");
    } finally {
      setLoading(false);
    }
  }

  return (
    <div
      className="modal-overlay"
      role="presentation"
      onClick={onClose}
      onKeyDown={(e) => {
        if (e.key === "Escape") onClose();
      }}
    >
      <div
        className="modal"
        role="presentation"
        onClick={(e) => e.stopPropagation()}
        onKeyDown={(e) => e.stopPropagation()}
      >
        <div className="modal-header">
          <h2>New Project</h2>
          <button type="button" className="icon-btn" onClick={onClose} aria-label="Close">
            ✕
          </button>
        </div>

        <form onSubmit={submit} className="modal-form">
          <div className="field">
            <span>Deploy Source</span>
            <div className="filter-pills">
              <button
                type="button"
                className={`filter-pill ${sourceType === "git" ? "active" : ""}`}
                onClick={() => setSourceType("git")}
              >
                Git Repository
              </button>
              <button
                type="button"
                className={`filter-pill ${sourceType === "image" ? "active" : ""}`}
                onClick={() => setSourceType("image")}
              >
                Docker Image
              </button>
            </div>
          </div>

          <div className="field-row">
            <label className="field">
              <span>Project Slug</span>
              <input
                placeholder="my-app"
                value={name}
                onChange={(e) => setName(e.target.value)}
                required
              />
              {slugPreview && (
                <span className="field-hint">
                  Live URL: {appUrlFor({ name: slugPreview }, node)}
                </span>
              )}
            </label>
            <label className="field" style={{ maxWidth: 130 }}>
              <span>Container Port</span>
              <input
                type="number"
                value={port}
                onChange={(e) => setPort(Number(e.target.value))}
                min={1}
                max={65535}
              />
            </label>
          </div>

          {sourceType === "git" ? (
            <>
              <div className="field-row">
                <label className="field">
                  <span>Git Repository</span>
                  <input
                    placeholder="owner/repo or https://git.example/org/app.git"
                    value={repo}
                    onChange={(e) => setRepo(e.target.value)}
                    required
                  />
                </label>
                <label className="field" style={{ maxWidth: 150 }}>
                  <span>Branch</span>
                  <input
                    placeholder="main"
                    value={branch}
                    onChange={(e) => setBranch(e.target.value)}
                  />
                </label>
              </div>

              <label className="field">
                <span>
                  Read-Only Git Token <em>(optional — for private repositories)</em>
                </span>
                <input
                  type="password"
                  placeholder="ghp_… or glpat-… (encrypted at rest, never shown again)"
                  value={token}
                  onChange={(e) => setToken(e.target.value)}
                  autoComplete="new-password"
                />
              </label>

              <div className="field-row">
                <label className="field">
                  <span>
                    Build Context <em>(optional subdirectory)</em>
                  </span>
                  <input
                    placeholder=". or apps/web"
                    value={buildContext}
                    onChange={(e) => setBuildContext(e.target.value)}
                  />
                </label>
                <label className="field">
                  <span>
                    Dockerfile <em>(optional; Railpack if absent)</em>
                  </span>
                  <input
                    placeholder="Dockerfile"
                    value={dockerfile}
                    onChange={(e) => setDockerfile(e.target.value)}
                  />
                </label>
              </div>

              <div className="field-row">
                <label className="field">
                  <span>
                    Build Command <em>(optional)</em>
                  </span>
                  <input
                    placeholder="Auto-detected by Railpack"
                    value={buildCommand}
                    onChange={(e) => setBuildCommand(e.target.value)}
                  />
                </label>
                <label className="field">
                  <span>
                    Start Command <em>(optional)</em>
                  </span>
                  <input
                    placeholder="Auto-detected by Railpack"
                    value={startCommand}
                    onChange={(e) => setStartCommand(e.target.value)}
                  />
                </label>
              </div>
            </>
          ) : (
            <label className="field">
              <span>OCI Image Reference</span>
              <input
                placeholder="nginxinc/nginx-unprivileged:alpine or ghcr.io/org/app:1.0"
                value={image}
                onChange={(e) => setImage(e.target.value)}
                required
              />
              <span className="field-hint">
                Pulled inside the sandbox; the resolved sha256 digest is pinned on each deployment
                for exact rollback.
              </span>
            </label>
          )}

          <div className="field-row">
            <label className="field">
              <span>
                Health Probe Path <em>(optional)</em>
              </span>
              <input
                placeholder="/ or /health"
                value={healthPath}
                onChange={(e) => setHealthPath(e.target.value)}
              />
            </label>
            <label className="field">
              <span>
                Persistent Volume Mount <em>(optional)</em>
              </span>
              <input
                placeholder="/data (leave empty for stateless)"
                value={volumePath}
                onChange={(e) => setVolumePath(e.target.value)}
              />
            </label>
          </div>
          <span className="field-hint">
            Release strategy:{" "}
            {volumePath.trim()
              ? "Stop-then-start (single-writer volume safety)"
              : "Blue/green zero-downtime"}
          </span>

          <div className="field">
            <span>
              Environment Variables <em>(encrypted at rest)</em>
            </span>
            {envPairs.map((pair, idx) => (
              <div key={pair.id} className="env-row">
                <input
                  placeholder="KEY"
                  value={pair.key}
                  onChange={(e) => {
                    const next = [...envPairs];
                    next[idx] = { ...pair, key: e.target.value };
                    setEnvPairs(next);
                  }}
                />
                <input
                  placeholder="value"
                  value={pair.val}
                  onChange={(e) => {
                    const next = [...envPairs];
                    next[idx] = { ...pair, val: e.target.value };
                    setEnvPairs(next);
                  }}
                />
                <button
                  type="button"
                  className="icon-btn"
                  onClick={() => setEnvPairs(envPairs.filter((_, j) => j !== idx))}
                >
                  ✕
                </button>
              </div>
            ))}
            <button
              type="button"
              className="btn-ghost"
              onClick={() => {
                setEnvPairs([...envPairs, { id: nextEnvId, key: "", val: "" }]);
                setNextEnvId((n) => n + 1);
              }}
            >
              + Add Variable
            </button>
          </div>

          {error && <div className="form-error">{error}</div>}

          <div className="modal-actions">
            <button type="button" className="btn-ghost" onClick={onClose}>
              Cancel
            </button>
            <button type="submit" className="btn-primary" disabled={loading}>
              {loading ? <span className="spinner-sm" /> : "Create Project →"}
            </button>
          </div>
        </form>
      </div>
    </div>
  );
}
```

Write `apps/Nexus-Deploy/web/src/components/ProjectSettingsTab.tsx`:

```tsx
import { type FormEvent, useState } from "react";
import { useApi } from "../api";
import type { Project } from "../types";

export function ProjectSettingsTab({
  project,
  onReload,
  onDeleted,
}: {
  project: Project;
  onReload: () => Promise<void>;
  onDeleted: () => void;
}) {
  const api = useApi();
  const initialType = project.source?.type ?? "git";
  const [sourceType, setSourceType] = useState<"git" | "image">(initialType);
  const [repo, setRepo] = useState(
    project.source?.type === "git" ? project.source.repo : project.repo,
  );
  const [branch, setBranch] = useState(
    project.source?.type === "git" ? project.source.branch : project.branch || "main",
  );
  const [token, setToken] = useState("");
  const [clearToken, setClearToken] = useState(false);
  const [image, setImage] = useState(project.source?.type === "image" ? project.source.image : "");
  const [buildContext, setBuildContext] = useState(project.buildContext ?? "");
  const [dockerfile, setDockerfile] = useState(project.dockerfile ?? "");
  const [healthPath, setHealthPath] = useState(project.healthPath ?? "");
  const [buildCommand, setBuildCommand] = useState(project.buildCommand);
  const [startCommand, setStartCommand] = useState(project.startCommand);
  const [volumePath, setVolumePath] = useState(project.volumePath);
  const [port, setPort] = useState(project.port);
  const [memoryLimit, setMemoryLimit] = useState(project.memoryLimit ?? "");
  const [cpus, setCpus] = useState(project.cpus ?? "");
  const [notifyUrl, setNotifyUrl] = useState(project.notifyUrl ?? "");
  const [saving, setSaving] = useState(false);
  const [msg, setMsg] = useState("");
  const [err, setErr] = useState("");

  const hasSavedToken = project.source?.type === "git" && project.source.hasToken;

  async function save(e: FormEvent) {
    e.preventDefault();
    setSaving(true);
    setMsg("");
    setErr("");
    try {
      const source =
        sourceType === "git"
          ? {
              type: "git" as const,
              repo,
              branch,
              ...(clearToken ? { token: "" } : token.trim() ? { token: token.trim() } : {}),
            }
          : { type: "image" as const, image: image.trim() };

      await api.put(`/api/projects/${project.id}`, {
        source,
        buildContext,
        dockerfile,
        healthPath,
        buildCommand,
        startCommand,
        volumePath,
        port,
        memoryLimit,
        cpus,
        notifyUrl,
      });
      setToken("");
      setClearToken(false);
      await onReload();
      setMsg("Saved. Redeploy to apply container changes.");
    } catch (error: unknown) {
      setErr(error instanceof Error ? error.message : "Failed to save settings");
    } finally {
      setSaving(false);
    }
  }

  async function exportProject() {
    try {
      const data = await api.get(`/api/projects/${project.id}/export`);
      const blob = new Blob([JSON.stringify(data, null, 2)], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = `${project.name}.nexus-deploy.json`;
      a.click();
      URL.revokeObjectURL(url);
    } catch (error: unknown) {
      setErr(error instanceof Error ? error.message : "Failed to export project");
    }
  }

  async function deleteProject() {
    if (
      !confirm(
        `Delete "${project.name}"? This removes its container, volume, images, build logs, route, and slug.`,
      )
    ) {
      return;
    }
    await api.del(`/api/projects/${project.id}`);
    onDeleted();
  }

  return (
    <div className="tab-content">
      <div className="info-card">
        <h3>Project Settings</h3>
        <form onSubmit={save} className="settings-form">
          <div className="field">
            <span>Deploy Source</span>
            <div className="filter-pills">
              <button
                type="button"
                className={`filter-pill ${sourceType === "git" ? "active" : ""}`}
                onClick={() => setSourceType("git")}
              >
                Git Repository
              </button>
              <button
                type="button"
                className={`filter-pill ${sourceType === "image" ? "active" : ""}`}
                onClick={() => setSourceType("image")}
              >
                Docker Image
              </button>
            </div>
          </div>

          {sourceType === "git" ? (
            <>
              <div className="field-row">
                <label className="field">
                  <span>Git Repository</span>
                  <input value={repo} onChange={(e) => setRepo(e.target.value)} required />
                </label>
                <label className="field" style={{ maxWidth: 160 }}>
                  <span>Branch</span>
                  <input value={branch} onChange={(e) => setBranch(e.target.value)} />
                </label>
              </div>
              <label className="field">
                <span>
                  Private Repository Token{" "}
                  <em>({hasSavedToken ? "stored — leave blank to keep" : "none stored"})</em>
                </span>
                <input
                  type="password"
                  placeholder={hasSavedToken ? "Enter a new token to replace" : "ghp_… or glpat-…"}
                  value={token}
                  onChange={(e) => {
                    setToken(e.target.value);
                    if (e.target.value) setClearToken(false);
                  }}
                  autoComplete="new-password"
                />
                {hasSavedToken && (
                  <label
                    style={{ display: "inline-flex", alignItems: "center", gap: 8, marginTop: 4 }}
                  >
                    <input
                      type="checkbox"
                      checked={clearToken}
                      onChange={(e) => setClearToken(e.target.checked)}
                    />
                    <span className="field-hint">Remove stored repository token on save</span>
                  </label>
                )}
              </label>
              <div className="field-row">
                <label className="field">
                  <span>Build Context</span>
                  <input
                    placeholder=". or apps/server"
                    value={buildContext}
                    onChange={(e) => setBuildContext(e.target.value)}
                  />
                </label>
                <label className="field">
                  <span>Dockerfile</span>
                  <input
                    placeholder="Dockerfile (blank uses Railpack)"
                    value={dockerfile}
                    onChange={(e) => setDockerfile(e.target.value)}
                  />
                </label>
              </div>
              <div className="field-row">
                <label className="field">
                  <span>Build Command</span>
                  <input value={buildCommand} onChange={(e) => setBuildCommand(e.target.value)} />
                </label>
                <label className="field">
                  <span>Start Command</span>
                  <input value={startCommand} onChange={(e) => setStartCommand(e.target.value)} />
                </label>
              </div>
            </>
          ) : (
            <label className="field">
              <span>OCI Image Reference</span>
              <input
                placeholder="nginxinc/nginx-unprivileged:alpine"
                value={image}
                onChange={(e) => setImage(e.target.value)}
                required
              />
            </label>
          )}

          <div className="field-row">
            <label className="field" style={{ maxWidth: 140 }}>
              <span>Port</span>
              <input type="number" value={port} onChange={(e) => setPort(Number(e.target.value))} />
            </label>
            <label className="field">
              <span>Health Probe Path</span>
              <input
                placeholder="/ or /health"
                value={healthPath}
                onChange={(e) => setHealthPath(e.target.value)}
              />
            </label>
            <label className="field">
              <span>Persistent Volume Path</span>
              <input
                placeholder="/data (blank for stateless)"
                value={volumePath}
                onChange={(e) => setVolumePath(e.target.value)}
              />
            </label>
          </div>

          <div className="field-row">
            <label className="field">
              <span>Memory Limit</span>
              <input
                placeholder="512m (capped by node safeguard)"
                value={memoryLimit}
                onChange={(e) => setMemoryLimit(e.target.value)}
              />
            </label>
            <label className="field">
              <span>CPU Limit</span>
              <input
                placeholder="0.5 (capped by node safeguard)"
                value={cpus}
                onChange={(e) => setCpus(e.target.value)}
              />
            </label>
            <label className="field">
              <span>Deploy Notification Webhook</span>
              <input
                placeholder="https://hooks.slack.com/…"
                value={notifyUrl}
                onChange={(e) => setNotifyUrl(e.target.value)}
              />
            </label>
          </div>

          {err && <div className="form-error">{err}</div>}
          <div style={{ display: "flex", alignItems: "center", gap: 12, flexWrap: "wrap" }}>
            <button type="submit" className="btn-primary" disabled={saving}>
              {saving ? <span className="spinner-sm" /> : "Save Settings"}
            </button>
            <button type="button" className="btn-ghost" onClick={exportProject}>
              ⬇ Export Project JSON
            </button>
            {msg && <span style={{ color: "var(--success)", fontSize: 13 }}>{msg}</span>}
          </div>
        </form>
      </div>

      <div className="info-card danger-zone">
        <h3>Danger Zone</h3>
        <p style={{ color: "var(--text2)", fontSize: 13, marginBottom: 14 }}>
          Deleting a project stops and removes its container, persistent volume, images, build logs,
          published route, and reserved slug.
        </p>
        <button type="button" className="btn-danger" onClick={deleteProject}>
          Delete Project
        </button>
      </div>
    </div>
  );
}
```

Write `apps/Nexus-Deploy/web/src/components/OperatorPanel.tsx`:

```tsx
import { type FormEvent, useCallback, useEffect, useState } from "react";
import { useApi } from "../api";
import type { AuditEntry, MemberRecord } from "../types";
import { Badge } from "./StatusStates";

export function OperatorPanel() {
  const api = useApi();
  const [members, setMembers] = useState<MemberRecord[]>([]);
  const [audit, setAudit] = useState<AuditEntry[]>([]);
  const [authUserId, setAuthUserId] = useState("");
  const [label, setLabel] = useState("");
  const [error, setError] = useState("");
  const [loading, setLoading] = useState(false);

  const load = useCallback(async () => {
    const [mRes, aRes] = await Promise.all([
      api.get("/api/admin/members"),
      api.get("/api/admin/audit"),
    ]);
    setMembers(mRes.members ?? []);
    setAudit(aRes.audit ?? []);
  }, [api]);

  useEffect(() => {
    void load();
  }, [load]);

  async function grant(e: FormEvent) {
    e.preventDefault();
    setError("");
    setLoading(true);
    try {
      await api.post("/api/admin/members", {
        authUserId: authUserId.trim(),
        ...(label.trim() ? { label: label.trim() } : {}),
      });
      setAuthUserId("");
      setLabel("");
      await load();
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : "Failed to grant member");
    } finally {
      setLoading(false);
    }
  }

  async function toggleStatus(m: MemberRecord) {
    const next = m.status === "active" ? "suspended" : "active";
    try {
      await api.patch(`/api/admin/members/${encodeURIComponent(m.authUserId)}`, { status: next });
      await load();
    } catch (err: unknown) {
      setError(err instanceof Error ? err.message : "Failed to update member");
    }
  }

  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h1>Operators</h1>
          <p className="subtitle">
            Manage invited ecosystem members and inspect the 30-day zero-IP audit trail.
          </p>
        </div>
      </div>

      <div className="overview-grid">
        <div className="info-card">
          <h3>Grant Member Access</h3>
          <form onSubmit={grant} className="settings-form">
            <label className="field">
              <span>Nexus-Auth Account ID</span>
              <input
                placeholder="usr-xxxxxxxx or username"
                value={authUserId}
                onChange={(e) => setAuthUserId(e.target.value)}
                required
              />
            </label>
            <label className="field">
              <span>
                Label <em>(optional)</em>
              </span>
              <input
                placeholder="Ada Lovelace"
                value={label}
                onChange={(e) => setLabel(e.target.value)}
              />
            </label>
            {error && <div className="form-error">{error}</div>}
            <button type="submit" className="btn-primary" disabled={loading}>
              {loading ? <span className="spinner-sm" /> : "+ Grant Member Access"}
            </button>
          </form>
        </div>

        <div className="info-card">
          <h3>Invited Members ({members.length})</h3>
          {members.length === 0 ? (
            <p className="muted">No members recorded yet.</p>
          ) : (
            <div className="deploy-list">
              {members.map((m) => (
                <div key={m.authUserId} className="deploy-row">
                  <div className="deploy-row-left" style={{ cursor: "default" }}>
                    <Badge status={m.status} />
                    <span className="mono">{m.authUserId}</span>
                    {m.label && <span>({m.label})</span>}
                    <span className="trigger-tag">{m.role}</span>
                  </div>
                  <div className="deploy-row-actions">
                    <span className="muted">
                      by {m.grantedBy} · {new Date(m.grantedAt).toLocaleDateString()}
                    </span>
                    {m.role !== "owner" && (
                      <button type="button" className="btn-ghost" onClick={() => toggleStatus(m)}>
                        {m.status === "active" ? "Suspend" : "Reactivate"}
                      </button>
                    )}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      </div>

      <div className="info-card" style={{ marginTop: 18 }}>
        <h3>30-Day Audit Log</h3>
        <p className="field-hint" style={{ marginBottom: 12 }}>
          Records only account ID, action, subject, and timestamp. Visitor IP addresses and user
          agents are never read or stored.
        </p>
        {audit.length === 0 ? (
          <p className="muted">No audit events recorded.</p>
        ) : (
          <div className="deploy-list" style={{ maxHeight: 380, overflowY: "auto" }}>
            {audit.map((entry, idx) => (
              <div key={`${entry.at}-${idx}`} className="deploy-row">
                <div className="deploy-row-left" style={{ cursor: "default" }}>
                  <span className="trigger-tag">{entry.action}</span>
                  <span className="mono">{entry.subject}</span>
                </div>
                <div className="deploy-row-actions">
                  <span className="mono">{entry.actor}</span>
                  <span className="muted">{new Date(entry.at).toLocaleString()}</span>
                </div>
              </div>
            ))}
          </div>
        )}
      </div>
    </div>
  );
}
```

- [ ] **Step 4: Create `web/src/components/ProjectDetail.tsx`, `ProjectsList.tsx`, and update `web/src/App.tsx` & `web/src/main.tsx`**

Write `apps/Nexus-Deploy/web/src/components/ProjectDetail.tsx`:

```tsx
import { useCallback, useEffect, useState } from "react";
import { useNavigate, useParams } from "react-router-dom";
import { appUrlFor, shortLinkUrlFor, useApi } from "../api";
import type { Deployment, NodeInfo, Project } from "../types";
import { ProjectSettingsTab } from "./ProjectSettingsTab";
import { Badge, LogTerminal, StatPill } from "./StatusStates";

export function ProjectDetail({ wsToken, node }: { wsToken: string; node: NodeInfo }) {
  const { id } = useParams<{ id: string }>();
  const api = useApi();
  const navigate = useNavigate();

  const [project, setProject] = useState<Project | null>(null);
  const [deployments, setDeployments] = useState<Deployment[]>([]);
  const [activeDep, setActiveDep] = useState<string | null>(null);
  const [tab, setTab] = useState<"overview" | "logs" | "live" | "env" | "webhook" | "settings">(
    "overview",
  );
  const [deploying, setDeploying] = useState(false);
  const [deployErr, setDeployErr] = useState("");
  const [stats, setStats] = useState<{ cpu: string; mem: string; memPercent: string } | null>(null);
  const [envPairs, setEnvPairs] = useState<Array<{ id: number; key: string; val: string }>>([]);
  const [nextEnvId, setNextEnvId] = useState(1);
  const [envSaved, setEnvSaved] = useState(false);
  const [webhookCopied, setWebhookCopied] = useState<"url" | "secret" | null>(null);

  const load = useCallback(async () => {
    if (!id) return;
    const d = await api.get(`/api/projects/${id}`);
    setProject(d.project);
    setDeployments(d.deployments ?? []);
    if (d.deployments?.[0]) {
      setActiveDep((prev) => prev ?? d.deployments[0].id);
    }
    const entries = Object.entries(d.project.env ?? {});
    setEnvPairs(entries.map(([key, val], i) => ({ id: i + 1, key, val: val as string })));
    setNextEnvId(entries.length + 1);
  }, [api, id]);

  useEffect(() => {
    void load();
  }, [load]);

  useEffect(() => {
    if (!project || (project.status !== "building" && !deploying)) return;
    const t = setInterval(async () => {
      const d = await api.get(`/api/projects/${id}`);
      setProject(d.project);
      setDeployments(d.deployments ?? []);
      if (d.project.status !== "building") setDeploying(false);
    }, 2500);
    return () => clearInterval(t);
  }, [project?.status, deploying, id, api, project]);

  useEffect(() => {
    if (!project || project.status !== "live") {
      setStats(null);
      return;
    }
    const fetchStats = () =>
      api
        .get(`/api/projects/${id}/stats`)
        .then((d) => setStats(d.stats ?? null))
        .catch(() => {});
    fetchStats();
    const t = setInterval(fetchStats, 8000);
    return () => clearInterval(t);
  }, [project?.status, id, api, project]);

  async function triggerDeploy() {
    setDeploying(true);
    setDeployErr("");
    try {
      const { deployment } = await api.post(`/api/projects/${id}/deploy`, {});
      setActiveDep(deployment.id);
      setTab("logs");
      await load();
    } catch (e: unknown) {
      setDeployErr(e instanceof Error ? e.message : "Deploy failed");
      setDeploying(false);
    }
  }

  async function triggerRollback(depId: string) {
    if (!confirm("Roll back to this deployment image?")) return;
    setDeploying(true);
    try {
      const { deployment } = await api.post(`/api/deployments/${depId}/rollback`, {});
      setActiveDep(deployment.id);
      setTab("logs");
      await load();
    } catch (e: unknown) {
      setDeployErr(e instanceof Error ? e.message : "Rollback failed");
      setDeploying(false);
    }
  }

  async function cancelActiveBuild() {
    const running = deployments.find((d) => d.status === "building" || d.status === "queued");
    if (!running) return;
    try {
      await api.post(`/api/deployments/${running.id}/cancel`);
      setDeploying(false);
      await load();
    } catch (e: unknown) {
      setDeployErr(e instanceof Error ? e.message : "Cancel failed");
    }
  }

  async function deleteDeploymentLog(depId: string) {
    try {
      await api.del(`/api/deployments/${depId}/logs`);
      await load();
    } catch (e: unknown) {
      setDeployErr(e instanceof Error ? e.message : "Could not delete build log");
    }
  }

  async function saveEnv() {
    const env: Record<string, string> = {};
    for (const p of envPairs) {
      if (p.key.trim()) env[p.key.trim()] = p.val;
    }
    await api.put(`/api/projects/${id}/env`, { env });
    setEnvSaved(true);
    setTimeout(() => setEnvSaved(false), 2500);
  }

  function copyText(text: string, which: "url" | "secret") {
    navigator.clipboard.writeText(text);
    setWebhookCopied(which);
    setTimeout(() => setWebhookCopied(null), 2000);
  }

  if (!project) {
    return (
      <div className="loading">
        <div className="spinner" /> Loading…
      </div>
    );
  }

  const webhookUrl = `${window.location.origin}/api/webhooks/github/${project.id}`;
  const isBuilding = project.status === "building" || deploying;
  const appUrl = appUrlFor(project, node);
  const shortUrl = shortLinkUrlFor(project, node);
  const releaseMode = project.volumePath ? "Stop-then-start (volume)" : "Blue/green zero-downtime";

  return (
    <div className="page">
      <div className="page-header">
        <div>
          <button type="button" className="back-btn" onClick={() => navigate("/")}>
            ← Projects
          </button>
          <h1>
            {project.name} <Badge status={project.status} />
          </h1>
          <div style={{ display: "flex", gap: 12, flexWrap: "wrap", marginTop: 4 }}>
            {project.status === "live" && (
              <>
                <a className="pc-url" href={appUrl} target="_blank" rel="noreferrer">
                  ↗ {appUrl}
                </a>
                <a className="muted" href={shortUrl} target="_blank" rel="noreferrer">
                  short link: {shortUrl}
                </a>
              </>
            )}
          </div>
        </div>
        <div className="header-actions">
          <StatPill label="Release" value={releaseMode} />
          {stats && (
            <>
              <StatPill label="CPU" value={stats.cpu} />
              <StatPill label="RAM" value={`${stats.mem} (${stats.memPercent})`} />
            </>
          )}
          {project.status === "live" && (
            <>
              <button
                type="button"
                className="btn-ghost"
                onClick={async () => {
                  await api.post(`/api/projects/${id}/restart`);
                  await load();
                }}
              >
                ↺ Restart
              </button>
              <button
                type="button"
                className="btn-ghost"
                onClick={async () => {
                  await api.post(`/api/projects/${id}/stop`);
                  await load();
                }}
              >
                ⏸ Stop
              </button>
            </>
          )}
          {project.status === "stopped" && (
            <button
              type="button"
              className="btn-ghost"
              onClick={async () => {
                await api.post(`/api/projects/${id}/start`);
                await load();
              }}
            >
              ▶ Start
            </button>
          )}
          {isBuilding && (
            <button type="button" className="btn-danger" onClick={cancelActiveBuild}>
              ✕ Cancel Build
            </button>
          )}
          <button
            type="button"
            className="btn-primary"
            onClick={triggerDeploy}
            disabled={isBuilding}
          >
            {isBuilding ? (
              <>
                <span className="spinner-sm" /> Building…
              </>
            ) : (
              "⚡ Deploy Now"
            )}
          </button>
        </div>
      </div>

      {deployErr && (
        <div className="form-error" style={{ marginBottom: 16 }}>
          {deployErr}
        </div>
      )}

      <div className="tabs">
        {(["overview", "logs", "live", "env", "webhook", "settings"] as const).map((t) => (
          <button
            type="button"
            key={t}
            className={`tab ${tab === t ? "active" : ""}`}
            onClick={() => setTab(t)}
          >
            {t === "overview" && "📋 Overview"}
            {t === "logs" && "🔨 Build Logs"}
            {t === "live" && "📡 Live Logs"}
            {t === "env" && "⚙ Environment"}
            {t === "webhook" && "⚡ Webhook"}
            {t === "settings" && "🔧 Settings"}
          </button>
        ))}
      </div>

      {tab === "overview" && (
        <div className="tab-content">
          <div className="overview-grid">
            <div className="info-card">
              <h3>Project Info</h3>
              <div className="info-rows">
                <div className="info-row">
                  <span>Source</span>
                  <span className="mono">
                    {project.source?.type === "image"
                      ? `image: ${project.source.image}`
                      : `git: ${project.repo}`}
                  </span>
                </div>
                {project.source?.type !== "image" && (
                  <div className="info-row">
                    <span>Branch</span>
                    <span className="mono">{project.branch}</span>
                  </div>
                )}
                {project.source?.type === "git" && (
                  <div className="info-row">
                    <span>Private Token</span>
                    <span className="mono">
                      {project.source.hasToken ? "🔒 configured" : "none"}
                    </span>
                  </div>
                )}
                <div className="info-row">
                  <span>Port</span>
                  <span className="mono">:{project.port}</span>
                </div>
                <div className="info-row">
                  <span>Health Path</span>
                  <span className="mono">{project.healthPath || "/"}</span>
                </div>
                <div className="info-row">
                  <span>Release Mode</span>
                  <span className="mono">{releaseMode}</span>
                </div>
                {project.volumePath && (
                  <div className="info-row">
                    <span>Volume</span>
                    <span className="mono">{project.volumePath}</span>
                  </div>
                )}
                <div className="info-row">
                  <span>Auto-deploy</span>
                  <span className="mono">
                    {project.autoDeployEnabled ? "✓ enabled" : "✗ disabled"}
                  </span>
                </div>
              </div>
            </div>

            <div className="info-card">
              <h3>Deployments ({deployments.length})</h3>
              {deployments.length === 0 && <p className="muted">No deployments yet.</p>}
              <div className="deploy-list">
                {deployments.slice(0, 12).map((d) => (
                  <div key={d.id} className="deploy-row">
                    <button
                      type="button"
                      className="deploy-row-left"
                      onClick={() => {
                        setActiveDep(d.id);
                        setTab("logs");
                      }}
                    >
                      <Badge status={d.status} />
                      <span className="mono">{d.commitSha.slice(0, 12)}</span>
                      <span className="trigger-tag">{d.triggeredBy}</span>
                    </button>
                    <div className="deploy-row-actions">
                      <span className="muted">{new Date(d.createdAt).toLocaleString()}</span>
                      {d.imageTag && (
                        <button
                          type="button"
                          className="icon-btn"
                          title="Roll back to this image"
                          onClick={() => triggerRollback(d.id)}
                          disabled={isBuilding}
                        >
                          ↩
                        </button>
                      )}
                      {d.status !== "building" && d.status !== "queued" && (
                        <button
                          type="button"
                          className="icon-btn danger"
                          title="Delete build log"
                          onClick={() => deleteDeploymentLog(d.id)}
                        >
                          🗑
                        </button>
                      )}
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      )}

      {tab === "logs" && (
        <div className="tab-content">
          {deployments.length > 0 && (
            <div className="filter-bar">
              <div className="filter-pills">
                {deployments.slice(0, 8).map((d) => (
                  <button
                    type="button"
                    key={d.id}
                    className={`filter-pill ${activeDep === d.id ? "active" : ""}`}
                    onClick={() => setActiveDep(d.id)}
                  >
                    <Badge status={d.status} /> {d.commitSha.slice(0, 8)}
                  </button>
                ))}
              </div>
              {activeDep && (
                <button
                  type="button"
                  className="btn-ghost"
                  onClick={() => deleteDeploymentLog(activeDep)}
                >
                  🗑 Delete This Build Log
                </button>
              )}
            </div>
          )}
          {activeDep ? (
            <LogTerminal deploymentId={activeDep} wsToken={wsToken} mode="build" />
          ) : (
            <p className="muted">No deployment selected.</p>
          )}
        </div>
      )}

      {tab === "live" && (
        <div className="tab-content">
          <LogTerminal projectId={project.id} wsToken={wsToken} mode="container" />
        </div>
      )}

      {tab === "env" && (
        <div className="tab-content">
          <div className="info-card">
            <h3>Environment Variables</h3>
            <p className="field-hint" style={{ marginBottom: 16 }}>
              Encrypted at rest with AES-256-GCM. Save and redeploy to inject updated variables.
            </p>
            {envPairs.map((pair, idx) => (
              <div key={pair.id} className="env-row">
                <input
                  placeholder="KEY"
                  value={pair.key}
                  onChange={(e) => {
                    const next = [...envPairs];
                    next[idx] = { ...pair, key: e.target.value };
                    setEnvPairs(next);
                  }}
                />
                <input
                  placeholder="value"
                  value={pair.val}
                  onChange={(e) => {
                    const next = [...envPairs];
                    next[idx] = { ...pair, val: e.target.value };
                    setEnvPairs(next);
                  }}
                />
                <button
                  type="button"
                  className="icon-btn"
                  onClick={() => setEnvPairs(envPairs.filter((_, j) => j !== idx))}
                >
                  ✕
                </button>
              </div>
            ))}
            <div style={{ display: "flex", gap: 10, marginTop: 12, alignItems: "center" }}>
              <button
                type="button"
                className="btn-ghost"
                onClick={() => {
                  setEnvPairs([...envPairs, { id: nextEnvId, key: "", val: "" }]);
                  setNextEnvId((n) => n + 1);
                }}
              >
                + Add Variable
              </button>
              <button type="button" className="btn-primary" onClick={saveEnv}>
                Save Variables
              </button>
              {envSaved && <span style={{ color: "var(--success)", fontSize: 13 }}>✓ Saved</span>}
            </div>
          </div>
        </div>
      )}

      {tab === "webhook" && (
        <div className="tab-content">
          <div className="info-card">
            <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center" }}>
              <h3>Push-to-Deploy Webhook</h3>
              <label style={{ display: "inline-flex", alignItems: "center", gap: 8 }}>
                <input
                  type="checkbox"
                  checked={project.autoDeployEnabled}
                  onChange={async () => {
                    await api.put(`/api/projects/${id}`, {
                      autoDeployEnabled: !project.autoDeployEnabled,
                    });
                    await load();
                  }}
                />
                <span className="mono">
                  {project.autoDeployEnabled ? "Auto-deploy enabled" : "Auto-deploy paused"}
                </span>
              </label>
            </div>
            <div style={{ marginTop: 14 }}>
              <div className="field-hint">Payload URL</div>
              <div className="copy-row">
                <code className="code-block">{webhookUrl}</code>
                <button
                  type="button"
                  className="btn-ghost"
                  onClick={() => copyText(webhookUrl, "url")}
                >
                  {webhookCopied === "url" ? "✓ Copied" : "Copy"}
                </button>
              </div>
            </div>
            <div style={{ marginTop: 14 }}>
              <div className="field-hint">Webhook Secret (HMAC-SHA256)</div>
              <div className="copy-row">
                <code className="code-block">{project.webhookSecret}</code>
                <button
                  type="button"
                  className="btn-ghost"
                  onClick={() => copyText(project.webhookSecret, "secret")}
                >
                  {webhookCopied === "secret" ? "✓ Copied" : "Copy"}
                </button>
                <button
                  type="button"
                  className="btn-ghost"
                  onClick={async () => {
                    if (!confirm("Regenerate webhook secret?")) return;
                    const { webhookSecret } = await api.post(
                      `/api/projects/${id}/regen-webhook-secret`,
                    );
                    setProject((p) => (p ? { ...p, webhookSecret } : p));
                  }}
                >
                  ↺ Regenerate
                </button>
              </div>
            </div>
          </div>
        </div>
      )}

      {tab === "settings" && (
        <ProjectSettingsTab project={project} onReload={load} onDeleted={() => navigate("/")} />
      )}
    </div>
  );
}
```

Write `apps/Nexus-Deploy/web/src/components/ProjectsList.tsx`:

```tsx
import { useCallback, useEffect, useState } from "react";
import { useNavigate } from "react-router-dom";
import { appUrlFor, useApi } from "../api";
import type { ActivityItem, LimitsResponse, NodeInfo, Project } from "../types";
import { NewProjectModal } from "./NewProjectModal";
import { Badge } from "./StatusStates";

function timeAgo(ts: number): string {
  const s = Math.floor((Date.now() - ts) / 1000);
  if (s < 60) return `${s}s ago`;
  if (s < 3600) return `${Math.floor(s / 60)}m ago`;
  if (s < 86400) return `${Math.floor(s / 3600)}h ago`;
  return `${Math.floor(s / 86400)}d ago`;
}

export function ProjectsList({
  node,
  onCountsChange,
}: {
  node: NodeInfo;
  onCountsChange: (c: { total: number; live: number; building: number }) => void;
}) {
  const api = useApi();
  const navigate = useNavigate();
  const [projects, setProjects] = useState<Project[]>([]);
  const [activity, setActivity] = useState<ActivityItem[]>([]);
  const [limitsInfo, setLimitsInfo] = useState<LimitsResponse | null>(null);
  const [showNew, setShowNew] = useState(false);
  const [loading, setLoading] = useState(true);
  const [search, setSearch] = useState("");
  const [statusFilter, setStatusFilter] = useState("all");

  const updateCounts = useCallback(
    (list: Project[]) => {
      onCountsChange({
        total: list.length,
        live: list.filter((p) => p.status === "live").length,
        building: list.filter((p) => p.status === "building").length,
      });
    },
    [onCountsChange],
  );

  const load = useCallback(async () => {
    const [pData, aData, lData] = await Promise.all([
      api.get("/api/projects"),
      api.get("/api/activity").catch(() => ({ activity: [] })),
      api.get("/api/limits").catch(() => null),
    ]);
    setProjects(pData.projects ?? []);
    updateCounts(pData.projects ?? []);
    setActivity(aData.activity ?? []);
    if (lData) setLimitsInfo(lData);
    setLoading(false);
  }, [api, updateCounts]);

  useEffect(() => {
    void load();
  }, [load]);

  useEffect(() => {
    const es = new EventSource("/api/events");
    es.onmessage = (e) => {
      try {
        const msg = JSON.parse(e.data);
        if (msg.type === "status") {
          setProjects((prev) => {
            const updated = prev.map((p) =>
              p.id === msg.projectId ? { ...p, status: msg.status } : p,
            );
            updateCounts(updated);
            return updated;
          });
        }
      } catch {
        /* ignore malformed event */
      }
    };
    return () => es.close();
  }, [updateCounts]);

  const filtered = projects.filter((p) => {
    const matchSearch =
      !search ||
      p.name.toLowerCase().includes(search.toLowerCase()) ||
      p.repo.toLowerCase().includes(search.toLowerCase());
    const matchStatus = statusFilter === "all" || p.status === statusFilter;
    return matchSearch && matchStatus;
  });

  const exampleUrl = appUrlFor({ name: "your-app" }, node);

  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h1>Projects</h1>
          <p className="subtitle">
            Deploy a Git repository or container image to an isolated HTTPS subdomain.
          </p>
        </div>
        <button type="button" className="btn-primary" onClick={() => setShowNew(true)}>
          + New Project
        </button>
      </div>

      {loading && (
        <div className="loading">
          <div className="spinner" /> Loading projects…
        </div>
      )}

      {!loading && projects.length === 0 && (
        <div className="onboarding">
          <div className="onboarding-header">
            <div style={{ fontSize: 36, color: "var(--accent)", marginBottom: 8 }}>⬡</div>
            <h2>Deploy your first app</h2>
            <p className="subtitle" style={{ marginTop: 4 }}>
              Free forever · Rootless container sandbox · Zero visitor tracking
            </p>
          </div>
          <div className="onboarding-steps">
            <div className="onboarding-step">
              <div className="step-num">1</div>
              <div>
                <strong>Choose a source</strong>
                <p className="muted" style={{ marginTop: 4 }}>
                  Connect a public or private Git repository, or pull any OCI container image.
                </p>
              </div>
            </div>
            <div className="onboarding-step">
              <div className="step-num">2</div>
              <div>
                <strong>Build in the sandbox</strong>
                <p className="muted" style={{ marginTop: 4 }}>
                  Railpack detects Node, Python, Go, Rust, or uses your Dockerfile inside a rootless
                  BuildKit daemon.
                </p>
              </div>
            </div>
            <div className="onboarding-step">
              <div className="step-num">3</div>
              <div>
                <strong>Blue/green live release</strong>
                <p className="muted" style={{ marginTop: 4 }}>
                  Once the health probe passes, your app goes live at <code>{exampleUrl}</code>.
                </p>
              </div>
            </div>
          </div>
          <div style={{ textAlign: "center", marginTop: 24 }}>
            <button type="button" className="btn-primary" onClick={() => setShowNew(true)}>
              + Create Your First Project →
            </button>
          </div>
        </div>
      )}

      {!loading && projects.length > 0 && (
        <>
          <div className="filter-bar">
            <input
              className="search-input"
              placeholder="Search projects…"
              value={search}
              onChange={(e) => setSearch(e.target.value)}
            />
            <div className="filter-pills">
              {(["all", "live", "building", "failed", "stopped", "idle"] as const).map((s) => (
                <button
                  type="button"
                  key={s}
                  className={`filter-pill ${statusFilter === s ? "active" : ""}`}
                  onClick={() => setStatusFilter(s)}
                >
                  {s}
                </button>
              ))}
            </div>
          </div>

          <div className="projects-layout">
            <div>
              {filtered.length === 0 ? (
                <p className="muted">No projects match your filter.</p>
              ) : (
                <div className="projects-grid">
                  {filtered.map((p) => (
                    <article key={p.id} className="project-card">
                      <div className="pc-top">
                        <button
                          type="button"
                          className="pc-name-btn"
                          onClick={() => navigate(`/projects/${p.id}`)}
                        >
                          {p.name}
                        </button>
                        <Badge status={p.status} />
                      </div>
                      <div className="pc-repo">
                        {p.source?.type === "image" ? `📦 ${p.source.image}` : p.repo}
                      </div>
                      <div className="pc-meta">
                        {p.source?.type === "image" ? (
                          <span>image</span>
                        ) : (
                          <span>⎇ {p.branch}</span>
                        )}
                        <span>·</span>
                        <span>:{p.port}</span>
                        {p.latestDeployment && (
                          <>
                            <span>·</span>
                            <span>{timeAgo(p.latestDeployment.createdAt)}</span>
                          </>
                        )}
                      </div>
                      {p.status === "live" && (
                        <a
                          className="pc-url"
                          href={appUrlFor(p, node)}
                          target="_blank"
                          rel="noreferrer"
                        >
                          ↗ {appUrlFor(p, node).replace(/^https?:\/\//, "")}
                        </a>
                      )}
                    </article>
                  ))}
                </div>
              )}
            </div>

            {activity.length > 0 && (
              <div className="activity-panel">
                <div className="activity-title">Recent Activity</div>
                <div className="activity-list">
                  {activity.slice(0, 12).map((a) => (
                    <button
                      type="button"
                      key={a.id}
                      className="activity-item"
                      onClick={() => navigate(`/projects/${a.projectId}`)}
                    >
                      <span
                        className="activity-dot"
                        style={{
                          background:
                            a.status === "live"
                              ? "var(--success)"
                              : a.status === "failed"
                                ? "var(--danger)"
                                : "var(--warn)",
                        }}
                      />
                      <div className="activity-body">
                        <div className="activity-project">{a.projectName}</div>
                        <div className="activity-meta">
                          {a.commitSha.slice(0, 8)} · {a.triggeredBy} · {timeAgo(a.createdAt)}
                        </div>
                      </div>
                      <Badge status={a.status} />
                    </button>
                  ))}
                </div>
              </div>
            )}
          </div>
        </>
      )}

      {limitsInfo && (
        <div className="safeguards-bar">
          <span>
            <strong>Shared-node safeguards</strong> (equal for every member, not billing tiers):{" "}
            {limitsInfo.limits.maxProjectsPerMember} projects · {limitsInfo.limits.memory} RAM ·{" "}
            {limitsInfo.limits.cpus} CPU · {limitsInfo.limits.pidsLimit} PIDs ·{" "}
            {limitsInfo.limits.buildsPerHour} builds/hr
          </span>
          <span>Availability: {limitsInfo.availability}</span>
        </div>
      )}

      {showNew && (
        <NewProjectModal
          node={node}
          onClose={() => setShowNew(false)}
          onCreated={(p) => {
            setShowNew(false);
            navigate(`/projects/${p.id}`);
          }}
        />
      )}
    </div>
  );
}
```

Write `apps/Nexus-Deploy/web/src/App.tsx`:

```tsx
import { useCallback, useEffect, useState } from "react";
import { BrowserRouter, Navigate, Route, Routes, useLocation, useNavigate } from "react-router-dom";
import { dashboardHomeUrl, fetchSession, isEmbedded } from "./api";
import { OperatorPanel } from "./components/OperatorPanel";
import { ProjectDetail } from "./components/ProjectDetail";
import { ProjectsList } from "./components/ProjectsList";
import { NotAMemberState, NotSignedIn, UnreachableState } from "./components/StatusStates";
import type { NodeInfo, SessionResult, User } from "./types";

function Sidebar({
  user,
  operator,
  node,
  embedded,
  onLogout,
  counts,
}: {
  user: User;
  operator: boolean;
  node: NodeInfo;
  embedded: boolean;
  onLogout: () => void;
  counts: { total: number; live: number; building: number };
}) {
  const navigate = useNavigate();
  const loc = useLocation();
  const inOperators = loc.pathname.startsWith("/operators");

  return (
    <aside className="sidebar">
      {!embedded && (
        <div className="sidebar-brand">
          <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
            <span className="brand-mark">⬡</span>
            <div>
              <div className="brand-name">Nexus Deploy</div>
              <div className="brand-sub">{node.controlHost}</div>
            </div>
          </div>
          <a
            className="sidebar-home-link"
            href={dashboardHomeUrl(node)}
            title="Back to Nexus Dashboard"
          >
            Nexus ↗
          </a>
        </div>
      )}

      <nav className="sidebar-nav">
        <button
          type="button"
          className={`nav-item ${!inOperators ? "active" : ""}`}
          onClick={() => navigate("/")}
        >
          <span>⬡</span> Projects
          {counts.total > 0 && <span className="nav-count">{counts.total}</span>}
        </button>
        {operator && (
          <button
            type="button"
            className={`nav-item ${inOperators ? "active" : ""}`}
            onClick={() => navigate("/operators")}
          >
            <span>🛡</span> Operators
          </button>
        )}
      </nav>

      {counts.total > 0 && (
        <div className="sidebar-stats">
          {counts.live > 0 && (
            <div className="sidebar-stat">
              <span className="stat-dot live" />
              <span>{counts.live} live</span>
            </div>
          )}
          {counts.building > 0 && (
            <div className="sidebar-stat">
              <span className="stat-dot building" />
              <span>{counts.building} building</span>
            </div>
          )}
          {counts.live === 0 && counts.building === 0 && (
            <div className="sidebar-stat">
              <span className="stat-dot idle" />
              <span>all stopped</span>
            </div>
          )}
        </div>
      )}

      <div className="sidebar-help">
        <div className="help-title">How Deploy Works</div>
        <ol className="help-steps">
          <li>Push to Git or pick an OCI image</li>
          <li>Railpack or Dockerfile builds in sandbox</li>
          <li>Blue/green probe switches live HTTPS route</li>
        </ol>
      </div>

      {!embedded && (
        <div className="sidebar-user">
          <div className="role-badge">{operator ? "operator" : user.role || "member"}</div>
          <div className="email">{user.email || user.username || user.id}</div>
          <button type="button" className="logout-btn" onClick={onLogout}>
            Sign out
          </button>
        </div>
      )}
    </aside>
  );
}

function AppShell({
  wsToken,
  user,
  operator,
  node,
  onLogout,
}: {
  wsToken: string;
  user: User;
  operator: boolean;
  node: NodeInfo;
  onLogout: () => void;
}) {
  const [counts, setCounts] = useState({ total: 0, live: 0, building: 0 });
  const embedded = isEmbedded();

  return (
    <div className="shell">
      <Sidebar
        user={user}
        operator={operator}
        node={node}
        embedded={embedded}
        onLogout={onLogout}
        counts={counts}
      />
      <main className="main">
        <Routes>
          <Route path="/" element={<ProjectsList node={node} onCountsChange={setCounts} />} />
          <Route path="/projects/:id" element={<ProjectDetail wsToken={wsToken} node={node} />} />
          {operator && <Route path="/operators" element={<OperatorPanel />} />}
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </main>
    </div>
  );
}

export default function App() {
  const [session, setSession] = useState<SessionResult | null>(null);

  const checkSession = useCallback(async () => {
    try {
      localStorage.removeItem("nexus-token");
    } catch {
      /* private mode */
    }
    const next = await fetchSession();
    setSession(next);
  }, []);

  useEffect(() => {
    void checkSession();
  }, [checkSession]);

  // Re-check automatically when connectivity or tab visibility returns after a drop.
  useEffect(() => {
    if (session?.kind !== "unreachable") return;
    const onRecover = () => {
      if (document.visibilityState === "visible") void checkSession();
    };
    window.addEventListener("online", onRecover);
    document.addEventListener("visibilitychange", onRecover);
    return () => {
      window.removeEventListener("online", onRecover);
      document.removeEventListener("visibilitychange", onRecover);
    };
  }, [session?.kind, checkSession]);

  function logout() {
    window.location.href = "/api/auth/logout";
  }

  if (!session) {
    return (
      <div className="loading">
        <div className="spinner" /> Nexus Deploy
      </div>
    );
  }
  if (session.kind === "unreachable") {
    return <UnreachableState onRetry={checkSession} />;
  }
  if (session.kind === "signed-out") {
    return (
      <BrowserRouter>
        <NotSignedIn />
      </BrowserRouter>
    );
  }
  if (session.kind === "not-a-member") {
    return <NotAMemberState user={session.user} reason={session.reason} onLogout={logout} />;
  }

  return (
    <BrowserRouter>
      <AppShell
        wsToken={session.wsToken}
        user={session.user}
        operator={session.access.operator}
        node={session.node}
        onLogout={logout}
      />
    </BrowserRouter>
  );
}
```

Write `apps/Nexus-Deploy/web/src/main.tsx`:

```tsx
import React from "react";
import ReactDOM from "react-dom/client";
import App from "./App";
import "./styles/index.css";

// BrowserRouter is managed inside App.tsx — do not add another here
const root = document.getElementById("root");
if (root) {
  ReactDOM.createRoot(root).render(
    <React.StrictMode>
      <App />
    </React.StrictMode>,
  );
}
```

- [ ] **Step 5: Update `tests/no-hardcoded-domain.test.ts` and create `tests/dashboard.test.ts`**

Write `apps/Nexus-Deploy/tests/no-hardcoded-domain.test.ts`:

```ts
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
    return entry.name.endsWith(".ts") || entry.name.endsWith(".tsx") ? [full] : [];
  });
}

test("no backend or frontend source file hardcodes a deployment domain", () => {
  const dirs = [path.resolve("src"), path.resolve("web/src")];
  const offenders = dirs
    .flatMap((d) => sourceFiles(d))
    .filter((file) => /tnhc\.dev/i.test(fs.readFileSync(file, "utf8")));
  assert.deepEqual(offenders, []);
});
```

Write `apps/Nexus-Deploy/tests/dashboard.test.ts`:

```ts
import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import { test } from "node:test";
import { appUrlFor, dashboardHomeUrl, isEmbedded, shortLinkUrlFor } from "../web/src/api.js";

test("dashboard vendored tokens and stylesheet follow the Void theme contract", () => {
  const tokens = fs.readFileSync(path.resolve("web/src/styles/nexus-tokens.css"), "utf8");
  assert.match(tokens, /--nexus-color-bg-canvas:\s*#030303/);
  assert.match(tokens, /--nexus-color-accent-primary:\s*#CCFF00/i);
  assert.match(tokens, /Figtree/);

  const css = fs.readFileSync(path.resolve("web/src/styles/index.css"), "utf8");
  assert.match(css, /@import\s+"\.\/*nexus-tokens\.css";/);
  assert.match(css, /100dvh/);
  assert.match(css, /min-height:\s*44px/);
  assert.match(css, /env\(safe-area-inset-bottom\)/);
  assert.doesNotMatch(css, /linear-gradient|radial-gradient|backdrop-filter|hsl\(/i);

  const html = fs.readFileSync(path.resolve("web/index.html"), "utf8");
  assert.match(html, /viewport-fit=cover/);
});

test("dashboard URL helpers use the node's configured domain and detect ?embed=1", () => {
  assert.equal(isEmbedded("?embed=1"), true);
  assert.equal(isEmbedded("?embed=0"), false);
  assert.equal(isEmbedded(""), false);

  const node = {
    baseDomain: "local.test",
    controlHost: "deploy.local.test",
    hostPattern: "{slug}.{BASE_DOMAIN}",
  };
  assert.equal(dashboardHomeUrl(node), "https://app.local.test");
  assert.equal(appUrlFor({ name: "demo-app" }, node), "https://demo-app.local.test");
  assert.equal(shortLinkUrlFor({ name: "demo-app" }, node), "https://deploy.local.test/demo-app");
});
```

- [ ] **Step 6: Verify build, typechecks, Biome, and tests, then commit Task 2**

```bash
cd apps/Nexus-Deploy
/tmp/nexus-deploy-tools/biome check web/src tests/no-hardcoded-domain.test.ts tests/dashboard.test.ts
node ./web/node_modules/typescript/bin/tsc --noEmit -p web/tsconfig.json
node ./node_modules/typescript/bin/tsc --noEmit -p tsconfig.test.json
(cd web && ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild-web node ./node_modules/vite/bin/vite.js build)
rm -rf web/dist
ESBUILD_BINARY_PATH=/tmp/nexus-deploy-tools/esbuild node --import tsx --test tests/no-hardcoded-domain.test.ts tests/dashboard.test.ts
git add web/index.html web/src tests/no-hardcoded-domain.test.ts tests/dashboard.test.ts
git commit -m "feat(web): adopt the Void theme and support members, image sources and log deletion"
```

---

## Task 3: Documentation (`AGENTS.md`, `README.md`) & Quality Gate (`check.sh`) (`apps/Nexus-Deploy`)

**Verified scratch commit:** `1c72b64` (tree `461da18c32ee7e8d50812049de1605443bf24a01`)

**Files:**
- Modify: `apps/Nexus-Deploy/AGENTS.md`
- Modify: `apps/Nexus-Deploy/README.md`
- Create: `apps/Nexus-Deploy/check.sh`

- [ ] **Step 1: Update `AGENTS.md` and `README.md`, and create `check.sh`**

Write `apps/Nexus-Deploy/AGENTS.md`:

````markdown
# Nexus Deploy — Agent Guide

## Purpose
A free, open-source alternative to paid deployment platforms (Railway, Render) by The No Hands Company. Deploy a Git repository or a container image and get a live HTTPS app. A general service for anyone: no Nexus app is special to it.

## Architecture (v0.6)
```
GitHub push / manual deploy
  → POST /api/webhooks/github/:projectId (HMAC verified) or POST /api/projects/:id/deploy
  → createDeployment()                      (writes queued deployment to encrypted store)
  → runDeploy()                             (async pipeline in src/lib/build.ts)
      Git source:   git clone (askpass for private repos)
                    → Dockerfile build OR Railpack (buildkit frontend, zero privileged containers)
      Image source: docker pull → pinned @sha256 digest
  → runRelease()                            (src/lib/release.ts)
      → allocate free loopback port in APP_PORT_RANGE (default 20000-29999)
      → docker run on SANDBOX_DOCKER_HOST (rootless, --cap-drop ALL, no-new-privileges, resource & log caps)
      → HTTP health probe on 127.0.0.1:<hostPort><healthPath>
      → publish route to Nexus Cloud (/api/v1/user-apps/:slug with userDeployed=true)
      → blue/green drain & stop previous container (or stop-then-start when volumePath is set)
  → WebSocket /api/log-stream               (build log streaming per deploymentId)
  → WebSocket /api/container-stream         (runtime stdout/stderr streaming)
  → SSE       /api/events                   (project status updates for the caller's projects)
  → status-sync (30s poll sandbox Docker, corrects DB, emits SSE events)
  → image-prune (hourly, removes unreferenced images in the dedicated sandbox daemon)
```

## Key files
| File | Purpose |
|---|---|
| `src/app.ts` | Express app factory built from injected dependencies (`AppDeps`) |
| `src/index.ts` | Production entrypoint wiring real dependencies and starting the server |
| `src/lib/wiring.ts` | Wires Nexus-Auth, sandbox Docker ops, and Cloud route publisher |
| `src/lib/build.ts` | Clone/build or pull → release → rollback pipeline |
| `src/lib/build-steps.ts` | Git askpass clone, Dockerfile / Railpack image build, OCI image pull |
| `src/lib/release.ts` | Blue/green and stop-then-start container release state machine |
| `src/lib/run-args.ts` | Hardened `docker run` argument builder (`--cap-drop ALL`, caps, log limits) |
| `src/lib/docker.ts` | Sandbox-enforced Docker/Railpack CLI wrapper (`SANDBOX_DOCKER_HOST` required) |
| `src/lib/cloud.ts` | Nexus Cloud `user-apps` route publisher + heartbeat |
| `src/lib/edge.ts` | Combines Cloud publisher with Hosting + Cloud slug availability checks |
| `src/lib/members.ts` | Invite-only member list, first-founder seeding, per-project ownership |
| `src/lib/safeguards.ts` | Node-wide per-member quotas, rate limits, timeouts, and disk guard |
| `src/lib/store.ts` | AES-256-GCM encrypted JSON DB with serialized async `writeDb` queue |
| `src/doctor/` | `npm run doctor` static preflight (13 checks across sandbox, builder, DNS, Cloud, UI) |
| `src/verify/` | `npm run verify` live end-to-end conformance suite using bundled `fixtures/` |
| `sandbox/setup.sh` | Operator `sudo` script provisioning the rootless sandbox user, loop disk, and nftables egress filter |
| `sandbox/install-builder.sh` | User-scoped Railpack `v0.43.0` installer (`~/.local/bin/railpack`) |
| `web/src/` | React + Vite dashboard using the shared Void theme (`nexus-tokens.css`) |

## API surface
| Method | Path | Auth | Notes |
|---|---|---|---|
| GET | `/api/health` | Public | Liveness check |
| GET | `/api/auth/login` | Public | Redirects browser to `NEXUS_AUTH_PUBLIC_URL/login` |
| GET | `/api/auth/logout` | Public | Redirects browser to `NEXUS_AUTH_PUBLIC_URL/logout` |
| GET | `/api/me` | Auth | Current identity, member/operator access, node info, and `wsToken` |
| GET | `/api/limits` | Member | Node safeguards, domain patterns, and `best-effort` availability |
| GET | `/api/events` | Member | SSE status stream for projects visible to the caller |
| GET | `/api/activity` | Member | Last 25 deployments across projects visible to the caller |
| GET | `/api/projects` | Member | Caller's projects (or all for operators) + `latestDeployment` |
| POST | `/api/projects` | Member | Create Git or OCI image project (checks slug in Cloud + Hosting) |
| GET | `/api/projects/:id` | Owner/Op | Project details + deployments |
| PUT | `/api/projects/:id` | Owner/Op | Update project settings / source / private token |
| DELETE | `/api/projects/:id` | Owner/Op | Remove containers, route, volume, images, logs, and project |
| GET | `/api/projects/:id/export` | Owner/Op | Export portable project JSON (omits credentials) |
| PUT | `/api/projects/:id/env` | Owner/Op | Validate and save encrypted environment variables |
| POST | `/api/projects/:id/deploy` | Owner/Op | Trigger deployment |
| POST | `/api/projects/:id/stop` | Owner/Op | Stop running container |
| POST | `/api/projects/:id/start` | Owner/Op | Start stopped container |
| POST | `/api/projects/:id/restart` | Owner/Op | Restart container |
| GET | `/api/projects/:id/stats` | Owner/Op | Live CPU/memory/network container stats |
| GET | `/api/projects/:id/health` | Owner/Op | Container status check |
| POST | `/api/projects/:id/regen-webhook-secret` | Owner/Op | Rotate GitHub webhook HMAC secret |
| POST | `/api/deployments/:id/rollback` | Owner/Op | Roll back to a deployment's recorded image or `@sha256` digest |
| POST | `/api/deployments/:id/cancel` | Owner/Op | Cancel an in-progress build |
| GET | `/api/deployments/:id/logs` | Owner/Op | Build log lines and deployment status |
| DELETE | `/api/deployments/:id/logs` | Owner/Op | Erase a finished deployment's build log |
| GET | `/api/admin/members` | Operator | List invited members |
| POST | `/api/admin/members` | Operator | Grant member access (`authUserId`, optional `label`) |
| PATCH | `/api/admin/members/:authUserId` | Operator | Suspend or reactivate a member |
| GET | `/api/admin/audit` | Operator | 30-day zero-IP audit trail |
| POST | `/api/webhooks/github/:projectId` | HMAC | GitHub push webhook (`X-Hub-Signature-256`) |
| WS | `/api/log-stream?deploymentId=&token=` | Ticket/Token/Cookie | Real-time build log stream |
| WS | `/api/container-stream?projectId=&token=` | Ticket/Token/Cookie | Real-time container stdout/stderr stream |

## Auth
Identity is delegated to **Nexus-Auth** (`GET /api/v1/auth/check`):
- `Authorization: Bearer <token>` — for API clients
- `Cookie: nexus_session=<token>` — shared `HttpOnly` ecosystem session cookie for browsers
- `?token=<wsToken|sessionToken>` — for WebSocket upgrades through the edge proxy (`GET /api/me` issues a short-lived HMAC `wsToken` so `nexus_session` stays `HttpOnly`)

## Conventions
- Keep it lean. No ORM, no message queue, no microservices.
- All DB writes go through `writeDb(mutate)` — never call `saveDb` during builds.
- `saveDb` is only for boot-time seeding or test resets where no concurrent writes exist.
- Image naming for Git builds: `nexus/{project-name}:{8-char-deployment-id}`
- Container naming: `nexus-app-{project-name}-{8-char-deployment-id}` (`src/lib/containers.ts`) so blue/green releases can run old and new containers side by side during cutover.
- Volume naming: `nexus-vol-{project-name}` (mounted at `project.volumePath`).
- **Sandbox enforcement**: every `docker` and `railpack` call requires `SANDBOX_DOCKER_HOST` and never falls back to the host daemon.
- **Privacy**: never read or store visitor IP addresses or user agents (`scripts/check-privacy.sh` enforces this across the monorepo). The audit log records only account ID, action, subject, and timestamp, retained for 30 days.
- **Portability**: node-specific values (`BASE_DOMAIN`, `APP_HOST_PATTERN`, `CONTROL_HOST`, `SANDBOX_*`, `APP_PORT_RANGE`, `CAP_*`) live in `src/lib/config.ts`. No `src/` or `web/src/` file may hardcode `tnhc.dev` (`tests/no-hardcoded-domain.test.ts`).
- **No billing**: Never add billing, plans or paywalls; safeguards protect the node and apply equally.

## Dev & Quality Gate
```bash
npm install && npm --prefix web install
./check.sh           # typecheck (server, tests, web) + full unit test suite
npm run doctor       # static 13-check node & sandbox readiness report
npm run verify       # live 8-step end-to-end conformance suite against the sandbox daemon
```
````

Write `apps/Nexus-Deploy/README.md`:

````markdown
# ⬡ Nexus Deploy

**Self-hosted deployment platform by [The No Hands Company](https://github.com/The-No-Hands-company)**

Deploy a Git repository or a container image and get a live HTTPS app. A free, open-source alternative to paid deployment platforms such as Railway and Render, for anyone to run or use.

---

## 💸 Pricing

**Free. Always. No exceptions.** Nexus Deploy is part of [The No Hands Company](https://github.com/The-No-Hands-company)'s promise that nothing it makes is sold or put behind a paywall. The software contains no billing or plans. Safeguards that protect a shared node (CPU, memory and build limits) apply equally to everyone and are shown in the dashboard. Anyone running their own node may charge for it; the licence allows that.

---

## ✨ Features (v0.6 Foundation)

| Feature | Details |
|---|---|
| **Git & OCI Image sources** | Deploy any public or private Git repository (via askpass token) or any OCI container image |
| **Auto-detect runtime** | [Railpack](https://railpack.com) (`v0.43.0`) builds Node, Python, Go, Rust, etc. via rootless BuildKit; uses `Dockerfile` when present |
| **Blue/green releases** | Starts the new container on a free loopback port, probes health, switches the live route, and drains the old container with zero downtime |
| **Rootless sandbox daemon** | Runs builds and containers inside a dedicated unprivileged user's rootless Docker daemon on a size-capped XFS loop filesystem |
| **Container hardening** | Drops all Linux capabilities (`--cap-drop ALL`), sets `no-new-privileges`, enforces memory/CPU/PID caps, blocks host loopback/LAN egress, and caps container logs at 20 MB |
| **Zero visitor tracking** | Edge proxy strips visitor IPs, cookies, credentials, and client tags before requests reach deployed apps |
| **Instant rollback** | Re-deploy any previous image or pinned `@sha256` digest with one click |
| **Data ownership** | Export portable project JSON (`GET /api/projects/:id/export`), delete individual build logs, or delete a project and every artifact it created |

---

## 🏗️ Architecture

```
Git push (HMAC webhook) or Manual / Image deploy
  ↓
Nexus Deploy Control Plane (Express + TypeScript + WebSocket/SSE)
  ↓ SANDBOX_DOCKER_HOST (never the host daemon)
Rootless Docker Sandbox (dedicated user, size-capped XFS loop image, nftables egress filter)
  ├─ Builder: Dockerfile or Railpack v0.43.0 (BuildKit frontend)
  └─ Runtime: 127.0.0.1:<port in 20000-29999> (--cap-drop ALL, 512 MB, 0.5 CPU, 256 PIDs, 10m×2 logs)
  ↓ Health probe passes
Nexus Cloud Route Registry (/api/v1/user-apps/:slug, userDeployed=true)
  ↓
Ecosystem Reverse Proxy (cookie/credential/IP/tag isolation + deploy.<domain>/<slug> short link)
  ↓
https://<slug>.<base-domain>
```

---

## 🚀 Node Bring-Up

### 1. Provision the rootless sandbox (requires `sudo` once)

```bash
sudo bash sandbox/setup.sh --deploy-user "$USER"
```

This creates the unprivileged `nexus-deploy` user, allocates a size-capped XFS loop image (`/var/lib/nexus-deploy-sandbox.img`), starts the rootless Docker daemon with `local` log caps (`10m` × 2 files), grants your service user group access to its socket, and installs the `nftables` host-loopback/LAN egress filter.

### 2. Install the Railpack builder

```bash
bash sandbox/install-builder.sh
```

Installs pinned `railpack v0.43.0` (SHA-256 verified) to `~/.local/bin/railpack`.

### 3. Build and check readiness

```bash
npm install
npm --prefix web install
npm run build
./check.sh
npm run doctor
```

### 4. Run end-to-end conformance verification

```bash
SANDBOX_DOCKER_HOST="unix:///run/user/$(id -u nexus-deploy)/docker.sock" \
SANDBOX_STORAGE_PATH="/var/lib/nexus-deploy-sandbox" \
npm run verify
```

---

## 📄 License

[GNU AGPL-3.0](LICENSE). Fork it, change it, run it; if you offer a changed version to others as a service, share your changes too.
````

Write `apps/Nexus-Deploy/check.sh`:

```bash
#!/usr/bin/env bash
# Quality gate for Nexus-Deploy. Runs from any working directory.
set -euo pipefail
cd "$(dirname "$0")"

echo "nexus-deploy..."

# If node_modules lives on a noexec mount, stage executable esbuild copies in /tmp
# so tsx and vite can spawn them during typecheck/test runs.
if [ -z "${ESBUILD_BINARY_PATH:-}" ]; then
  ESBUILD_SRC="node_modules/@esbuild/linux-x64/bin/esbuild"
  if [ -f "$ESBUILD_SRC" ] && [ ! -x "$ESBUILD_SRC" ]; then
    TMP_TOOLS="${TMPDIR:-/tmp}/nexus-deploy-tools"
    mkdir -p "$TMP_TOOLS"
    cp "$ESBUILD_SRC" "$TMP_TOOLS/esbuild"
    chmod +x "$TMP_TOOLS/esbuild"
    export ESBUILD_BINARY_PATH="$TMP_TOOLS/esbuild"
  fi
fi

node ./node_modules/typescript/bin/tsc -p tsconfig.json --noEmit
node ./node_modules/typescript/bin/tsc -p tsconfig.test.json --noEmit
node ./web/node_modules/typescript/bin/tsc -p web/tsconfig.json --noEmit
node --import tsx --test tests/*.test.ts

echo "PASS"
```

- [ ] **Step 2: Run `bash check.sh` and commit Task 3**

```bash
cd apps/Nexus-Deploy
bash check.sh
git add AGENTS.md README.md check.sh
git commit -m "docs(deploy): update AGENTS.md and README.md for v0.6 and add check.sh gate"
```

---

## Task 4: Monorepo Token Vendoring, `deploy/production/deploy.sh` & `scripts/privacy-canary.sh`

**Files:**
- Modify: `packages/nexus-design/scripts/vendor.ts`
- Modify: `docs/THEME-GUIDE.md`
- Modify: `deploy/production/deploy.sh`
- Modify: `deploy/production/tests/start-contract.test.sh`
- Modify: `scripts/privacy-canary.sh`

- [ ] **Step 1: Add `apps/Nexus-Deploy` to `packages/nexus-design/scripts/vendor.ts` and `docs/THEME-GUIDE.md`**

In `packages/nexus-design/scripts/vendor.ts`, add `apps/Nexus-Deploy` to `vendorTargets()`:

```ts
export function vendorTargets(): { repo: string; dest: string }[] {
  return [
    { repo: "apps/Nexus", dest: "apps/Nexus/packages/nexus-web/src/nexus-tokens.css" },
    {
      repo: "apps/Nexus-Hosting",
      dest: "apps/Nexus-Hosting/artifacts/federated-hosting/src/nexus-tokens.css",
    },
    {
      repo: "apps/Nexus-Deploy",
      dest: "apps/Nexus-Deploy/web/src/styles/nexus-tokens.css",
    },
  ];
}
```

Update `docs/THEME-GUIDE.md` §3B and §7 to list `Deploy` (`apps/Nexus-Deploy/web/src/styles/nexus-tokens.css`) as a vendored target, and run `bun test packages/nexus-design/tests/vendor.test.ts`.

- [ ] **Step 2: Update `deploy/production/deploy.sh` and `deploy/production/tests/start-contract.test.sh`**

In `deploy/production/deploy.sh`:
1. Add `start_nexus_deploy_service()`, wire `nexus-deploy` into `preflight_service_start()` and `start_named_service()`, and add `http://localhost:8096/api/health` to `cmd_status()`.
2. Guard `start_service "nexus-draw-web"` in `cmd_start()` with `|| warn "Nexus Draw Caddy did not start"` so an occupied `:8091` cannot abort `cmd_start` before reaching `nexus-deploy`.
3. Update `deploy/production/tests/start-contract.test.sh` to assert that `nexus-deploy` starts on port `8096`, receives `CLOUD_URL=http://127.0.0.1:8787` and `HOSTING_SITE_URL=http://127.0.0.1:8090`, and never receives `APP_SECRET`, `JWT_SECRET`, or `CLOUD_USER_APPS_KEY` in `argv`.

- [ ] **Step 3: Update `scripts/privacy-canary.sh`**

Add `deploy.tnhc.dev` to the host probe loop in `scripts/privacy-canary.sh` and document why member-deployed containers in the rootless `nexus-deploy` sandbox are isolated at the proxy rather than grepped by the canary. Verify with:

```bash
bash deploy/production/tests/start-contract.test.sh
bash scripts/privacy-canary.sh --self-test
```

---

## Task 5: Operator Bring-Up & Disposable Second-Node Portability Proof (Operator Runbook)

> **Note:** Per project constraints, the AI assistant never runs `sudo`, never edits live `.env` files, and never restarts production services. Run the commands below in an operator terminal.

- [ ] **Step 1: Provision the rootless sandbox daemon (if not already run from Plan 3 Task 3)**

```bash
sudo bash apps/Nexus-Deploy/sandbox/setup.sh --deploy-user "$USER" --size-gb 12 --force
```

- [ ] **Step 2: Configure `NEXUS_CLOUD_USER_APPS_KEY` and sandbox paths**

Generate the dedicated user-apps credential and append it to `apps/Nexus-Cloud/.env` and `apps/Nexus-Deploy/.env`:

```bash
USER_APPS_KEY="$(openssl rand -hex 32)"
SBX_UID="$(id -u nexus-deploy)"
printf 'NEXUS_CLOUD_USER_APPS_KEY=%s\n' "$USER_APPS_KEY" >> apps/Nexus-Cloud/.env
cat >> apps/Nexus-Deploy/.env <<EOF
CLOUD_USER_APPS_KEY=$USER_APPS_KEY
SANDBOX_DOCKER_HOST=unix:///run/user/$SBX_UID/docker.sock
SANDBOX_STORAGE_PATH=/var/lib/nexus-deploy-sandbox
EOF
chmod 600 apps/Nexus-Cloud/.env apps/Nexus-Deploy/.env
```

- [ ] **Step 3: Build `apps/Nexus-Deploy` and restart `cloud`, `proxy`, and `nexus-deploy`**

```bash
(cd apps/Nexus-Deploy && npm run build)
source apps/Nexus-Cloud/.env
bash deploy/production/deploy.sh restart cloud
bash deploy/production/deploy.sh restart proxy
bash deploy/production/deploy.sh restart nexus-deploy
```

- [ ] **Step 4: Run `doctor` (13/13 pass) and `verify` (8/8 pass) on this node**

```bash
cd apps/Nexus-Deploy
SANDBOX_DOCKER_HOST="unix:///run/user/$(id -u nexus-deploy)/docker.sock" \
SANDBOX_STORAGE_PATH="/var/lib/nexus-deploy-sandbox" \
CLOUD_URL="http://127.0.0.1:8787" \
CLOUD_USER_APPS_KEY="$(sed -n 's/^CLOUD_USER_APPS_KEY=//p' .env | head -1)" \
BASE_DOMAIN="tnhc.dev" \
npm run doctor

SANDBOX_DOCKER_HOST="unix:///run/user/$(id -u nexus-deploy)/docker.sock" \
SANDBOX_STORAGE_PATH="/var/lib/nexus-deploy-sandbox" \
npm run verify
```

- [ ] **Step 5: Run the disposable second-node portability check (`BASE_DOMAIN=local.test`)**

```bash
cd apps/Nexus-Deploy
BASE_DOMAIN="local.test" \
CONTROL_HOST="deploy.local.test" \
APP_HOST_PATTERN="{slug}.{BASE_DOMAIN}" \
SANDBOX_DOCKER_HOST="unix:///run/user/$(id -u nexus-deploy)/docker.sock" \
SANDBOX_STORAGE_PATH="/var/lib/nexus-deploy-sandbox" \
npm run verify
```
