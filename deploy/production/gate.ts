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

/**
 * Where the proxy reaches Auth internally. Never a public URL — that would
 * send the request back out through Cloudflare and into this proxy again,
 * which is the same trap NEXUS_AUTH_BASE_URL documents in deploy.sh.
 */
const AUTH_INTERNAL_URL = process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310";

const SESSION_COOKIE = "nexus_session";

/** Normal freshness. Beyond this an entry is revalidated against Auth. */
export const FRESH_MS = 60_000;
/**
 * How long a stale entry may still be served while Auth is unreachable.
 *
 * Bounded by the identity token's own lifetime, not by how long we would
 * *like* to ride out an outage. Auth mints these for 120 seconds
 * (IDENTITY_TOKEN_TTL_SECONDS), so a cached token is worthless the moment it
 * expires — the app verifies `exp` and returns 401. This was 15 minutes, which
 * meant all but the first two of them handed out tokens guaranteed to be
 * rejected downstream.
 *
 * Surviving a longer Auth outage is possible, but it needs a longer token TTL,
 * which is a different trade: a longer-lived token is a longer-lived thing to
 * steal.
 */
export const STALE_MS = 100_000;

type CacheEntry = { token: string; fetchedAt: number };
const identityCache = new Map<string, CacheEntry>();

/**
 * Entries are only ever written for sessions Auth accepted, so this cannot be
 * grown by an attacker — but it was never pruned either, so every (session,
 * host) pair a real user ever visited stayed for the life of the process.
 * Sweeping on write keeps it proportional to who is actually online.
 */
function sweepExpired(now: number): void {
  for (const [k, v] of identityCache) {
    if (now - v.fetchedAt >= STALE_MS) identityCache.delete(k);
  }
}

export function __resetGateForTest(): void {
  identityCache.clear();
}

/**
 * Every value sent under `name`, not just the first.
 *
 * A browser will happily hold two cookies with the same name at different
 * scopes — one host-only on chat.tnhc.dev, one on .tnhc.dev — and sends both,
 * most-specific first, with no way for the server to tell them apart. Taking
 * only the first meant a single stale host-scoped cookie shadowed the good
 * ecosystem session permanently: the gate would refuse, redirect the app's own
 * fetch to the sign-in host, and the page's CSP would block the redirect, so
 * the user saw "not signed in" on a host they were signed in to, with no way
 * to clear it short of wiping cookies by hand.
 */
export function readCookies(req: Request, name: string): string[] {
  const header = req.headers.get("cookie");
  if (!header) return [];
  const out: string[] = [];
  for (const part of header.split(";")) {
    const eq = part.indexOf("=");
    if (eq === -1) continue;
    if (part.slice(0, eq).trim() !== name) continue;
    const raw = part.slice(eq + 1).trim();
    try {
      const value = decodeURIComponent(raw);
      if (value) out.push(value);
    } catch {
      // Undecodable is simply not a session; keep looking at the others.
    }
  }
  return out;
}

