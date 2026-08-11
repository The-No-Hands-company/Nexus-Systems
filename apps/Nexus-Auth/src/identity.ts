import { randomUUID } from "node:crypto";
import type { SafeUser } from "./users";

/**
 * How long an identity token is good for.
 *
 * Short on purpose. The token is minted per request by the proxy and carried
 * one hop to the app, so it never needs to outlive that hop — and a leaked one
 * is useless almost immediately. It is deliberately longer than the proxy's 60s
 * session-cache window so a cached token cannot expire before the cache entry
 * that holds it.
 */
export const IDENTITY_TOKEN_TTL_SECONDS = 120;

export type IdentityClaims = {
  iss: "nexus-auth";
  sub: string;
  aud: string;
  email: string;
  username: string;
  role: string;
  /** Distinguishes these from the service tokens issueServiceToken mints. */
  typ: "identity";
  iat: number;
  exp: number;
  jti: string;
};

/**
 * The claim set an app receives about the signed-in user.
 *
 * `aud` is the host the token was minted for, and apps must check it: without
 * that check a token minted for one app could be replayed against another.
 *
 * Takes a SafeUser rather than a User so secret material cannot reach the
 * claims even by accident — the password hash is not in the input type.
 */
export function buildIdentityClaims(
  user: SafeUser,
  audience: string,
  now = Math.floor(Date.now() / 1000),
): IdentityClaims {
  return {
    iss: "nexus-auth",
    sub: user.id,
    aud: audience,
    email: user.email,
    username: user.username,
    role: user.role,
    typ: "identity",
    iat: now,
    exp: now + IDENTITY_TOKEN_TTL_SECONDS,
    jti: randomUUID(),
  };
}
