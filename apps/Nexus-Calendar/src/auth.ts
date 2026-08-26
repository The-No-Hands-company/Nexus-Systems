/**
 * Who is asking.
 *
 * Calendar accepts identity from exactly two places, both of which the browser
 * cannot forge:
 *
 *   1. `x-nexus-identity` — an RS256 token the ecosystem proxy mints after
 *      checking the session, scoped to this service's audience. The proxy
 *      strips any inbound copy before setting its own, so a client cannot
 *      supply one. Verified by @nexus/identity, which is the one implementation
 *      of that contract in the ecosystem.
 *   2. `x-nexus-subject` — the subject Dashboard resolved through Auth, on its
 *      private loopback hop. Trusted ONLY when the request also carries the
 *      shared deployment secret, because unlike (1) this header is a bare
 *      string anyone could type.
 *
 * Anything else is anonymous. In particular a browser-supplied
 * `x-nexus-subject` on its own is ignored: that header arriving from the
 * public internet means nothing at all.
 */

import { timingSafeEqual } from "node:crypto";
import {
  verifyIdentityToken,
  IDENTITY_HEADER,
  __resetIdentityCacheForTest,
} from "../../../packages/nexus-identity/src/index";

export interface Caller {
  subject: string;
}

export function __resetAuthCacheForTest(): void {
  __resetIdentityCacheForTest();
}

/**
 * Read per call, never captured at module load: a top-level const freezes
 * whatever the environment held when this module was first imported, which in
 * a test run is decided by import order between files.
 */
function jwksUrl(): string {
  const base = (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
  return `${base}/api/v1/auth/oauth/jwks`;
}

function expectedAudience(): string {
  return process.env.NEXUS_CALENDAR_JWT_AUDIENCE || "calendar.tnhc.dev";
}

/** Constant-time comparison that does not leak length through early return. */
function secretMatches(presented: string, configured: string): boolean {
  const a = Buffer.from(presented);
  const b = Buffer.from(configured);
  if (a.length !== b.length) {
    // Still burn a comparison so a wrong length is not measurably faster.
    timingSafeEqual(b, b);
    return false;
  }
  return timingSafeEqual(a, b);
}

export async function resolveCaller(req: Request): Promise<Caller | null> {
  // The private Dashboard hop, gated on a secret that never reaches a browser.
  const configured = process.env.NEXUS_CALENDAR_DASHBOARD_SECRET;
  if (configured) {
    const presented = req.headers.get("x-nexus-dashboard-secret");
    if (presented !== null && secretMatches(presented, configured)) {
      const subject = req.headers.get("x-nexus-subject")?.trim();
      if (subject) return { subject };
      // Correct secret but no usable subject: a Dashboard bug, not a caller.
      return null;
    }
  }

  const token = req.headers.get(IDENTITY_HEADER);
  if (token) {
    const result = await verifyIdentityToken(token, {
      audience: expectedAudience(),
      jwksUrl: jwksUrl(),
    });
    if (result.ok) return { subject: result.claims.sub };
    // Logged, never returned: the reason is for an operator reading logs, not
    // for whoever sent the token.
    console.warn(`[nexus-calendar] identity token refused: ${result.reason}`);
  }

  return null;
}
