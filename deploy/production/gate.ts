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
    identityCache.set(key, { token, fetchedAt: Date.now() });
    return token;
  } catch {
    // Auth unreachable — distinct from Auth saying no. Keep an existing
    // session working rather than logging the whole ecosystem out because the
    // identity service restarted.
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

  // Public routes pay nothing: no cookie read, no call to Auth, no token.
  if (!target.requiresAuth) return { allow: true, identityToken: null };

  const identityToken = await resolveIdentity(readCookie(req, SESSION_COOKIE), host);
  if (!identityToken) return { allow: false, response: loginRedirect(publicUrl(req)) };

  return { allow: true, identityToken };
}
