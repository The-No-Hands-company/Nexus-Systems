/** Shared interpretation of the authenticated Dashboard caller. */
export type Caller = {
  subject: string;
  role: string | null;
};

/** Roles permitted to use operator-only Dashboard surfaces. */
export const ADMIN_ROLES = ["founder", "admin"] as const;

export function isAdminRole(role: string | null): boolean {
  return ADMIN_ROLES.some((adminRole) => adminRole === role);
}

function authInternalUrl(): string {
  return (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
}

/**
 * Asks Auth to validate the caller's session cookie. The role and stable
 * subject must always come from Auth, never browser-provided headers.
 */
async function sessionIdentity(req: Request): Promise<Caller | null> {
  try {
    const res = await fetch(`${authInternalUrl()}/api/v1/auth/me`, {
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

/**
 * Resolves who is calling.
 *
 * Two credential types are accepted:
 *
 * 1. A Bearer JWT, verified against Auth's published JWKS without a network
 *    round trip per request (keys cache for five minutes). Intended for API
 *    clients; the audience must be issued for this service.
 * 2. The ecosystem session cookie, validated by Auth exactly as before.
 *
 * JWT is tried first and the cookie is the fallback, so a request carrying a
 * valid Bearer token is authenticated by it even when a stale cookie also
 * rides along — the explicit credential wins.
 */
export async function callerIdentity(req: Request): Promise<Caller | null> {
  if (req.headers.get("authorization")?.startsWith("Bearer ")) {
    const { validateRequest } = await import("./jwt");
    const fromJwt = await validateRequest(req, process.env.NEXUS_DASHBOARD_JWT_AUDIENCE || "nexus-dashboard");
    if (fromJwt) return fromJwt;
  }
  return sessionIdentity(req);
}