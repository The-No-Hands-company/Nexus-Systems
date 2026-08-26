/**
 * Verification of ecosystem identity tokens minted by Nexus-Auth.
 *
 * The proxy authenticates the browser, mints a short-lived RS256 token
 * describing the signed-in user, and forwards it as `X-Nexus-Identity`. This is
 * the app side of that contract: it turns the header into verified claims, or
 * refuses.
 *
 * It is a TypeScript port of `apps/Nexus/crates/nexus-common/src/identity.rs`,
 * which is the only implementation in the ecosystem that has ever been
 * complete. Two hand-rolled TypeScript verifiers existed alongside it — one in
 * Nexus-Dashboard, one in Nexus-Calendar — and both checked the signature, the
 * expiry and the audience while omitting the two claims that distinguish a user
 * from a machine. This package exists so there is one answer to "who is asking"
 * rather than three, and so a fix lands once.
 *
 * Everything here is about refusing correctly. The header arrives on an
 * ordinary HTTP request, so the only thing separating a real user from someone
 * typing a header by hand is what follows — every rejection path is
 * load-bearing and every one has a test.
 */

import { createVerify } from "node:crypto";

/** The header the proxy sets, and the only one an app should read. */
export const IDENTITY_HEADER = "x-nexus-identity";

/** The issuer Nexus-Auth stamps on every token it mints. */
export const IDENTITY_ISSUER = "nexus-auth";

/**
 * The `typ` that marks a token as describing a *user*.
 *
 * Auth signs its service-to-service tokens with the same key and publishes one
 * kid, so the signature alone cannot tell the two apart — this claim can.
 * `issueServiceToken()` takes both `sub` and `aud` from its caller and sets no
 * `typ` at all, which means a verifier that skips this check treats a service
 * token as whatever user its `sub` happens to name.
 */
export const IDENTITY_TYP = "identity";

/** How long a fetched key set is trusted before a refresh is due. */
const JWKS_TTL_MS = 60 * 60 * 1000;

/**
 * Floor between two refreshes triggered by an unrecognised `kid`.
 *
 * Unknown kids are attacker-controlled: the token is unauthenticated at the
 * point its header is read, so anyone can send a random kid and, without this,
 * make the service hit Auth once per request. Rotation still lands promptly; a
 * flood costs one fetch per interval.
 */
const MIN_REFRESH_MS = 30 * 1000;

/** Tolerance for clock skew between Auth and this host. */
const CLOCK_SKEW_S = 60;

export interface IdentityClaims {
  iss: string;
  sub: string;
  aud: string | string[];
  typ: string;
  exp: number;
  iat?: number;
  email?: string;
  username?: string;
  role?: string;
  [claim: string]: unknown;
}

export type IdentityRejection =
  | "malformed"
  | "unsupported_algorithm"
  | "missing_kid"
  | "unknown_kid"
  | "bad_signature"
  | "expired"
  | "not_yet_valid"
  | "wrong_issuer"
  | "wrong_type"
  | "wrong_audience"
  | "no_subject"
  | "jwks_unavailable";

export type IdentityResult =
  | { ok: true; claims: IdentityClaims }
  | { ok: false; reason: IdentityRejection };

export interface VerifyOptions {
  /** The audience this service accepts. Required — never defaulted. */
  audience: string;
  /** Absolute URL of Auth's JWKS document. */
  jwksUrl: string;
}

interface JWK { kty: string; kid: string; alg?: string; n: string; e: string }
interface JWKS { keys: JWK[] }

interface CacheEntry { jwks: JWKS; fetchedAt: number; lastAttemptAt: number }

const cache = new Map<string, CacheEntry>();
let fetchCount = 0;

export function __resetIdentityCacheForTest(): void {
  cache.clear();
  fetchCount = 0;
}
export function __jwksFetchCountForTest(): number {
  return fetchCount;
}

