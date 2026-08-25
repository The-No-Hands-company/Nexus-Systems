import { createVerify, timingSafeEqual } from "node:crypto";

type Jwk = {
  kty: string;
  kid: string;
  alg?: string;
  use?: string;
  n: string;
  e: string;
};

type Jwks = { keys: Jwk[] };

type JwtHeader = { alg?: unknown; kid?: unknown };
type JwtPayload = { sub?: unknown; aud?: unknown; exp?: unknown; iat?: unknown };

let cachedJwks: Jwks | null = null;
let cachedJwksAt = 0;
const JWKS_CACHE_TTL_MS = 5 * 60 * 1000;

function authInternalUrl(): string {
  return (process.env.NEXUS_AUTH_INTERNAL_URL || "http://127.0.0.1:4310").replace(/\/+$/, "");
}

function audience(): string {
  return process.env.NEXUS_CALENDAR_JWT_AUDIENCE || "calendar.tnhc.dev";
}

function decodeJson(value: string): unknown {
  return JSON.parse(Buffer.from(value, "base64url").toString("utf8"));
}

function jwtJwkToPem(jwk: Jwk): string {
  const modulus = Buffer.from(jwk.n, "base64url");
  const exponent = Buffer.from(jwk.e, "base64url");
  const length = (size: number): Buffer => {
    if (size < 0x80) return Buffer.from([size]);
    if (size < 0x100) return Buffer.from([0x81, size]);
    return Buffer.from([0x82, size >> 8, size & 0xff]);
  };
  const integer = (value: Buffer): Buffer => {
    const positive = value[0]! & 0x80 ? Buffer.concat([Buffer.from([0]), value]) : value;
    return Buffer.concat([Buffer.from([0x02]), length(positive.length), positive]);
  };
  const rsaKey = Buffer.concat([
    Buffer.from([0x30]),
    length(integer(modulus).length + integer(exponent).length),
    integer(modulus),
    integer(exponent),
  ]);
  const bitString = Buffer.concat([Buffer.from([0]), rsaKey]);
  const algorithmId = Buffer.from([
    0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00,
  ]);
  const body = Buffer.concat([algorithmId, Buffer.concat([Buffer.from([0x03]), length(bitString.length), bitString])]);
  const der = Buffer.concat([Buffer.from([0x30]), length(body.length), body]);
  const lines = der.toString("base64").match(/.{1,64}/g) ?? [];
  return `-----BEGIN PUBLIC KEY-----\n${lines.join("\n")}\n-----END PUBLIC KEY-----`;
}

async function fetchJwks(): Promise<Jwks> {
  const now = Date.now();
  if (cachedJwks && now - cachedJwksAt < JWKS_CACHE_TTL_MS) return cachedJwks;

  const response = await fetch(`${authInternalUrl()}/api/v1/auth/oauth/jwks`, {
    signal: AbortSignal.timeout(3000),
  });
  if (!response.ok) throw new Error("jwks_unavailable");
  const jwks = await response.json() as Jwks;
  if (!Array.isArray(jwks.keys)) throw new Error("invalid_jwks");
  cachedJwks = jwks;
  cachedJwksAt = now;
  return jwks;
}

function hasExpectedAudience(value: unknown): boolean {
  return (Array.isArray(value) ? value : [value]).some((item) => item === audience());
}

async function subjectFromIdentity(token: string): Promise<string | null> {
  const parts = token.split(".");
  if (parts.length !== 3) return null;
  const [encodedHeader, encodedPayload, encodedSignature] = parts;
  if (!encodedHeader || !encodedPayload || !encodedSignature) return null;

  let header: JwtHeader;
  let payload: JwtPayload;
  try {
    header = decodeJson(encodedHeader) as JwtHeader;
    payload = decodeJson(encodedPayload) as JwtPayload;
  } catch {
    return null;
  }
  if (header.alg !== "RS256" || typeof header.kid !== "string" || !header.kid) return null;
  if (typeof payload.sub !== "string" || !payload.sub.trim()) return null;
  if (!hasExpectedAudience(payload.aud)) return null;

  const now = Math.floor(Date.now() / 1000);
  if (typeof payload.exp !== "number" || payload.exp <= now) return null;
  if (typeof payload.iat === "number" && payload.iat > now + 60) return null;

  try {
    const jwks = await fetchJwks();
    const key = jwks.keys.find((candidate) =>
      candidate.kid === header.kid && candidate.kty === "RSA" && (candidate.alg === undefined || candidate.alg === "RS256"),
    );
    if (!key) return null;
    const verifier = createVerify("RSA-SHA256");
    verifier.update(`${encodedHeader}.${encodedPayload}`);
    verifier.end();
    if (!verifier.verify(jwtJwkToPem(key), Buffer.from(encodedSignature, "base64url"))) return null;
    return payload.sub.trim();
  } catch {
    return null;
  }
}

function dashboardSubject(req: Request): string | null {
  const configuredSecret = process.env.NEXUS_CALENDAR_DASHBOARD_SECRET;
  const presentedSecret = req.headers.get("x-nexus-dashboard-secret");
  const subject = req.headers.get("x-nexus-subject")?.trim();
  if (!configuredSecret || !presentedSecret || !subject) return null;

  const expected = Buffer.from(configuredSecret);
  const actual = Buffer.from(presentedSecret);
  if (expected.length !== actual.length || !timingSafeEqual(expected, actual)) return null;
  return subject;
}

/** Resolves identity only from a verified Auth JWT or authenticated Dashboard hop. */
export async function resolveCaller(req: Request): Promise<{ subject: string } | null> {
  const identity = req.headers.get("x-nexus-identity");
  if (identity) {
    const subject = await subjectFromIdentity(identity);
    return subject ? { subject } : null;
  }
  const subject = dashboardSubject(req);
  return subject ? { subject } : null;
}
