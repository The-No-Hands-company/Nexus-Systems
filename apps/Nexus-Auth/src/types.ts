import { existsSync, mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export type IdentityRole = "founder" | "admin" | "operator" | "user" | "viewer";

export type Permission =
  | "auth:admin"
  | "auth:read"
  | "users:create"
  | "users:read"
  | "users:update"
  | "users:delete"
  | "users:approve"
  | "tokens:issue"
  | "tokens:revoke"
  | "tokens:validate"
  | "apikeys:create"
  | "apikeys:revoke"
  | "apikeys:read"
  | "sessions:read"
  | "sessions:revoke"
  | "system:health"
  | "system:config";

/**
 * Account lifecycle. Replaces the old `disabled` boolean, which could not
 * express "approved but not yet claimed" — the state an invited user sits in
 * between the operator saying yes and the user setting a password.
 *
 *   pending   — access requested, awaiting operator decision
 *   approved  — operator said yes; claim code not yet redeemed
 *   active    — password set, can log in
 *   suspended — operator revoked access; recoverable
 *   rejected  — operator said no; terminal
 */
export type AccountStatus = "pending" | "approved" | "active" | "suspended" | "rejected";

export interface User {
  id: string;
  username: string;
  email: string;
  role: IdentityRole;
  passwordHash: string;
  totpSecret?: string;
  totpEnabled: boolean;
  status: AccountStatus;
  /** sha256 of the one-time claim code. Cleared once redeemed. */
  claimCodeHash?: string;
  /** Free-text reason supplied with an access request. Operator-facing only. */
  note?: string;
  approvedAt?: string;
  approvedBy?: string;
  createdAt: string;
  updatedAt: string;
  lastLoginAt?: string;
}

export interface ApiKey {
  id: string;
  userId: string;
  name: string;
  keyHash: string;
  keyPrefix: string;
  scopes: string[];
  expiresAt?: string;
  revoked: boolean;
  lastUsedAt?: string;
  createdAt: string;
}

export type ServiceTokenPayload = {
  iss: "nexus-auth";
  sub: string;
  aud: string;
  scopes: string[];
  role: string;
  iat: number;
  exp: number;
  jti: string;
};

export type ValidateTokenResult =
  | { valid: true; payload: ServiceTokenPayload }
  | { valid: false; reason: string };

export interface OAuthClient {
  clientId: string;
  clientSecret: string;
  name: string;
  redirectUris: string[];
  scopes: string[];
  grantTypes: ("authorization_code" | "client_credentials" | "refresh_token")[];
  confidential: boolean;
  disabled: boolean;
  createdAt: string;
}

export interface OAuthAuthorizationCode {
  code: string;
  clientId: string;
  userId: string;
  redirectUri: string;
  scopes: string[];
  codeChallenge?: string;
  codeChallengeMethod?: "S256" | "plain";
  expiresAt: number;
  used: boolean;
}

export interface OAuthToken {
  accessToken: string;
  refreshToken?: string;
  tokenType: "Bearer";
  expiresIn: number;
  scopes: string[];
  userId: string;
  clientId: string;
  issuedAt: number;
}

export interface Session {
  id: string;
  userId: string;
  token: string;
  ipAddress: string;
  userAgent: string;
  createdAt: string;
  expiresAt: string;
  revoked: boolean;
}

export interface LoginRequest {
  username: string;
  password: string;
  totpToken?: string;
}

export interface LoginResult {
  success: boolean;
  token?: string;
  user?: Omit<User, "passwordHash" | "totpSecret">;
  reason?: string;
}

export interface AuthorizeRequest {
  responseType: "code";
  clientId: string;
  redirectUri: string;
  scope: string;
  state: string;
  codeChallenge?: string;
  codeChallengeMethod?: "S256" | "plain";
}

export interface TokenRequest {
  grantType: "authorization_code" | "client_credentials" | "refresh_token";
  code?: string;
  redirectUri?: string;
  clientId: string;
  clientSecret?: string;
  codeVerifier?: string;
  refreshToken?: string;
  scope?: string;
}

export interface JwksKey {
  kty: string;
  kid: string;
  alg: string;
  use: string;
  n?: string;
  e?: string;
  crv?: string;
  x?: string;
  y?: string;
}

export { existsSync, mkdirSync, readFileSync, renameSync, rmSync, writeFileSync, dirname, join, resolve, fileURLToPath };
