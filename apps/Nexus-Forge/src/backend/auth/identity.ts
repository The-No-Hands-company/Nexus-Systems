/**
 * Ecosystem identity: turns the x-nexus-identity JWT the proxy forwards into
 * a Forge UserRecord, creating one on the fly for new TNHC users.
 *
 * The proxy only sets x-nexus-identity after validating the TNHC session, and
 * Forge only listens on 127.0.0.1, so the header cannot arrive from outside.
 * We still verify the JWT fully (signature + audience + expiry + typ) using the
 * shared @nexus/identity package resolved by Bun at runtime via workspace:*.
 * tsc uses the stub in src/types/nexus-identity.d.ts.
 */

import { identityFromRequest } from "../../../../../packages/nexus-identity/src/index";
import type { ForgeDB, UserRecord } from "../storage/db";

const NEXUS_AUTH_INTERNAL = (process.env.NEXUS_AUTH_INTERNAL_URL ?? "http://127.0.0.1:4310").replace(/\/+$/, "");
const FORGE_AUDIENCE = process.env.NEXUS_FORGE_AUDIENCE
  ?? (process.env.PUBLIC_URL?.replace(/^https?:\/\//, "") ?? "forge.tnhc.dev");

function jwksUrl(): string {
  return `${NEXUS_AUTH_INTERNAL}/api/v1/auth/oauth/jwks`;
}

/**
 * Resolve the caller's Forge UserRecord from the x-nexus-identity header.
 * Returns null if the header is absent, invalid, or Auth's JWKS is unreachable.
 * On first sight of a TNHC username, provisions a Forge user automatically.
 */
export async function forgeUserFromIdentity(
  db: ForgeDB,
  request: Request,
): Promise<UserRecord | null> {
  const claims = await identityFromRequest(request, {
    jwksUrl: jwksUrl(),
    audience: FORGE_AUDIENCE,
  });
  if (!claims) return null;

  const username = claims.username;
  if (!username) return null;

  const existing = db.getUserByName(username);
  if (existing) return existing;

  // First visit — provision a Forge user. Race-safe: addUser has UNIQUE constraint.
  try {
    return db.addUser(username);
  } catch {
    return db.getUserByName(username) ?? null;
  }
}
