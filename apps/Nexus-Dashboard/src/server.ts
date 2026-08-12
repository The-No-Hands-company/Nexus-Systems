import { join } from "node:path";
import { toAppEntries } from "./apps";

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
const AUTH_INTERNAL_URL = process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310";
const CLOUD_URL = process.env.NEXUS_CLOUD_URL || "http://127.0.0.1:8787";
const CLOUD_API_KEY = process.env.NEXUS_CLOUD_API_KEY || "";
const WEB_ROOT = process.env.NEXUS_DASHBOARD_WEB_ROOT || join(import.meta.dir, "..", "frontend", "dist");

/**
 * The only prefix that is proxied. Anything broader would turn this public
 * host into an open relay into the private network.
 */
const AUTH_PREFIX = "/api/v1/auth/";

async function fetchApps(): Promise<Response> {
  try {
    const res = await fetch(`${CLOUD_URL.replace(/\/+$/, "")}/api/v1/tools`, {
      headers: CLOUD_API_KEY ? { "X-Api-Key": CLOUD_API_KEY } : {},
      signal: AbortSignal.timeout(3000),
    });
    if (!res.ok) return Response.json({ apps: [] });
    return Response.json({ apps: toAppEntries(await res.json(), AUTH_HOST, SELF_HOST) });
  } catch {
    // Cloud being down must degrade to an empty grid, not a broken dashboard —
    // the account pages still work and the user can still sign in.
    return Response.json({ apps: [] });
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
    return new Response(shell, { headers: { "content-type": "text/html; charset=utf-8" } });
  }

  // The SPA has not been built. `frontend/dist` is a build artifact and is
  // gitignored, so a fresh clone or a deploy that skipped `npm run build`
  // lands here. Say so plainly — the alternative is an unexplained blank page
  // or a stack trace, and the fix is one command.
  return new Response(
    "<!doctype html><meta charset=\"utf-8\"><title>Nexus Dashboard</title>" +
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