function readCookie(req: Request, name: string): string | null {
  const header = req.headers.get("cookie");
  if (!header) return null;
  for (const part of header.split(";")) {
    const eq = part.indexOf("=");
    if (eq === -1) continue;
    if (part.slice(0, eq).trim() !== name) continue;
    const raw = part.slice(eq + 1).trim();
    try {
      return decodeURIComponent(raw);
    } catch {
      // decodeURIComponent throws on malformed escapes like "%zz". Unhandled,
      // that turned any request carrying a broken cookie into a 500 from the
      // proxy — a crash anyone could trigger with one header, on the one code
      // path every gated request goes through. A cookie we cannot decode is
      // simply not a session.
      return null;
    }
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

/**
 * The address the browser actually asked for, as the browser would write it.
 *
 * `req.url` here is always `http://` — Cloudflare terminates TLS at the edge
 * and the tunnel hands this proxy plain HTTP — so passing it straight to
 * `isRedirectAllowed`, which requires https, made *every* gated login discard
 * the return address and bounce to the apex. The path was being thrown away at
 * exactly the moment it mattered.
 *
 * Rebuilding with https is safe because the scheme was never the security
 * property: the host is, and it is still checked. Nothing reachable through
 * this proxy is served over plain HTTP publicly.
 */
export function publicUrl(req: Request): string {
  const url = new URL(req.url);
  url.protocol = "https:";
  // Strip a port the origin only sees because of the tunnel hop; the public
  // address has none, and leaving :8080 on would fail the host check.
  url.port = "";
  return url.toString();
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

  const key = `${audience} ${cookie}`;
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
    const now = Date.now();
    sweepExpired(now);
    identityCache.set(key, { token, fetchedAt: now });
    return token;
  } catch {
    // Auth unreachable — distinct from Auth saying no. Keep an existing
    // session working rather than logging the whole ecosystem out because the
    // identity service restarted.
    if (hit && age < STALE_MS) return hit.token;
    return null;
  }
}

/**
 * Hosts that are ALWAYS public, regardless of what Cloud's route table says.
 *
 * auth.<domain> is the login page itself — gating it would deadlock.
 *
 * Everything else on *.tnhc.dev requires a signed-in session. This is enforced
 * HERE rather than in Cloud's per-tool requiresAuth flag because:
 *  - The policy is "nothing public except login" — a single boolean here is
 *    easier to audit than N flags across M tools.
 *  - Cloud's buildTool() drops requiresAuth on initial registration and has
 *    no endpoint to set it, so the route-table path is unreliable.
 *  - If a service ever genuinely needs a public endpoint, it can be added to
 *    this set explicitly rather than by forgetting to set a flag.
 */
const PUBLIC_HOSTS = new Set([AUTH_HOST]);

/** Paths exempt from the auth gate on ANY host (deploy.sh probes these). */
const PUBLIC_PATHS = new Set(["/health", "/health/live", "/health/ready"]);

/** Decides whether a request may proceed, and with what identity. */
export async function gate(
  req: Request,
  target: RouteTarget,
): Promise<{ allow: true; identityToken: string | null } | { allow: false; response: Response }> {
  const url = new URL(req.url);
  const host = url.hostname.toLowerCase();

  // Structural allowlist — checked before policy, so no route row can gate it.
  if (PUBLIC_HOSTS.has(host)) return { allow: true, identityToken: null };

  // Test bypass: proxy.test.ts exercises forwarding logic, not the auth gate.
  // Never set in production.
  if (process.env.GATE_SKIP_AUTH === "true") return { allow: true, identityToken: null };

  // Health checks are always public.
  if (PUBLIC_PATHS.has(url.pathname)) return { allow: true, identityToken: null };

  // Static assets and the SPA shell are always public. Hashed filenames carry
  // no user data; blocking them prevents the SPA from loading at all. Auth is
  // enforced on the API layer (/ipa/*), not the static file layer.
  if (url.pathname.startsWith("/assets/") ||
      url.pathname === "/index.html" ||
      !url.pathname.includes(".") && !url.pathname.startsWith("/ipa/") && !url.pathname.startsWith("/api/")) {
    return { allow: true, identityToken: null };
  }

  // Default-deny: every other host requires a signed-in session.
  // The old behaviour read requiresAuth from Cloud's route table, but that
  // field doesn't survive tool registration (buildTool drops it) and has no
  // setter endpoint. Enforcing here is the single point of truth.

  // Try every candidate: one stale cookie must not shadow a valid one.
  let identityToken: string | null = null;
  for (const candidate of readCookies(req, SESSION_COOKIE)) {
    identityToken = await resolveIdentity(candidate, host);
    if (identityToken) break;
  }
  if (!identityToken) return { allow: false, response: loginRedirect(publicUrl(req)) };

  return { allow: true, identityToken };
}
