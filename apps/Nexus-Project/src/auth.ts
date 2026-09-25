/**
 * Who is asking. The same contract as Nexus-Calendar: identity comes from a
 * trusted front door, from exactly two places the browser cannot forge.
 *
 *   1. `x-nexus-identity` — an RS256 token the ecosystem proxy mints after
 *      checking the session, verified by @nexus/identity (issuer, typ and this
 *      service's audience all checked).
 *   2. `x-nexus-subject` — the subject Dashboard resolved through Auth on its
 *      private loopback hop, believed ONLY alongside the deployment secret,
 *      because unlike (1) this header is a bare string anyone could type.
 *
 * Anything else is anonymous.
 */
import { timingSafeEqual } from "node:crypto";
import { IDENTITY_HEADER, verifyIdentityToken } from "../../../packages/nexus-identity/src/index";

export interface Caller {
  subject: string;
}

/** Read per call, never at module load, so tests can point them elsewhere. */
function jwksUrl(): string {
  const base = (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
  return `${base}/api/v1/auth/oauth/jwks`;
}

function expectedAudience(): string {
  return process.env.NEXUS_PROJECT_JWT_AUDIENCE || "project.tnhc.dev";
}

/** Constant-time comparison that does not leak the length through an early return. */
function secretMatches(presented: string, configured: string): boolean {
  const a = Buffer.from(presented);
  const b = Buffer.from(configured);
  if (a.length !== b.length) {
    timingSafeEqual(b, b);
    return false;
  }
  return timingSafeEqual(a, b);
}

export async function resolveCaller(req: Request): Promise<Caller | null> {
  const configured = process.env.NEXUS_PROJECT_DASHBOARD_SECRET;
  if (configured) {
    const presented = req.headers.get("x-nexus-dashboard-secret");
    if (presented !== null && secretMatches(presented, configured)) {
      const subject = req.headers.get("x-nexus-subject")?.trim();
      // The right secret with no subject is a Dashboard bug, not a caller.
      return subject ? { subject } : null;
    }
  }

  const token = req.headers.get(IDENTITY_HEADER);
  if (token) {
    const result = await verifyIdentityToken(token, {
      audience: expectedAudience(),
      jwksUrl: jwksUrl(),
    });
    if (result.ok) return { subject: result.claims.sub };
    // Logged for the operator, never returned to whoever sent the token.
    console.warn(`[nexus-project] identity token refused: ${result.reason}`);
  }
  return null;
}
