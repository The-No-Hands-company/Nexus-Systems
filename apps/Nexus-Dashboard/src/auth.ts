import {
  verifyIdentityToken,
  IDENTITY_HEADER,
} from "../../../packages/nexus-identity/src/index";

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
 * The audience the proxy mints identity tokens for when it forwards a request
 * to this host. The proxy scopes each token to the hostname it is answering, so
 * this must match the public host, not a service name.
 */
function identityAudience(): string {
  return (
    process.env.NEXUS_DASHBOARD_IDENTITY_AUDIENCE ||
    process.env.NEXUS_DASHBOARD_HOST ||
    `app.${process.env.DOMAIN || "tnhc.dev"}`
  );
}

function identityJwksUrl(): string {
  return `${authInternalUrl()}/api/v1/auth/oauth/jwks`;
}

/**
 * Resolves who is calling.
 *
 * Three credential types are accepted, in order:
 *
 * 1. `x-nexus-identity` — the token the ecosystem proxy mints after checking
 *    the session, verified by @nexus/identity. Dashboard did not read this at
 *    all until now, which is why the proxy's careful minting and stripping of
 *    the header had exactly one consumer in the whole ecosystem.
 * 2. A Bearer JWT, verified against Auth's published JWKS. These are Auth's
 *    *service* tokens: `issueServiceToken()` sets no `typ`, so they are a
 *    deliberately different contract from (1) and are checked by ./jwt rather
 *    than by the identity package, which would reject them by design.
 * 3. The ecosystem session cookie, validated by Auth exactly as before.
 *
 * Explicit credentials win over an ambient cookie, so a stale cookie riding
 * along cannot shadow the credential the caller actually presented.
 */
export async function callerIdentity(req: Request): Promise<Caller | null> {
  const identityToken = req.headers.get(IDENTITY_HEADER);
  if (identityToken) {
    const result = await verifyIdentityToken(identityToken, {
      audience: identityAudience(),
      jwksUrl: identityJwksUrl(),
    });
    if (result.ok) {
      const role = result.claims.role;
      return { subject: result.claims.sub, role: typeof role === "string" ? role : null };
    }
    // For an operator reading logs, never for the caller.
    console.warn(`[dashboard] identity token refused: ${result.reason}`);
  }

  if (req.headers.get("authorization")?.startsWith("Bearer ")) {
    const { validateRequest } = await import("./jwt");
    const fromJwt = await validateRequest(req, process.env.NEXUS_DASHBOARD_JWT_AUDIENCE || "nexus-dashboard");
    if (fromJwt) return fromJwt;
  }
  return sessionIdentity(req);
}