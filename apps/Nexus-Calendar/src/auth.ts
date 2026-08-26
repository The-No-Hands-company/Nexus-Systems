/**
 * Who is asking.
 *
 * Calendar accepts identity from exactly two places, both of which the browser
 * cannot forge:
 *
 *   1. `x-nexus-identity` — an RS256 token the ecosystem proxy mints after
 *      checking the session, scoped to this service's audience. The proxy
 *      strips any inbound copy before setting its own, so a client cannot
 *      supply one.
 *   2. `x-nexus-subject` — the subject Dashboard resolved through Auth, on its
 *      private loopback hop. Trusted ONLY when the request also carries the
 *      shared deployment secret, because unlike (1) this header is a bare
 *      string anyone could type.
 *
 * Anything else is anonymous. In particular a browser-supplied
 * `x-nexus-subject` on its own is ignored: that header arriving from the
 * public internet means nothing at all.
 *
 * NOTE: the RS256/JWKS verification below mirrors apps/Nexus-Dashboard/src/jwt.ts
 * almost line for line. That duplication is deliberate for now and tracked —
 * the ecosystem has three identity mechanisms and no shared contract, and
 * folding both of these into one package is its own piece of work.
 */

import { createVerify, timingSafeEqual } from "node:crypto";

export interface Caller {
  subject: string;
}

interface JWK { kty: string; kid: string; alg: string; n: string; e: string }
interface JWKS { keys: JWK[] }

let jwksCache: JWKS | null = null;
let jwksCacheAt = 0;
const JWKS_CACHE_TTL = 5 * 60 * 1000;

export function __resetAuthCacheForTest(): void {
  jwksCache = null;
  jwksCacheAt = 0;
}

/**
 * Read per call, never captured at module load: a top-level const freezes
 * whatever the environment held when this module was first imported, which in
 * a test run is decided by import order between files.
 */
function authInternalUrl(): string {
  return (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
}

function expectedAudience(): string {
  return process.env.NEXUS_CALENDAR_JWT_AUDIENCE || "calendar.tnhc.dev";
}

async function fetchJWKS(): Promise<JWKS> {
  const now = Date.now();
  if (jwksCache && now - jwksCacheAt < JWKS_CACHE_TTL) return jwksCache;
  const res = await fetch(`${authInternalUrl()}/api/v1/auth/oauth/jwks`, {
    signal: AbortSignal.timeout(3000),
  });
  if (!res.ok) throw new Error("jwks_unavailable");
  const jwks = (await res.json()) as JWKS;
  jwksCache = jwks;
  jwksCacheAt = now;
  return jwks;
}

/** SPKI PEM built from an RSA JWK's modulus and exponent. */
function jwkToPem(jwk: JWK): string {
  const n = Buffer.from(jwk.n, "base64url");
  const e = Buffer.from(jwk.e, "base64url");
  const len = (bytes: number): Buffer =>
    bytes < 0x80 ? Buffer.from([bytes])
    : bytes < 0x100 ? Buffer.from([0x81, bytes])
    : Buffer.from([0x82, bytes >> 8, bytes & 0xff]);
  const integer = (buf: Buffer): Buffer => {
    const padded = buf[0]! & 0x80 ? Buffer.concat([Buffer.from([0]), buf]) : buf;
    return Buffer.concat([Buffer.from([0x02]), len(padded.length), padded]);
  };
  const rsaKey = Buffer.concat([
    Buffer.from([0x30]), len(integer(n).length + integer(e).length), integer(n), integer(e),
  ]);
  const bitString = Buffer.concat([Buffer.from([0x00]), rsaKey]);
  const algorithmId = Buffer.from([
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
  ]);
  const spkiBody = Buffer.concat([
    algorithmId, Buffer.concat([Buffer.from([0x03]), len(bitString.length), bitString]),
  ]);
  const der = Buffer.concat([Buffer.from([0x30]), len(spkiBody.length), spkiBody]);
  const b64 = der.toString("base64").match(/.{1,64}/g) ?? [];
  return `-----BEGIN PUBLIC KEY-----\n${b64.join("\n")}\n-----END PUBLIC KEY-----`;
}

/** Verifies an identity token and returns its subject, or null for any failure. */
async function subjectFromIdentityToken(token: string): Promise<string | null> {
  const parts = token.split(".");
  if (parts.length !== 3) return null;
  const [encodedHeader, encodedPayload, encodedSignature] = parts as [string, string, string];

  let header: { alg?: string; kid?: string };
  try {
    header = JSON.parse(Buffer.from(encodedHeader, "base64url").toString("utf8"));
  } catch {
    return null;
  }

  // Only RS256. Believing the header's own algorithm claim is how "alg: none"
  // and HS256-signed-with-the-public-key forgeries get in.
  if (header.alg !== "RS256" || !header.kid) return null;

  let key: JWK | undefined;
  try {
    key = (await fetchJWKS()).keys.find((k) => k.kid === header.kid);
  } catch {
    return null;
  }
  if (!key) return null;

  const verifier = createVerify("RSA-SHA256");
  verifier.update(`${encodedHeader}.${encodedPayload}`);
  verifier.end();
  try {
    if (!verifier.verify(jwkToPem(key), Buffer.from(encodedSignature, "base64url"))) return null;
  } catch {
    return null;
  }

  let payload: { sub?: unknown; aud?: unknown; exp?: unknown; iat?: unknown };
  try {
    payload = JSON.parse(Buffer.from(encodedPayload, "base64url").toString("utf8"));
  } catch {
    return null;
  }

  const now = Math.floor(Date.now() / 1000);
  if (typeof payload.exp === "number" && payload.exp <= now) return null;
  if (typeof payload.iat === "number" && payload.iat > now + 60) return null;

  // Audience is required, not optional. A token minted for chat is not a token
  // for the calendar, even though the signature is perfectly good.
  const audiences = Array.isArray(payload.aud) ? payload.aud : [payload.aud];
  if (!audiences.includes(expectedAudience())) return null;

  return typeof payload.sub === "string" && payload.sub.trim() ? payload.sub : null;
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

  const identity = req.headers.get("x-nexus-identity");
  if (identity) {
    const subject = await subjectFromIdentityToken(identity);
    if (subject) return { subject };
  }

  return null;
}
