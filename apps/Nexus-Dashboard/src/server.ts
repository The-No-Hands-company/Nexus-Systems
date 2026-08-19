import { join } from "node:path";
import { mergeApps, shellNativeEntries, toAppEntries } from "./apps";
import { cloudBaseUrl, cloudHeaders } from "./cloud";

/**
 * The ecosystem front door — app.<domain>.
 *
 * Serves the dashboard SPA and reverse-proxies the auth API onto the same
 * origin. Same-origin is the whole point: the session cookie is scoped to the
 * parent domain, but a credentialed cross-origin XHR from app.<domain> to
 * auth.<domain> still needs CORS with explicit origins and Allow-Credentials,
 * which is easy to get subtly wrong and fails in ways that look like a login
 * bug. Proxying sidesteps it.
 *
 * This host is deliberately PUBLIC — it carries request-access and claim,
 * which people who are not signed in must be able to reach. Gating it would
 * deadlock exactly as gating the auth host would.
 */

const PORT = Number(process.env.PORT || "3132");
const DOMAIN = process.env.DOMAIN || "tnhc.dev";
const AUTH_HOST = process.env.NEXUS_AUTH_HOST || `auth.${DOMAIN}`;
/** This app's own public host, so it does not list itself in its own grid. */
const SELF_HOST = process.env.NEXUS_DASHBOARD_HOST || `app.${DOMAIN}`;
/**
 * Cloud's public host — not where fetchApps() reads the registry from
 * (CLOUD_URL, below, is the internal address), but the host its own tile's
 * publicUrl carries. Matching on it is how toAppEntries() turns Cloud's tile
 * into an in-shell /cloud link instead of an external one; see apps.ts.
 */
const CLOUD_HOST = process.env.NEXUS_CLOUD_HOST || `cloud.${DOMAIN}`;
const AUTH_INTERNAL_URL = process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310";
const CLOUD_URL = process.env.NEXUS_CLOUD_URL || "http://127.0.0.1:8787";
const CLOUD_API_KEY = process.env.NEXUS_CLOUD_API_KEY || "";
const WEB_ROOT =
  process.env.NEXUS_DASHBOARD_WEB_ROOT || join(import.meta.dir, "..", "frontend", "dist");

/**
 * The only prefix that is proxied. Anything broader would turn this public
 * host into an open relay into the private network.
 */
const AUTH_PREFIX = "/api/v1/auth/";

/**
 * The browser never talks to Cloud. It calls this Dashboard route, which
 * forwards to Cloud with the operator's X-Api-Key attached (see cloud.ts).
 * Handing that key to the browser instead is exactly what this proxy exists
 * to avoid, so the mapping below is a fixed allow-list, not a passthrough:
 * only these Cloud paths are reachable, and every one of them is read-only.
 * Anything not in this table 404s, and no other method than GET is served.
 */
const CLOUD_PREFIX = "/api/cloud/";

/**
 * Mail lives behind its own service on loopback. Unlike the Cloud proxy, which
 * attaches an operator key, this one attaches the *caller's identity*: the mail
 * API trusts X-Nexus-Subject to decide whose mailbox to open.
 *
 * That means this proxy is the only thing standing between a signed-in user and
 * everyone else's mail, so the subject is taken from Auth on every request and
 * never from anything the browser sent. An inbound X-Nexus-Subject is stripped
 * before forwarding — without that, any user could read any mailbox by setting
 * one header.
 */
const MAIL_PREFIX = "/api/mail/";

/**
 * Read per call rather than captured at module load. A top-level const freezes
 * whatever the environment held the first time this module was imported, which
 * in a test run is decided by import order between files — the same trap
 * documented in cloud-proxy.test.ts. A function has no such ordering.
 */
function mailUrl(): string {
  return (process.env.NEXUS_EMAIL_URL || "http://127.0.0.1:3140").replace(/\/+$/, "");
}

