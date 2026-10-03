import { createHash, randomBytes } from "node:crypto";
import type { ForgeDB, UserRecord } from "../storage/db";

const PREFIX = "nxf_";
const HOUR_MS = 60 * 60 * 1000;
export const DEFAULT_TOKEN_TTL_MS = 8 * HOUR_MS;
export const MAX_TOKEN_TTL_MS = 30 * 24 * HOUR_MS;

function hashToken(token: string): string {
  return createHash("sha256").update(token).digest("hex");
}

/**
 * Issue an access token. Every token expires; there is no "never" option.
 * Only the SHA-256 of the token is stored, so a copy of the database does
 * not hand out working credentials.
 */
export function issueToken(
  db: ForgeDB,
  userId: number,
  ttlMs = DEFAULT_TOKEN_TTL_MS,
  now = Date.now(),
): string {
  if (!Number.isFinite(ttlMs) || ttlMs <= 0 || ttlMs > MAX_TOKEN_TTL_MS) {
    throw new Error(`token lifetime must be between 1ms and ${MAX_TOKEN_TTL_MS}ms`);
  }
  const token = PREFIX + randomBytes(32).toString("base64url");
  db.addToken(userId, hashToken(token), now + ttlMs);
  return token;
}

export function userForToken(db: ForgeDB, token: string, now = Date.now()): UserRecord | null {
  if (!token.startsWith(PREFIX)) return null;
  return db.userForTokenHash(hashToken(token), now);
}

/**
 * The caller's identity from an Authorization header. Accepts `Bearer
 * <token>` (API clients) and HTTP Basic with the token as the password (git,
 * which only speaks Basic). The Basic username is ignored: the token alone
 * names the user.
 */
export function principalFromRequest(
  db: ForgeDB,
  request: Request,
  now = Date.now(),
): UserRecord | null {
  const header = request.headers.get("authorization");
  if (!header) return null;
  const space = header.indexOf(" ");
  if (space < 0) return null;
  const scheme = header.slice(0, space).toLowerCase();
  const value = header.slice(space + 1).trim();
  if (scheme === "bearer") return userForToken(db, value, now);
  if (scheme === "basic") {
    let decoded: string;
    try {
      decoded = Buffer.from(value, "base64").toString("utf8");
    } catch {
      return null;
    }
    const colon = decoded.indexOf(":");
    if (colon < 0) return null;
    return userForToken(db, decoded.slice(colon + 1), now);
  }
  return null;
}
