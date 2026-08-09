/**
 * OpenID Connect provider — authorization code flow with PKCE.
 *
 * Nexus-Auth already published `oauth/jwks` and `oauth/userinfo`, which made it
 * look like an OIDC provider without being one: there was no discovery document
 * and no way to obtain a token, so a standards-compliant client could not
 * complete a single sign-in. Nexus-Hosting calls `client.discovery()` from
 * openid-client and got a 404.
 *
 * Implementing the real flow rather than special-casing Nexus-Auth inside each
 * app is what keeps those apps self-hostable: Hosting's whole premise is that
 * other people run nodes, and those operators point it at their own Authentik or
 * Keycloak. An app that speaks OIDC works with all of them, including this one.
 *
 * Deliberate constraints:
 *
 * - PKCE is mandatory, S256 only. `plain` is accepted by the spec but offers no
 *   protection against code interception, and there is no legacy client here to
 *   support, so it is rejected outright.
 * - redirect_uri must match a registered value exactly — no prefix or wildcard
 *   matching. An open redirect here hands an attacker an authorization code.
 * - Codes are single-use and expire in 60 seconds. Redemption deletes the code
 *   before any other work, so a replay cannot race the first exchange.
 * - The code is bound to the client_id and redirect_uri it was issued for, and
 *   both are re-checked at the token endpoint.
 */

import { createHash, randomBytes, randomUUID, timingSafeEqual } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { signJwt } from "./token";

export type OidcClient = {
  clientId: string;
  /** Absent for public clients, which authenticate with PKCE alone. */
  clientSecret?: string;
  redirectUris: string[];
  name?: string;
};

type AuthorizationCode = {
  code: string;
  clientId: string;
  redirectUri: string;
  userId: string;
  scope: string;
  nonce?: string;
  codeChallenge: string;
  expiresAt: number;
};

const CODE_TTL_MS = 60_000;
const ID_TOKEN_TTL_SECONDS = 3600;

/** In memory on purpose: codes live 60s, so surviving a restart is worthless. */
const codes = new Map<string, AuthorizationCode>();

const moduleDir = dirname(fileURLToPath(import.meta.url));

function clientsPath(): string {
  return (
    process.env.NEXUS_AUTH_OIDC_CLIENTS_FILE ||
    resolve(join(moduleDir, "..", "data", "oidc-clients.json"))
  );
}

export function issuer(): string {
  // Must match what clients use as ISSUER_URL and what appears as `iss` in the
  // ID token, or verification fails. Falls back to the local address so
  // development works without configuration.
  const base = process.env.NEXUS_AUTH_PUBLIC_URL || process.env.NEXUS_AUTH_ISSUER;
  if (base) return base.replace(/\/+$/, "");
  return `http://localhost:${process.env.PORT || "4310"}`;
}

export function loadClients(): OidcClient[] {
  const path = clientsPath();
  if (!existsSync(path)) return [];
  try {
    const parsed = JSON.parse(readFileSync(path, "utf8")) as { clients?: OidcClient[] };
    return Array.isArray(parsed.clients) ? parsed.clients : [];
  } catch {
    return [];
  }
}

export function saveClients(clients: OidcClient[]): void {
  const path = clientsPath();
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, JSON.stringify({ clients }, null, 2), { mode: 0o600 });
}

export function getClient(clientId: string): OidcClient | undefined {
  return loadClients().find((c) => c.clientId === clientId);
}

/**
 * Exact match, deliberately. Prefix matching on a redirect_uri is a standing
 * open-redirect: a registered "https://app.example/cb" would also accept
 * "https://app.example/cb.attacker.test".
 */
export function redirectUriAllowed(client: OidcClient, redirectUri: string): boolean {
  return client.redirectUris.includes(redirectUri);
}

function constantTimeEquals(a: string, b: string): boolean {
  const bufA = Buffer.from(a);
  const bufB = Buffer.from(b);
  if (bufA.length !== bufB.length) return false;
  return timingSafeEqual(bufA, bufB);
}

export function verifyClientSecret(client: OidcClient, presented: string | undefined): boolean {
  if (!client.clientSecret) return true; // public client — PKCE is the proof
  if (!presented) return false;
  return constantTimeEquals(client.clientSecret, presented);
}

export function createAuthorizationCode(input: {
  clientId: string;
  redirectUri: string;
  userId: string;
  scope: string;
  nonce?: string;
  codeChallenge: string;
}): string {
  const code = randomBytes(32).toString("base64url");
  codes.set(code, { ...input, code, expiresAt: Date.now() + CODE_TTL_MS });
  return code;
}

/**
 * Single-use redemption. The entry is removed before validation so a concurrent
 * replay finds nothing, rather than both callers passing the checks.
 */
export function redeemAuthorizationCode(input: {
  code: string;
  clientId: string;
  redirectUri: string;
  codeVerifier: string;
}): { ok: true; record: AuthorizationCode } | { ok: false; error: string } {
  const record = codes.get(input.code);
  codes.delete(input.code);

  if (!record) return { ok: false, error: "invalid_grant" };
  if (record.expiresAt < Date.now()) return { ok: false, error: "invalid_grant" };
  if (record.clientId !== input.clientId) return { ok: false, error: "invalid_grant" };
  // Re-checked here even though it was validated at /authorize: the token
  // request is a separate, unauthenticated call and must not be trusted to
  // repeat the same values.
  if (record.redirectUri !== input.redirectUri) return { ok: false, error: "invalid_grant" };

  const challenge = createHash("sha256").update(input.codeVerifier).digest("base64url");
  if (!constantTimeEquals(record.codeChallenge, challenge)) {
    return { ok: false, error: "invalid_grant" };
  }

  return { ok: true, record };
}

export function issueIdToken(input: {
  userId: string;
  clientId: string;
  nonce?: string;
  extraClaims?: Record<string, unknown>;
}): string {
  const now = Math.floor(Date.now() / 1000);
  return signJwt({
    iss: issuer(),
    sub: input.userId,
    aud: input.clientId,
    iat: now,
    exp: now + ID_TOKEN_TTL_SECONDS,
    jti: randomUUID(),
    ...(input.nonce ? { nonce: input.nonce } : {}),
    ...(input.extraClaims ?? {}),
  });
}

export function discoveryDocument(): Record<string, unknown> {
  const base = issuer();
  return {
    issuer: base,
    authorization_endpoint: `${base}/api/v1/auth/oauth/authorize`,
    token_endpoint: `${base}/api/v1/auth/oauth/token`,
    userinfo_endpoint: `${base}/api/v1/auth/oauth/userinfo`,
    jwks_uri: `${base}/api/v1/auth/oauth/jwks`,
    end_session_endpoint: `${base}/api/v1/auth/logout`,
    response_types_supported: ["code"],
    grant_types_supported: ["authorization_code"],
    subject_types_supported: ["public"],
    id_token_signing_alg_values_supported: ["RS256"],
    scopes_supported: ["openid", "profile", "email"],
    token_endpoint_auth_methods_supported: ["client_secret_post", "client_secret_basic", "none"],
    // Advertised without "plain" because plain is rejected.
    code_challenge_methods_supported: ["S256"],
    claims_supported: ["sub", "iss", "aud", "exp", "iat", "nonce", "preferred_username", "email", "role"],
  };
}