async function proxyToMail(req: Request, rest: string, search: string): Promise<Response> {
  const who = await callerIdentity(req);
  if (!who) return Response.json({ error: "not_authenticated" }, { status: 401 });

  // Only the mail API's own surface, and only the methods it serves.
  if (!/^(folders|messages|search|threads)(\/|$)/.test(rest)) {
    return Response.json({ error: "not_found" }, { status: 404 });
  }
  if (req.method !== "GET" && req.method !== "POST") {
    return Response.json({ error: "method_not_allowed" }, { status: 405 });
  }

  const headers = new Headers();
  headers.set("content-type", req.headers.get("content-type") ?? "application/json");
  headers.set("x-nexus-subject", who.subject);

  try {
    const res = await fetch(`${mailUrl()}/api/v1/${rest}${search}`, {
      method: req.method,
      headers,
      body: req.method === "POST" ? await req.text() : undefined,
      signal: AbortSignal.timeout(10000),
    });
    // Attachments come back as bytes with headers that matter — the
    // Content-Disposition that forces a download and the nosniff that stops
    // the browser guessing a type. Passing the body through as text would
    // corrupt binary files, and dropping those headers would let an HTML
    // attachment render inline on this origin.
    const passthrough = new Headers();
    for (const h of [
      "content-type",
      "content-disposition",
      "x-content-type-options",
      "content-security-policy",
    ]) {
      const v = res.headers.get(h);
      if (v) passthrough.set(h, v);
    }
    if (!passthrough.has("content-type")) passthrough.set("content-type", "application/json");
    return new Response(res.body, { status: res.status, headers: passthrough });
  } catch {
    // Mail being down must not take the shell page with it.
    return Response.json({ error: "mail_unavailable" }, { status: 503 });
  }
}
const CLOUD_ALLOWLIST: Record<string, { upstream: string; adminOnly?: boolean }> = {
  // Overview (view-dashboard): status.html's loadDashboard() calls these.
  status: { upstream: "/api/v1/status" },
  tools: { upstream: "/api/v1/tools" },
  audit: { upstream: "/api/v1/audit" },
  // Tools/API views (view-tools, view-api).
  endpoints: { upstream: "/api/v1/endpoints" },
  // Admin-only: the one Cloud path that lists every account, and hiding a tab
  // client-side is not a control.
  //
  // No view calls this. Cloud has delegated accounts to Nexus-Auth — its own
  // POST /api/v1/users answers 410 saying so — and its GET requires a Cloud
  // session that SSO no longer issues, so it returns 401 even to the
  // operator's key. The entry stays because it is the enforcement point a real
  // users view will need when one is built against Auth, and because it is the
  // only adminOnly route, so deleting it would take the tests covering that
  // mechanism with it.
  users: { upstream: "/api/v1/users", adminOnly: true },
  // Federation + identity views.
  "federation/peers": { upstream: "/v1/federation/peers" },
  "federation/identity": { upstream: "/v1/federation/identity" },
};

/** Mirrors frontend/src/api.ts ADMIN_ROLES — keep the two in sync. */
const ADMIN_ROLES = ["founder", "admin"];

/**
 * Who is making this request, per Auth's own session cookie check — the same
 * mechanism the account/admin surfaces rely on (frontend/src/api.ts `me()`,
 * proxied through AUTH_PREFIX). Cloud has no notion of a Nexus session, so
 * the Dashboard server has to ask Auth on the caller's behalf rather than
 * trusting anything the browser claims about itself.
 */
/**
 * Who Auth says this request is. Returns the ecosystem subject (the stable
 * user id) alongside the role, because mail is per-user: the mail service needs
 * to know *whose* mailbox to open, not merely that somebody is signed in.
 */