async function loadJwks(url: string, wantKid: string): Promise<JWKS | null> {
  const now = Date.now();
  const hit = cache.get(url);

  const fresh = hit && now - hit.fetchedAt < JWKS_TTL_MS;
  const hasKid = hit?.jwks.keys.some((k) => k.kid === wantKid) ?? false;

  // A cached set that already contains the key is the common path.
  if (fresh && hasKid) return hit!.jwks;

  // An unknown kid is a reason to refresh — a rotation looks exactly like
  // this — but only if we have not just tried, or it becomes a free amplifier.
  if (hit && !hasKid && now - hit.lastAttemptAt < MIN_REFRESH_MS) return hit.jwks;

  try {
    fetchCount++;
    const res = await fetch(url, { signal: AbortSignal.timeout(3000) });
    if (!res.ok) throw new Error(`jwks ${res.status}`);
    const jwks = (await res.json()) as JWKS;
    cache.set(url, { jwks, fetchedAt: now, lastAttemptAt: now });
    return jwks;
  } catch {
    if (hit) {
      // Record the attempt so a flood of unknown kids cannot retry per request,
      // and keep serving the keys we have: Auth being briefly unreachable
      // should not invalidate tokens it already signed.
      hit.lastAttemptAt = now;
      return hit.jwks;
    }
    return null;
  }
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

function decodeSegment(segment: string): unknown {
  return JSON.parse(Buffer.from(segment, "base64url").toString("utf8"));
}

export async function verifyIdentityToken(token: string, options: VerifyOptions): Promise<IdentityResult> {
  const parts = token.split(".");
  if (parts.length !== 3 || parts.some((p) => !p)) return { ok: false, reason: "malformed" };
  const [encodedHeader, encodedPayload, encodedSignature] = parts as [string, string, string];

  let header: { alg?: unknown; kid?: unknown };
  try {
    header = decodeSegment(encodedHeader) as { alg?: unknown; kid?: unknown };
  } catch {
    return { ok: false, reason: "malformed" };
  }

  // Only RS256, decided here rather than taken from the token. Believing the
  // header's own claim is how "alg: none" and an HS256 forgery signed with the
  // public key get accepted.
  if (header.alg !== "RS256") return { ok: false, reason: "unsupported_algorithm" };
  if (typeof header.kid !== "string" || !header.kid) return { ok: false, reason: "missing_kid" };

  const jwks = await loadJwks(options.jwksUrl, header.kid);
  if (!jwks) return { ok: false, reason: "jwks_unavailable" };

  const key = jwks.keys.find((k) => k.kid === header.kid);
  if (!key) return { ok: false, reason: "unknown_kid" };

  const verifier = createVerify("RSA-SHA256");
  verifier.update(`${encodedHeader}.${encodedPayload}`);
  verifier.end();
  let signatureValid = false;
  try {
    signatureValid = verifier.verify(jwkToPem(key), Buffer.from(encodedSignature, "base64url"));
  } catch {
    signatureValid = false;
  }
  if (!signatureValid) return { ok: false, reason: "bad_signature" };

  let claims: IdentityClaims;
  try {
    claims = decodeSegment(encodedPayload) as IdentityClaims;
  } catch {
    return { ok: false, reason: "malformed" };
  }

  // Order matters only for which reason is reported; every check below is
  // required, and a token must satisfy all of them.
  if (claims.iss !== IDENTITY_ISSUER) return { ok: false, reason: "wrong_issuer" };

  // The check the hand-rolled verifiers were missing. See IDENTITY_TYP.
  if (claims.typ !== IDENTITY_TYP) return { ok: false, reason: "wrong_type" };

  const now = Math.floor(Date.now() / 1000);
  if (typeof claims.exp !== "number" || claims.exp <= now) return { ok: false, reason: "expired" };
  if (typeof claims.iat === "number" && claims.iat > now + CLOCK_SKEW_S) {
    return { ok: false, reason: "not_yet_valid" };
  }

  // Audience is required, not optional-if-present. A token minted for chat is
  // not a token for the calendar, however good its signature.
  const audiences = Array.isArray(claims.aud) ? claims.aud : [claims.aud];
  if (!audiences.includes(options.audience)) return { ok: false, reason: "wrong_audience" };

  if (typeof claims.sub !== "string" || !claims.sub.trim()) return { ok: false, reason: "no_subject" };

  return { ok: true, claims };
}

/**
 * The whole contract in one call: read the agreed header, verify it, and return
 * claims or null. Services that want to know *why* a token was refused — to log
 * it, not to tell the client — should call verifyIdentityToken directly.
 */
export async function identityFromRequest(
  req: Request,
  options: VerifyOptions,
): Promise<IdentityClaims | null> {
  const token = req.headers.get(IDENTITY_HEADER);
  if (!token) return null;
  const result = await verifyIdentityToken(token, options);
  return result.ok ? result.claims : null;
}
