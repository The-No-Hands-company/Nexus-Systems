/**
 * JWT validation for Dashboard API.
 *
 * Validates RS256 JWTs issued by Nexus-Auth against its published JWKS.
 * Tokens are audience-scoped: when an expected audience is supplied, a token
 * minted for a different audience is rejected even though the signature holds.
 */

import { createVerify } from "node:crypto";

interface JWK {
  kty: string;
  use: string;
  kid: string;
  alg: string;
  n: string;
  e: string;
}

interface JWKS {
  keys: JWK[];
}

export interface JWTPayload {
  iss: string;
  sub: string;
  aud: string | string[];
  exp?: number;
  iat?: number;
  jti?: string;
  role?: string | null;
  [key: string]: unknown;
}

let jwksCache: JWKS | null = null;
let jwksCacheAt = 0;
const JWKS_CACHE_TTL = 5 * 60 * 1000;

/**
 * Read per call rather than captured at module load. A top-level const freezes
 * whatever the environment held the first time this module was imported, which
 * in a test run is decided by import order between files — the same trap
 * documented for mailUrl() in server.ts.
 */
function authInternalUrl(): string {
  return (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
}

async function fetchJWKS(): Promise<JWKS> {
  const now = Date.now();
  if (jwksCache && now - jwksCacheAt < JWKS_CACHE_TTL) return jwksCache;

  const res = await fetch(`${authInternalUrl()}/api/v1/auth/oauth/jwks`, {
    signal: AbortSignal.timeout(3000),
  });
  if (!res.ok) throw new Error("Failed to fetch JWKS");
  const jwks = (await res.json()) as JWKS;
  jwksCache = jwks;
  jwksCacheAt = now;
  return jwks;
}

function base64UrlDecode(value: string): string {
  return Buffer.from(value, "base64url").toString("utf8");
}

/** Spki PEM built from an RSA JWK's modulus/exponent. */
function jwkToPem(jwk: JWK): string {
  // DER encoding of an SPKI RSA public key:
  // SEQUENCE { SEQ{oid,NULL}, BITSTRING{ SEQ { INTEGER n, INTEGER e } } }
  const n = Buffer.from(jwk.n, "base64url");
  const e = Buffer.from(jwk.e, "base64url");

  function len(bytes: number): Buffer {
    if (bytes < 0x80) return Buffer.from([bytes]);
    if (bytes < 0x100) return Buffer.from([0x81, bytes]);
    return Buffer.from([0x82, bytes >> 8, bytes & 0xff]);
  }
  function integer(buf: Buffer): Buffer {
    // Positive integers need a leading zero byte when the high bit is set.
    const padded = buf[0]! & 0x80 ? Buffer.concat([Buffer.from([0]), buf]) : buf;
    return Buffer.concat([Buffer.from([0x02]), len(padded.length), padded]);
  }

  // RSAPublicKey is itself a SEQUENCE wrapping the two integers.
  const rsaKey = Buffer.concat([Buffer.from([0x30]), len(integer(n).length + integer(e).length), integer(n), integer(e)]);
  const bitString = Buffer.concat([Buffer.from([0x00]), rsaKey]);
  const algorithmId = Buffer.from([
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
  ]);
  const spkiBody = Buffer.concat([algorithmId, Buffer.concat([Buffer.from([0x03]), len(bitString.length), bitString])]);
  const der = Buffer.concat([Buffer.from([0x30]), len(spkiBody.length), spkiBody]);

  const b64 = der.toString("base64").match(/.{1,64}/g) ?? [];
  return `-----BEGIN PUBLIC KEY-----\n${b64.join("\n")}\n-----END PUBLIC KEY-----`;
}

export class JWTValidationError extends Error {
  constructor(message: string, public readonly code: string) {
    super(message);
    this.name = "JWTValidationError";
  }
}

export async function validateJWT(token: string, expectedAudience?: string): Promise<JWTPayload> {
  const parts = token.split(".");
  if (parts.length !== 3) {
    throw new JWTValidationError("Invalid token format", "INVALID_FORMAT");
  }
  const [encodedHeader, encodedPayload, encodedSignature] = parts;

  let header: { alg?: string; kid?: string };
  try {
    header = JSON.parse(base64UrlDecode(encodedHeader));
  } catch {
    throw new JWTValidationError("Invalid header", "INVALID_HEADER");
  }

  // Reject any declared algorithm other than RS256 outright — accepting the
  // header's own claim unvalidated is the classic alg-confusion downgrade.
  if (header.alg !== "RS256") {
    throw new JWTValidationError("Unsupported algorithm", "UNSUPPORTED_ALGORITHM");
  }
  if (!header.kid) {
    throw new JWTValidationError("Missing key ID", "MISSING_KID");
  }

  const jwks = await fetchJWKS();
  const key = jwks.keys.find((k) => k.kid === header.kid);
  if (!key) throw new JWTValidationError("Key not found", "KEY_NOT_FOUND");

  const verifier = createVerify("RSA-SHA256");
  verifier.update(`${encodedHeader}.${encodedPayload}`);
  verifier.end();
  let signatureValid = false;
  try {
    signatureValid = verifier.verify(jwkToPem(key), Buffer.from(encodedSignature, "base64url"));
  } catch {
    signatureValid = false;
  }
  if (!signatureValid) {
    throw new JWTValidationError("Invalid signature", "INVALID_SIGNATURE");
  }

  let payload: JWTPayload;
  try {
    payload = JSON.parse(base64UrlDecode(encodedPayload));
  } catch {
    throw new JWTValidationError("Invalid payload", "INVALID_PAYLOAD");
  }

  const now = Math.floor(Date.now() / 1000);
  if (typeof payload.exp === "number" && payload.exp <= now) {
    throw new JWTValidationError("Token expired", "TOKEN_EXPIRED");
  }
  if (typeof payload.iat === "number" && payload.iat > now + 60) {
    throw new JWTValidationError("Token not yet valid", "TOKEN_NOT_YET_VALID");
  }
  if (payload.aud !== undefined && expectedAudience) {
    const audiences = Array.isArray(payload.aud) ? payload.aud : [payload.aud];
    if (!audiences.includes(expectedAudience)) {
      throw new JWTValidationError("Audience mismatch", "AUDIENCE_MISMATCH");
    }
  }

  return payload;
}

export interface AuthContext {
  subject: string;
  role: string | null;
}

/**
 * Validate a request's Bearer token, if it carries one.
 *
 * Returns null both when no Authorization header is present and when the
 * token fails validation — callers fall back to session-cookie identity,
 * which keeps browser flows working unchanged while API clients can present
 * a JWT instead.
 */
export async function validateRequest(req: Request, expectedAudience?: string): Promise<AuthContext | null> {
  const auth = req.headers.get("authorization");
  if (!auth?.startsWith("Bearer ")) return null;
  const token = auth.slice(7).trim();
  if (!token) return null;

  try {
    const payload = await validateJWT(token, expectedAudience);
    return { subject: payload.sub, role: payload.role ?? null };
  } catch {
    return null;
  }
}