async function callerIdentity(
  req: Request,
): Promise<{ subject: string; role: string | null } | null> {
  try {
    const res = await fetch(`${AUTH_INTERNAL_URL.replace(/\/+$/, "")}/api/v1/auth/me`, {
      headers: { cookie: req.headers.get("cookie") ?? "" },
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) return null;
    const body = (await res.json().catch(() => null)) as {
      user?: { id?: unknown; role?: unknown };
    } | null;
    const id = body?.user?.id;
    if (typeof id !== "string" || !id) return null;
    const role = body?.user?.role;
    return { subject: id, role: typeof role === "string" ? role : null };
  } catch {
    return null;
  }
}

async function callerRole(req: Request): Promise<string | null> {
  try {
    const res = await fetch(`${AUTH_INTERNAL_URL.replace(/\/+$/, "")}/api/v1/auth/me`, {
      headers: { cookie: req.headers.get("cookie") ?? "" },
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) return null;
    const body = (await res.json().catch(() => null)) as { user?: { role?: unknown } } | null;
    const role = body?.user?.role;
    return typeof role === "string" ? role : null;
  } catch {
    // Auth unreachable is a "no" for an admin-only resource, not a crash.
    return null;
  }
}

async function proxyToCloud(req: Request, name: string, search: string): Promise<Response> {
  // Object.hasOwn, not a bare lookup. `name` comes from the URL, and every
  // object inherits truthy `constructor`, `__proto__` and `toString`, so
  // `CLOUD_ALLOWLIST["constructor"]` passes an `if (!entry)` guard, skips the
  // adminOnly check (undefined), and forwards to `undefined` — an
  // attacker-chosen request to the control plane with the operator key
  // attached. Own-property only.
  const entry = Object.hasOwn(CLOUD_ALLOWLIST, name) ? CLOUD_ALLOWLIST[name] : undefined;
  // Off the allow-list. This must never fall through to a generic forward.
  if (!entry) return Response.json({ error: "not_found" }, { status: 404 });

  if (entry.adminOnly) {
    const role = await callerRole(req);
    if (!role || !ADMIN_ROLES.includes(role)) {
      return Response.json({ error: "forbidden" }, { status: 403 });
    }
  }

  try {
    const res = await fetch(`${cloudBaseUrl()}${entry.upstream}${search}`, {
      headers: cloudHeaders(),
      signal: AbortSignal.timeout(5000),
    });
    const body = await res.text();
    // Only the content-type rides along. Copying every upstream header would
    // risk carrying something Cloud-specific (and is more than the ported
    // views need); it never includes the outbound X-Api-Key, which is a
    // request header we set, not one Cloud echoes back.
    return new Response(body, {
      status: res.status,
      headers: { "content-type": res.headers.get("content-type") ?? "application/json" },
    });
  } catch {
    // Cloud must be assumed to be down sometimes. The account pages keep
    // working without it (see fetchApps below); this proxy degrades the same
    // way instead of throwing a 500 that would take a shell page down with it.
    return Response.json({ error: "cloud_unavailable" }, { status: 503 });
  }
}

/**
 * Is the mail API answering?
 *
 * Any HTTP response proves the service is up and routing — the mail API has no
 * /health route, so a 404 here is a live server saying "not that path", which
 * is exactly the fact we need. Only a connection failure or timeout is offline.
 */
async function mailReachable(): Promise<boolean> {
  try {
    await fetch(`${mailUrl()}/`, { signal: AbortSignal.timeout(2000) });
    return true;
  } catch {
    return false;
  }
}

async function fetchApps(): Promise<Response> {
  // Probed once and used on both paths: the shell's own views must not
  // disappear just because Cloud is down.
  const native = shellNativeEntries({ mailHealthy: await mailReachable() });

  try {
    const res = await fetch(`${CLOUD_URL.replace(/\/+$/, "")}/api/v1/tools`, {
      headers: CLOUD_API_KEY ? { "X-Api-Key": CLOUD_API_KEY } : {},
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) return Response.json({ apps: native });
    const registry = toAppEntries(await res.json(), AUTH_HOST, SELF_HOST, CLOUD_HOST);
    return Response.json({ apps: mergeApps(registry, native) });
  } catch {
    // Cloud being down must degrade to the shell's own views, not a broken
    // dashboard — the account pages still work and the user can still sign in.
    return Response.json({ apps: native });
  }
}

async function proxyToAuth(req: Request, path: string): Promise<Response> {
  const incoming = new URL(req.url);
  const upstream = new URL(AUTH_INTERNAL_URL);
  upstream.pathname = path;
  upstream.search = incoming.search;

  const headers = new Headers(req.headers);
  // The body is re-read into a buffer below, so the inbound framing headers no
  // longer describe it; host must go too or Auth sees the wrong vhost.
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

  // GET-only, and only for names on CLOUD_ALLOWLIST — anything else (a
  // mutating method, or a name not in the table) falls through to the
  // catch-all 404 below rather than reaching proxyToCloud at all.
  if (path.startsWith(MAIL_PREFIX)) {
    return proxyToMail(req, path.slice(MAIL_PREFIX.length), url.search);
  }

  if (req.method === "GET" && path.startsWith(CLOUD_PREFIX)) {
    return proxyToCloud(req, path.slice(CLOUD_PREFIX.length), url.search);
  }

  if (path.startsWith(AUTH_PREFIX)) return proxyToAuth(req, path);

  // Anything else under /api is not ours and must not fall through to the SPA.
  // Returning the HTML shell for a mistyped API call is how a caller ends up
  // parsing "<!doctype html>" as JSON.
  if (path.startsWith("/api/")) {
    return Response.json({ error: "not_found" }, { status: 404 });
  }

  // A real asset if we have one, otherwise the SPA shell so client-side routes
  // survive a hard reload.
  if (path !== "/") {
    const asset = Bun.file(join(WEB_ROOT, path));
    if (await asset.exists()) return new Response(asset);
  }

  const shell = Bun.file(join(WEB_ROOT, "index.html"));
  if (await shell.exists()) {
    return new Response(shell, {
      headers: {
        "content-type": "text/html; charset=utf-8",
        // The shell is the authenticated front door — launcher, account, admin.
        // Unlike the apps it frames, nothing frames the shell, so it permits
        // nobody: 'self' only, never the apps' origins.
        "content-security-policy": "frame-ancestors 'self'",
      },
    });
  }

  // The SPA has not been built. `frontend/dist` is a build artifact and is
  // gitignored, so a fresh clone or a deploy that skipped `npm run build`
  // lands here. Say so plainly — the alternative is an unexplained blank page
  // or a stack trace, and the fix is one command.
  return new Response(
    '<!doctype html><meta charset="utf-8"><title>Nexus Dashboard</title>' +
      "<p>The dashboard UI has not been built. Run <code>npm install &amp;&amp; npm run build</code> " +
      "in <code>apps/Nexus-Dashboard/frontend</code>.</p>",
    { status: 503, headers: { "content-type": "text/html; charset=utf-8" } },
  );
}

export function startServer() {
  // Loopback unless told otherwise — Bun.serve binds every interface by
  // default, and the proxy is the only intended client.
  const server = Bun.serve({
    port: PORT,
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    fetch: handleRequest,
  });
  console.log(`[nexus-dashboard] Listening on port ${server.port}`);
  return server;
}

if (import.meta.main) startServer();
