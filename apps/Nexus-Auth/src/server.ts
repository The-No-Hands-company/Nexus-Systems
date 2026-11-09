import { randomBytes } from "node:crypto";
import { buildSystemsApiRegistrationPayload } from "./contracts";
import { startNexusAuthCloudRegistrationHeartbeat } from "./cloud";
import {
  createUser,
  authenticateUser,
  findUserByUsername,
  getUser,
  getRawUser,
  listUsers,
  updateUser,
  changePassword,
  deleteUser,
  userHasPermission,
  getUserPermissions,
  seedDefaultUsers,
  sanitizeUser,
  findUserByEmail,
  createAccessRequest,
  claimAccount,
  listByStatus,
  setUserStatus,
  MIN_PASSWORD_LENGTH,
  resetPassword,
  setTotpSecret,
  enableTotp,
  disableTotp,
  updateProfile,
} from "./users";
import { consumeRecoveryCode, countRemainingRecoveryCodes, issueRecoveryCodes } from "./recovery";
import { createInvite, redeemInvite } from "./invites";
import { isPlausibleEmail, sendSystemMail, provisionMailbox } from "./system-mail";
import { checkRateLimit, recordFailure, clearFailures } from "./ratelimit";
import { generateTotpSecret, generateTotpCode, verifyTotpCode, buildTotpUri } from "./totp";
import { renderAccountPage } from "./account-page";
import {
  ONBOARDING_HEADERS,
  ONBOARDING_LIMITS,
  escapeHtml,
  renderClaimPage,
  renderJoinPage,
  renderRecoveredPage,
  renderRecoverPage,
  renderRequestedPage,
  renderResetPasswordPage,
  renderRequestPage,
  renderWelcomePage,
} from "./onboarding-page";
import { audit, recentActivity } from "./audit";
import {
  createApiKey,
  validateApiKey,
  getApiKey,
  listApiKeys,
  revokeApiKey,
} from "./api-keys";
import {
  createSession,
  validateSession,
  getSession,
  listSessions,
  revokeSession,
  revokeAllUserSessions,
} from "./sessions";
import { publicJwk } from "./keys";
import { renderLoginPage, safeRedirect } from "./login-page";
import {
  createAuthorizationCode,
  discoveryDocument,
  getClient,
  issuer,
  issueIdToken,
  redeemAuthorizationCode,
  redirectUriAllowed,
  verifyClientSecret,
} from "./oidc";
import { issueServiceToken, validateServiceToken, signJwt } from "./token";
import { buildAuthContract } from "./auth-contract";
import { buildIdentityClaims, IDENTITY_TOKEN_TTL_SECONDS } from "./identity";
import { getCatalogPreferences, saveCatalogPreferences } from "./preferences";
import type { AccountStatus, LoginResult, Permission } from "./types";
import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";
import { verifyDidSignature } from "../../../packages/phantom-utils/src/verify";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";

const phantomApp = new PhantomApp("nexus-auth");
const CLOUD_URL = process.env.NEXUS_CLOUD_URL ?? "http://localhost:8787";
phantomApp.start()
  .then(() => phantomApp.registerWithCloud(CLOUD_URL))
  .catch((err: unknown) => {
    console.error("[nexus-auth] Phantom startup failed:", err instanceof Error ? err.message : err);
  });

function jsonResponse(payload: unknown, init?: ResponseInit): Response {
  // Merge, do not replace. This previously spread `init` and then overwrote
  // `headers` wholesale, so any caller passing headers had them silently
  // dropped — which made Set-Cookie impossible.
  const headers = new Headers(init?.headers);
  headers.set("content-type", "application/json; charset=utf-8");
  return new Response(JSON.stringify(payload, null, 2), { ...init, headers });
}

/**
 * Single sign-on across the ecosystem.
 *
 * Every Nexus app lives on a subdomain of one parent domain, so a session
 * cookie scoped to that parent is sent by the browser to all of them: sign in
 * once at the apex and deploy.tnhc.dev, chat.tnhc.dev and the rest see the same
 * session with no second login. That is the entire reason the subdomain layout
 * matters — separate domains could not share a cookie at all.
 *
 * NEXUS_AUTH_COOKIE_DOMAIN carries the leading dot (".tnhc.dev"). Left unset,
 * no Domain attribute is emitted and the cookie stays host-only, which is the
 * correct default for a plain localhost dev box.
 */
const SESSION_COOKIE = "nexus_session";

/**
 * Where to send someone who signs in without saying where they were going.
 *
 * Someone visiting the sign-in page directly has no return address, and the
 * previous answer was `/login` — which set their cookie and then rendered the
 * empty form again. They *were* signed in; it just looked exactly like a
 * silent failure, with no error and a blank password box.
 *
 * The dashboard is the right answer: it is the front door and it lists
 * everything the account can now reach.
 */
/**
 * The browser-facing origin of this service, for links that leave the node in
 * an email. NEXUS_AUTH_PUBLIC_URL in production; otherwise derived from the
 * cookie domain; otherwise unknown, and no email is sent.
 */
function authPublicOrigin(): string | null {
  const configured = process.env.NEXUS_AUTH_PUBLIC_URL?.trim();
  if (configured) return configured.replace(/\/+$/, "");
  const domain = cookieDomain()?.replace(/^\./, "");
  return domain ? `https://auth.${domain}` : null;
}

/** At most this many emailed invites per operator per day. Bounds what a
 *  stolen operator session could send in the platform's name. */
const EMAILED_INVITES_PER_DAY = 20;

function defaultPostLogin(): string {
  const configured = process.env.NEXUS_POST_LOGIN_URL?.trim();
  if (configured) return configured;
  const domain = (cookieDomain()?.replace(/^\./, "") || process.env.DOMAIN || "tnhc.dev").trim();
  return `https://app.${domain}/`;
}

/** Exposed for tests: the destination a return-address-less sign-in uses. */
export function defaultPostLoginForTest(): string {
  return defaultPostLogin();
}

function cookieDomain(): string | null {
  const raw = process.env.NEXUS_AUTH_COOKIE_DOMAIN?.trim();
  return raw ? raw : null;
}

/**
 * Whether to mark the session cookie Secure.
 *
 * Defaults to on whenever a cookie domain is configured, since that only happens
 * for a real deployment behind TLS. Separately overridable because the two are
 * not actually the same question: testing cross-subdomain sign-on locally needs
 * a cookie domain over plain http, and a browser silently refuses to store a
 * Secure cookie on an insecure origin — the login would appear to succeed and
 * the user would arrive still signed out.
 *
 * Only set NEXUS_AUTH_COOKIE_SECURE=false for local testing. Over the public
 * internet it lets the session travel in clear text.
 */
function cookieSecure(): boolean {
  const raw = process.env.NEXUS_AUTH_COOKIE_SECURE?.trim().toLowerCase();
  if (raw === "false") return false;
  if (raw === "true") return true;
  return cookieDomain() !== null;
}

function readCookie(request: Request, name: string): string | null {
  const header = request.headers.get("cookie");
  if (!header) return null;
  for (const part of header.split(";")) {
    const eq = part.indexOf("=");
    if (eq === -1) continue;
    if (part.slice(0, eq).trim() === name) {
      return decodeURIComponent(part.slice(eq + 1).trim());
    }
  }
  return null;
}

function sessionCookie(token: string, maxAgeSeconds: number): string {
  const domain = cookieDomain();
  const parts = [
    `${SESSION_COOKIE}=${encodeURIComponent(token)}`,
    "Path=/",
    "HttpOnly",
    // Lax, not Strict: a redirect back from the apex login page is a top-level
    // navigation, and Strict would withhold the cookie on that first hop.
    "SameSite=Lax",
    `Max-Age=${maxAgeSeconds}`,
  ];
  if (domain) parts.push(`Domain=${domain}`);
  if (cookieSecure()) parts.push("Secure");
  return parts.join("; ");
}

function clearedSessionCookie(): string {
  const domain = cookieDomain();
  // Must mirror the attributes the cookie was set with, or the browser treats
  // it as a different cookie and the old one survives the logout.
  const parts = [`${SESSION_COOKIE}=`, "Path=/", "HttpOnly", "SameSite=Lax", "Max-Age=0"];
  if (domain) parts.push(`Domain=${domain}`);
  if (cookieSecure()) parts.push("Secure");
  return parts.join("; ");
}

async function parseBody(request: Request): Promise<Record<string, unknown>> {
  try {
    const parsed = await request.json();
    return typeof parsed === "object" && parsed !== null ? (parsed as Record<string, unknown>) : {};
  } catch {
    return {};
  }
}

function extractToken(request: Request): string | null {
  const auth = request.headers.get("authorization") || "";
  const bearer = auth.match(/^Bearer\s+(.+)$/i);
  if (bearer) return bearer[1];

  // Browsers cannot attach an Authorization header to an ordinary navigation,
  // so the shared session cookie is what makes cross-subdomain SSO work at all.
  // Checked after Bearer so an explicit header still wins for API clients.
  const cookie = readCookie(request, SESSION_COOKIE);
  if (cookie) return cookie;

  const url = new URL(request.url);
  return url.searchParams.get("token");
}

function requireAuth(request: Request): { userId: string; session: ReturnType<typeof validateSession> } | null {
  const token = extractToken(request);
  if (!token) return null;

  const session = validateSession(token);
  if (!session) {
    const apiKey = validateApiKey(token);
    if (apiKey) {
      return { userId: apiKey.userId, session: undefined };
    }
    return null;
  }

  return { userId: session.userId, session };
}

function requirePermission(userId: string, permission: Permission): boolean {
  return userHasPermission(userId, permission);
}

// Attach a failed attempt to the account it targeted so the owner can see it.
// No deviceId (the attacker has no session) and nothing about the caller.
// Unknown username: nothing to attach it to, so no row.
function auditLoginFailure(username: string): void {
  const target = username ? findUserByUsername(username) : undefined;
  if (target) audit({ event: "login_failure", userId: target.id });
}

// The front door strips every address header and forwards an opaque,
// non-reversible tag instead. Auth never reads or stores a network address.
function clientTag(request: Request): string {
  return request.headers.get("x-nexus-client-tag") || "unknown";
}

// ── Onboarding: the three ways in for someone without an account ──────────
// Served here because this is the one host the proxy's gate keeps public; on
// app.tnhc.dev they sat behind the login they exist to get past. See
// onboarding-page.ts.

const ONBOARDING_PATHS = new Set(["/request", "/claim", "/join", "/recover", "/recover/password"]);

/**
 * Between a verified recovery code and a new password. Random, single-use,
 * 15 minutes, in memory only: a restart simply means using another code.
 */
const RESET_GRANT_TTL_MS = 15 * 60_000;
const resetGrants = new Map<string, { userId: string; expiresAt: number }>();

function issueResetGrant(userId: string): string {
  const now = Date.now();
  for (const [key, grant] of resetGrants) if (grant.expiresAt <= now) resetGrants.delete(key);
  const grant = randomBytes(32).toString("hex");
  resetGrants.set(grant, { userId, expiresAt: now + RESET_GRANT_TTL_MS });
  return grant;
}

function findResetGrant(grant: string): { userId: string } | null {
  const found = resetGrants.get(grant);
  if (!found || found.expiresAt <= Date.now()) return null;
  return found;
}

function renderOnboardingResponse(body: string, status: number, extra: Record<string, string> = {}): Response {
  return new Response(body, { status, headers: { ...ONBOARDING_HEADERS, ...extra } });
}

/** One form field, trimmed (unless it is a password) and capped at its limit. */
function parseFormField(form: FormData | null, name: keyof typeof ONBOARDING_LIMITS | "confirm", trim = true): string {
  const raw = String(form?.get(name) ?? "");
  const limit = name === "confirm" ? ONBOARDING_LIMITS.password : ONBOARDING_LIMITS[name];
  const value = raw.slice(0, limit);
  return trim ? value.trim() : value;
}

function describeRetry(retryAfterMs: number): string {
  return `Too many attempts. Try again in ${Math.max(1, Math.ceil(retryAfterMs / 60_000))} minutes.`;
}

/** Sign the new account in, exactly as /login does, and show its recovery codes once. */
function renderSignedInWelcome(user: { id: string; username: string }, recoveryCodes: string[], status: number): Response {
  const session = createSession({ userId: user.id });
  const maxAge = Math.max(60, Math.floor((new Date(session.expiresAt).getTime() - Date.now()) / 1000));
  return renderOnboardingResponse(
    renderWelcomePage({ username: user.username, recoveryCodes, continueTo: defaultPostLogin() }),
    status,
    { "set-cookie": sessionCookie(session.token, maxAge) },
  );
}

const WEAK_PASSWORD_MESSAGE = `Passwords must be at least ${MIN_PASSWORD_LENGTH} characters.`;  // pragma: allowlist secret

const CLAIM_ERRORS: Record<string, string> = {
  invalid_code: "That email and claim code do not match an account waiting to be claimed.",
  not_approved: "Your request has not been approved yet. Try again once an operator has approved it.",
  weak_password: WEAK_PASSWORD_MESSAGE,
};

const JOIN_ERRORS: Record<string, string> = {
  invalid_code: "That invite code is not valid or has already been used.",
  expired: "That invite has expired. Ask whoever invited you for a new one.",
  weak_password: WEAK_PASSWORD_MESSAGE,
  username_taken: "That username is taken. Pick another.",
  email_taken: "That email already has an account. Sign in instead.",
};

async function handleOnboarding(request: Request, path: string, signedIn: boolean): Promise<Response> {
  const url = new URL(request.url);
  // Someone with an account has nothing to do here.
  if (request.method === "GET" && signedIn) {
    return new Response(null, { status: 303, headers: { location: defaultPostLogin(), "cache-control": "no-store" } });
  }
  if (request.method === "GET") {
    if (path === "/request") return renderOnboardingResponse(renderRequestPage({}), 200);
    if (path === "/recover") return renderOnboardingResponse(renderRecoverPage({}), 200);
    // The reset step only follows a verified code; on its own it means nothing.
    if (path === "/recover/password") {
      return new Response(null, { status: 303, headers: { location: "/recover", "cache-control": "no-store" } });
    }
    if (path === "/claim") {
      const email = (url.searchParams.get("email") ?? "").slice(0, ONBOARDING_LIMITS.email);
      return renderOnboardingResponse(renderClaimPage({ email, minPasswordLength: MIN_PASSWORD_LENGTH }), 200);
    }
    const code = (url.searchParams.get("code") ?? "").slice(0, ONBOARDING_LIMITS.code);
    return renderOnboardingResponse(renderJoinPage({ code, minPasswordLength: MIN_PASSWORD_LENGTH }), 200);
  }
  if (request.method !== "POST") {
    return renderOnboardingResponse("Method not allowed", 405, { allow: "GET, POST", "content-type": "text/plain; charset=utf-8" });
  }

  const form = await request.formData().catch(() => null);
  const tag = clientTag(request);

  if (path === "/recover") {
    const email = parseFormField(form, "email");
    const code = parseFormField(form, "code");
    const limit = checkRateLimit("recover", tag);
    if (!limit.allowed) {
      return renderOnboardingResponse(renderRecoverPage({ email, error: describeRetry(limit.retryAfterMs) }), 429,
        { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) });
    }
    const user = email ? findUserByEmail(email) : undefined;
    // One answer for an unknown email, an inactive account and a wrong code,
    // so the page cannot be used to test which emails have accounts.
    if (!user || user.status !== "active" || !code || !consumeRecoveryCode(user.id, code)) {
      recordFailure("recover", tag);
      return renderOnboardingResponse(renderRecoverPage({ email, error: "That email and recovery code do not match an account." }), 400);
    }
    clearFailures("recover", tag);
    audit({ event: "recovery_code_used", userId: user.id });
    return renderOnboardingResponse(
      renderResetPasswordPage({ grant: issueResetGrant(user.id), minPasswordLength: MIN_PASSWORD_LENGTH }), 200);
  }

  if (path === "/recover/password") {
    const grant = String(form?.get("grant") ?? "").slice(0, 128);
    const password = parseFormField(form, "password", false);
    const confirm = parseFormField(form, "confirm", false);
    const found = findResetGrant(grant);
    if (!found) {
      return renderOnboardingResponse(
        renderRecoverPage({ error: "That step has expired or was already used. Start again with another recovery code." }), 400);
    }
    const renderResetError = (error: string) => renderOnboardingResponse(
      renderResetPasswordPage({ grant, minPasswordLength: MIN_PASSWORD_LENGTH, error }), 400);
    if (password.length < MIN_PASSWORD_LENGTH) return renderResetError(WEAK_PASSWORD_MESSAGE);
    if (password !== confirm) return renderResetError("The two passwords do not match.");
    if (!resetPassword(found.userId, password)) return renderResetError("The password could not be changed. Try again.");

    resetGrants.delete(grant);
    // Whoever knew the old password may hold a session; recovery ends them all.
    revokeAllUserSessions(found.userId);
    audit({ event: "password_change", userId: found.userId, detail: { via: "recovery_code" } });
    const session = createSession({ userId: found.userId });
    const maxAge = Math.max(60, Math.floor((new Date(session.expiresAt).getTime() - Date.now()) / 1000));
    return renderOnboardingResponse(
      renderRecoveredPage({ remaining: countRemainingRecoveryCodes(found.userId), continueTo: defaultPostLogin() }),
      200,
      { "set-cookie": sessionCookie(session.token, maxAge) },
    );
  }

  if (path === "/request") {
    // Every request counts here, not only failures: there is no secret to
    // guess, but an open form that creates records is a spam target.
    const limit = checkRateLimit("access-request", tag);
    const username = parseFormField(form, "username");
    const email = parseFormField(form, "email");
    const note = parseFormField(form, "note");
    if (!limit.allowed) {
      return renderOnboardingResponse(renderRequestPage({ username, email, note, error: describeRetry(limit.retryAfterMs) }), 429,
        { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) });
    }
    if (!username || !email) {
      return renderOnboardingResponse(renderRequestPage({ username, email, note, error: "Username and email are both required." }), 400);
    }
    recordFailure("access-request", tag);
    try {
      const result = createAccessRequest({ username, email, ...(note ? { note } : {}) });
      return renderOnboardingResponse(renderRequestedPage({ email: result.user.email, claimCode: result.claimCode }), 201);
    } catch {
      // One message for both clashes: naming which one would let anyone test
      // whether an email address has an account here.
      return renderOnboardingResponse(renderRequestPage({ username, email, note, error: "That username or email is already in use." }), 409);
    }
  }

  const password = parseFormField(form, "password", false);
  const confirm = parseFormField(form, "confirm", false);

  if (path === "/claim") {
    const email = parseFormField(form, "email");
    const claimCode = String(form?.get("claimCode") ?? "").trim().slice(0, ONBOARDING_LIMITS.code);
    const renderClaimError = (error: string, status: number, extra: Record<string, string> = {}) =>
      renderOnboardingResponse(renderClaimPage({ email, minPasswordLength: MIN_PASSWORD_LENGTH, error }), status, extra);

    const limit = checkRateLimit("claim", tag);
    if (!limit.allowed) {
      return renderClaimError(describeRetry(limit.retryAfterMs), 429, { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) });
    }
    if (password.length < MIN_PASSWORD_LENGTH) return renderClaimError(WEAK_PASSWORD_MESSAGE, 400);
    if (password !== confirm) return renderClaimError("The two passwords do not match.", 400);

    const result = claimAccount({ email, claimCode, password });
    if (!result.ok) {
      if (result.reason !== "weak_password") recordFailure("claim", tag);
      return renderClaimError(CLAIM_ERRORS[result.reason] ?? "That claim could not be completed.", 400);
    }
    clearFailures("claim", tag);
    void provisionMailbox({ subject: result.user.id, username: result.user.username, email: result.user.email });
    return renderSignedInWelcome(result.user, result.recoveryCodes, 200);
  }

  // /join
  const code = parseFormField(form, "code");
  const username = parseFormField(form, "username");
  const email = parseFormField(form, "email");
  const renderJoinError = (error: string, status: number, extra: Record<string, string> = {}) =>
    renderOnboardingResponse(renderJoinPage({ code, username, email, minPasswordLength: MIN_PASSWORD_LENGTH, error }), status, extra);

  const limit = checkRateLimit("invite", tag);
  if (!limit.allowed) {
    return renderJoinError(describeRetry(limit.retryAfterMs), 429, { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) });
  }
  if (!username || !email) return renderJoinError("Username and email are both required.", 400);
  if (password.length < MIN_PASSWORD_LENGTH) return renderJoinError(WEAK_PASSWORD_MESSAGE, 400);
  if (password !== confirm) return renderJoinError("The two passwords do not match.", 400);

  const result = redeemInvite({ code, username, email, password });
  if (!result.ok) {
    // Only a bad code is a guess worth counting; a taken name is not.
    if (result.reason === "invalid_code" || result.reason === "expired") recordFailure("invite", tag);
    return renderJoinError(JOIN_ERRORS[result.reason] ?? "That invite could not be redeemed.", 400);
  }
  clearFailures("invite", tag);
  void provisionMailbox({ subject: result.user.id, username: result.user.username, email: result.user.email });
  return renderSignedInWelcome(result.user, result.recoveryCodes, 201);
}

// Read once at module scope so handleRequest can be a plain top-level
// function rather than a closure over createAuthServer's locals.
const AUTH_PORT = Number(process.env.PORT || "4310");
const AUTH_BASE_URL = process.env.NEXUS_AUTH_BASE_URL || `http://localhost:${AUTH_PORT}`;
const CLOUD_INTEGRATION_ENABLED =
  (process.env.NEXUS_AUTH_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";

/**
 * The whole HTTP surface. Lifted out of Bun.serve's `fetch` so tests can
 * drive routes directly with a Request instead of binding a port.
 */
export async function handleRequest(request: Request): Promise<Response> {
    const url = new URL(request.url);
    const path = url.pathname;
    const auth = requireAuth(request);

    if (request.method === "GET" && (path === "/" || path === "")) {
      return new Response(null, {
        status: 303,
        headers: { location: defaultPostLogin(), "cache-control": "no-store" },
      });
    }

    if (request.method === "GET" && path === "/health") {
      return jsonResponse({ status: "ok", service: "nexus-auth", timestamp: new Date().toISOString(), phantom: phantomApp.status() });
    }

    if (request.method === "GET" && path === "/api/v1/auth/contract") {
      return jsonResponse(buildAuthContract());
    }

    if (request.method === "GET" && path === "/api/v1/auth/readiness") {
      return jsonResponse({
        status: "ready",
        contracts: {
          contract: "/api/v1/auth/contract",
          login: "/api/v1/auth/login",
          logout: "/api/v1/auth/logout",
          users: "/api/v1/auth/users",
          me: "/api/v1/auth/me",
          catalogPreferences: "/api/v1/auth/preferences/catalog",
          tokens: "/api/v1/auth/tokens",
          apiKeys: "/api/v1/auth/api-keys",
          sessions: "/api/v1/auth/sessions",
          oauthAuthorize: "/api/v1/auth/oauth/authorize",
          oauthToken: "/api/v1/auth/oauth/token",
          oauthJwks: "/api/v1/auth/oauth/jwks",
          oauthUserinfo: "/api/v1/auth/oauth/userinfo",
        },
        userCount: listUsers().length,
        cloudIntegration: { enabled: CLOUD_INTEGRATION_ENABLED },
      });
    }

    if (request.method === "GET" && path === "/api/v1/auth/contracts/registration") {
      return jsonResponse({ status: "ok", payload: buildSystemsApiRegistrationPayload(AUTH_BASE_URL) });
    }

    // ── OpenID Connect discovery ──
    // Public and unauthenticated by design: this is the document every OIDC
    // client fetches first, before it has any credential. Nexus-Hosting's
    // openid-client calls discovery() against ISSUER_URL and got a 404 here,
    // which is what made ecosystem sign-in impossible for it.
    if (request.method === "GET" && path === "/.well-known/openid-configuration") {
      return jsonResponse(discoveryDocument(), { headers: { "cache-control": "public, max-age=300" } });
    }

    // ── OAuth authorize ──
    if (request.method === "GET" && path === "/api/v1/auth/oauth/authorize") {
      const clientId = url.searchParams.get("client_id") || "";
      const redirectUri = url.searchParams.get("redirect_uri") || "";
      const responseType = url.searchParams.get("response_type") || "";
      const scope = url.searchParams.get("scope") || "openid";
      const state = url.searchParams.get("state");
      const nonce = url.searchParams.get("nonce") || undefined;
      const codeChallenge = url.searchParams.get("code_challenge") || "";
      const codeChallengeMethod = url.searchParams.get("code_challenge_method") || "";

      const client = getClient(clientId);
      // An unregistered client or an unregistered redirect_uri must NOT be
      // redirected to — that is precisely the open redirect this endpoint
      // would otherwise become. Errors are shown here instead.
      if (!client) {
        return jsonResponse({ error: "invalid_client" }, { status: 400 });
      }
      if (!redirectUri || !redirectUriAllowed(client, redirectUri)) {
        return jsonResponse({ error: "invalid_request", error_description: "redirect_uri is not registered for this client" }, { status: 400 });
      }

      // Past this point the redirect_uri is trusted, so protocol errors go
      // back to the client as the spec requires.
      const bounce = (error: string, description?: string) => {
        const target = new URL(redirectUri);
        target.searchParams.set("error", error);
        if (description) target.searchParams.set("error_description", description);
        if (state) target.searchParams.set("state", state);
        return new Response(null, { status: 303, headers: { location: target.toString() } });
      };

      if (responseType !== "code") return bounce("unsupported_response_type");
      if (!codeChallenge) return bounce("invalid_request", "PKCE is required");
      if (codeChallengeMethod !== "S256") return bounce("invalid_request", "code_challenge_method must be S256");

      // Not signed in: reuse the ordinary login page and come straight back
      // here afterwards, so there is still exactly one place a password is
      // typed. The Request URL names the loopback upstream when this arrives
      // through the production proxy; rebuild both browser-facing URLs from
      // the configured issuer so neither sends the browser to 127.0.0.1.
      if (!auth) {
        const login = new URL("/login", issuer());
        const returnToAuthorize = new URL(`${url.pathname}${url.search}`, issuer());
        login.searchParams.set("redirect", returnToAuthorize.toString());
        return new Response(null, { status: 303, headers: { location: login.toString() } });
      }

      const code = createAuthorizationCode({
        clientId,
        redirectUri,
        userId: auth.userId,
        scope,
        ...(nonce ? { nonce } : {}),
        codeChallenge,
      });

      const target = new URL(redirectUri);
      target.searchParams.set("code", code);
      if (state) target.searchParams.set("state", state);
      return new Response(null, { status: 303, headers: { location: target.toString() } });
    }

    // ── OAuth token ──
    if (request.method === "POST" && path === "/api/v1/auth/oauth/token") {
      const form = await request.formData().catch(() => null);
      const get = (k: string) => String(form?.get(k) ?? "");

      // client_secret_basic as well as _post: openid-client defaults to Basic
      // for confidential clients, and rejecting it would look like a bad secret.
      let clientId = get("client_id");
      let clientSecret = get("client_secret") || undefined;  // pragma: allowlist secret
      const authz = request.headers.get("authorization") || "";
      const basic = authz.match(/^Basic\s+(.+)$/i);
      if (basic) {
        const [u, p] = Buffer.from(basic[1], "base64").toString("utf8").split(":");
        if (u) clientId = decodeURIComponent(u);
        if (p) clientSecret = decodeURIComponent(p);
      }

      if (get("grant_type") !== "authorization_code") {
        return jsonResponse({ error: "unsupported_grant_type" }, { status: 400 });
      }

      const client = getClient(clientId);
      if (!client || !verifyClientSecret(client, clientSecret)) {
        return jsonResponse({ error: "invalid_client" }, { status: 401 });
      }

      const redeemed = redeemAuthorizationCode({
        code: get("code"),
        clientId,
        redirectUri: get("redirect_uri"),
        codeVerifier: get("code_verifier"),
      });
      if (!redeemed.ok) {
        return jsonResponse({ error: redeemed.error }, { status: 400 });
      }

      const user = getUser(redeemed.record.userId);
      if (!user) return jsonResponse({ error: "invalid_grant" }, { status: 400 });

      // The access token is a real session rather than a second token type, so
      // /userinfo and every existing check validate it unchanged — and it can
      // be revoked through the same machinery as any other session.
      const session = createSession({
        userId: user.id,
      });

      const idToken = issueIdToken({
        userId: user.id,
        clientId,
        ...(redeemed.record.nonce ? { nonce: redeemed.record.nonce } : {}),
        extraClaims: {
          preferred_username: user.username,
          email: user.email,
          role: user.role,
        },
      });

      return jsonResponse(
        {
          access_token: session.token,
          token_type: "Bearer",
          expires_in: 86400,
          id_token: idToken,
          scope: redeemed.record.scope,
        },
        { headers: { "cache-control": "no-store", pragma: "no-cache" } },
      );
    }

    if (ONBOARDING_PATHS.has(path)) return await handleOnboarding(request, path, Boolean(auth));

    // ── Apex sign-in page ──
    // The one place a human types a password. Apps that get an unauthenticated
    // browser navigation send it here with ?redirect=<where they were going>.
    if (request.method === "GET" && path === "/login") {
      // Accept both spellings. Every caller in the ecosystem sends
      // `redirect_uri` — the proxy's login gate and the dashboard's sign-in
      // button both do — while this page only ever read `redirect`, so the
      // return address was silently dropped and everyone landed back on
      // /login after signing in instead of where they were going. Reading
      // both fixes every existing link without touching any of them.
      const target = safeRedirect(
        url.searchParams.get("redirect_uri") ?? url.searchParams.get("redirect"),
        cookieDomain(),
      );
      // Already signed in: honour the round trip immediately rather than
      // asking for a password that is not needed. With no return address the
      // destination is the dashboard — showing a signed-in person a login form
      // is how this looked broken.
      if (auth) {
        return new Response(null, {
          status: 303,
          headers: { location: target ?? defaultPostLogin() },
        });
      }
      return new Response(renderLoginPage({ redirect: target }), {
        status: 200,
        headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store", "content-security-policy": "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'self'" },
      });
    }

    if (request.method === "POST" && path === "/login") {
      const form = await request.formData().catch(() => null);
      const username = String(form?.get("username") ?? "").trim();
      const password = String(form?.get("password") ?? "");  // pragma: allowlist secret
      const target = safeRedirect(String(form?.get("redirect") ?? "") || null, cookieDomain());

      // Brute-force gate. The claim/recover/invite endpoints already had this;
      // login was the one credential check that didn't, which made it the
      // cheapest path to a password. Keyed on IP + username so an attacker
      // can't lock out a victim by spraying from their own IP alone.
      const tag = clientTag(request);
      const loginLimit = checkRateLimit("login", `${tag}:${username}`);
      if (!loginLimit.allowed) {
        return new Response(
          renderLoginPage({ redirect: target, error: "Too many attempts. Try again later." }),
          { status: 429, headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store", "retry-after": String(Math.ceil(loginLimit.retryAfterMs / 1000)), "content-security-policy": "default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'self'" } },
        );
      }

      const user = username && password ? authenticateUser(username, password) : null;
      if (!user) {
        recordFailure("login", `${tag}:${username}`);
        auditLoginFailure(username);
        return new Response(
          renderLoginPage({ redirect: target, error: "Incorrect username or password." }),
          { status: 401, headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" } },
        );
      }

      if (user.totpEnabled) {
        const totpCode = String(form?.get("totpCode") ?? "").trim();
        if (!totpCode) {
          const twoFactorHtml = `<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8" />
<meta name="viewport" content="width=device-width, initial-scale=1" />
<title>Two-Factor Authentication — Nexus Systems</title>
<style>
  :root { --bg: #0b0d12; --surface: #141821; --fg: #e8eaf0; --muted: #8b93a7; --border: #2b3145; --input-bg: #0e1119; --accent: #4c7dff; }
  body { background: var(--bg); color: var(--fg); font-family: -apple-system, sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; margin: 0; }
  .card { background: var(--surface); border: 1px solid var(--border); border-radius: 8px; padding: 2rem; width: 100%; max-width: 400px; }
  h1 { font-size: 1.3rem; margin-bottom: 0.5rem; }
  p { font-size: 0.9rem; color: var(--muted); margin-bottom: 1.5rem; }
  input { width: 100%; padding: 0.75rem; background: var(--input-bg); border: 1px solid var(--border); border-radius: 6px; color: var(--fg); font-size: 1.2rem; text-align: center; letter-spacing: 4px; box-sizing: border-box; }
  button { width: 100%; margin-top: 1rem; padding: 0.75rem; background: var(--accent); color: white; border: none; border-radius: 6px; font-weight: 600; cursor: pointer; }
</style>
</head>
<body>
  <div class="card">
    <h1>Two-Factor Authentication</h1>
    <p>Enter the 6-digit code from your authenticator app.</p>
    <form method="post" action="/login">
      <input type="hidden" name="username" value="${escapeHtml(username)}" />
      <input type="hidden" name="password" value="${escapeHtml(password)}" />
      <input type="hidden" name="redirect" value="${escapeHtml(target ?? "")}" />
      <input type="text" name="totpCode" pattern="\\d{6}" maxlength="6" autofocus required placeholder="000000" autocomplete="one-time-code" />
      <button type="submit">Verify &amp; Sign In</button>
    </form>
  </div>
</body>
</html>`;
          return new Response(twoFactorHtml, {
            status: 200,
            headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" },
          });
        }
        const rawUser = getRawUser(user.id);
        if (!rawUser?.totpSecret || !verifyTotpCode(rawUser.totpSecret, totpCode)) {
          recordFailure("login", `${tag}:${username}`);
          return new Response(
            renderLoginPage({ redirect: target, error: "Invalid authenticator code." }),
            { status: 401, headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" } },
          );
        }
      }

      clearFailures("login", `${tag}:${username}`);

      const session = createSession({
        userId: user.id,
      });
      void provisionMailbox({ subject: user.id, username: user.username, email: user.email });
      const maxAge = Math.max(
        60,
        Math.floor((new Date(session.expiresAt).getTime() - Date.now()) / 1000),
      );
      // 303 so the browser follows with GET rather than re-POSTing.
      return new Response(null, {
        status: 303,
        headers: {
          location: target ?? defaultPostLogin(),
          "set-cookie": sessionCookie(session.token, maxAge),
          "cache-control": "no-store",
        },
      });
    }

    // ── Explicit Logout (top-level /logout and /api/v1/auth/logout) ──
    if ((request.method === "GET" || request.method === "POST") && (path === "/logout" || path === "/api/v1/auth/logout")) {
      if (auth?.session) {
        revokeSession(auth.session.id);
        audit({ event: "logout", userId: auth.userId, deviceId: auth.session.deviceId });
      }

      if (request.method === "POST" && (!request.headers.get("content-type")?.includes("application/x-www-form-urlencoded") && request.headers.get("accept")?.includes("application/json") || path === "/api/v1/auth/logout")) {
        return jsonResponse(
          { success: true },
          { headers: { "set-cookie": clearedSessionCookie() } },
        );
      }

      const redirectParam = url.searchParams.get("redirect") ?? url.searchParams.get("post_logout_redirect_uri");
      const redirectTarget = safeRedirect(redirectParam, cookieDomain()) || "/login";
      return new Response(null, {
        status: 303,
        headers: {
          location: redirectTarget,
          "set-cookie": clearedSessionCookie(),
          "cache-control": "no-store",
        },
      });
    }

    // ── User Account & Profile Settings UI ──
    if (request.method === "GET" && (path === "/account" || path === "/profile")) {
      if (!auth) {
        return new Response(null, {
          status: 303,
          headers: { location: `/login?redirect=${encodeURIComponent(path)}` },
        });
      }
      const user = getUser(auth.userId);
      if (!user) {
        return new Response(null, { status: 303, headers: { location: "/login" } });
      }
      const allSessions = listSessions(auth.userId);
      const remainingCodes = countRemainingRecoveryCodes(auth.userId);
      const noticeKind = url.searchParams.get("noticeKind") as "success" | "error" | null;
      const noticeMessage = url.searchParams.get("notice");
      const notice = noticeKind && noticeMessage ? { kind: noticeKind, message: noticeMessage } : undefined;
      const rawUser = getRawUser(auth.userId);
      const candidateSecret = url.searchParams.get("setup2fa") ? rawUser?.totpSecret : undefined;
      const totpUri = candidateSecret ? buildTotpUri(user.username, candidateSecret) : undefined;

      const html = renderAccountPage({
        user,
        sessions: allSessions,
        currentSessionId: auth.session?.id,
        remainingRecoveryCodes: remainingCodes,
        totpSecretCandidate: candidateSecret,
        totpUri,
        notice,
        dashboardUrl: defaultPostLogin(),
      });
      return new Response(html, { headers: ONBOARDING_HEADERS });
    }

    if (request.method === "POST" && path === "/account/profile") {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      const form = await request.formData().catch(() => null);
      const displayName = String(form?.get("displayName") ?? "");
      const email = String(form?.get("email") ?? "");
      updateUser(auth.userId, { displayName: displayName || undefined, email: email || undefined });
      return new Response(null, {
        status: 303,
        headers: { location: "/account?noticeKind=success&notice=Profile+updated" },
      });
    }

    if (request.method === "POST" && path === "/account/password") {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      const form = await request.formData().catch(() => null);
      const currentPassword = String(form?.get("currentPassword") ?? "");
      const newPassword = String(form?.get("newPassword") ?? "");
      if (newPassword.length < MIN_PASSWORD_LENGTH) {
        return new Response(null, {
          status: 303,
          headers: { location: `/account?noticeKind=error&notice=Password+must+be+at+least+${MIN_PASSWORD_LENGTH}+characters` },
        });
      }
      const ok = changePassword(auth.userId, currentPassword, newPassword);
      if (!ok) {
        return new Response(null, {
          status: 303,
          headers: { location: "/account?noticeKind=error&notice=Current+password+incorrect" },
        });
      }
      audit({ event: "password_change", userId: auth.userId, deviceId: auth.session?.deviceId });
      return new Response(null, {
        status: 303,
        headers: { location: "/account?noticeKind=success&notice=Password+changed+successfully" },
      });
    }

    if (request.method === "POST" && path === "/account/2fa/setup") {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      const secret = generateTotpSecret();
      setTotpSecret(auth.userId, secret);
      return new Response(null, {
        status: 303,
        headers: { location: "/account?setup2fa=1" },
      });
    }

    if (request.method === "POST" && path === "/account/2fa/verify") {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      const form = await request.formData().catch(() => null);
      const code = String(form?.get("code") ?? "");
      const rawUser = getRawUser(auth.userId);
      if (!rawUser?.totpSecret || !verifyTotpCode(rawUser.totpSecret, code)) {
        return new Response(null, {
          status: 303,
          headers: { location: "/account?setup2fa=1&noticeKind=error&notice=Invalid+6-digit+code.+Please+try+again." },
        });
      }
      enableTotp(auth.userId);
      audit({ event: "totp_enabled", userId: auth.userId, deviceId: auth.session?.deviceId });
      return new Response(null, {
        status: 303,
        headers: { location: "/account?noticeKind=success&notice=Two-factor+authentication+enabled" },
      });
    }

    if (request.method === "POST" && path === "/account/2fa/disable") {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      disableTotp(auth.userId);
      audit({ event: "totp_disabled", userId: auth.userId, deviceId: auth.session?.deviceId });
      return new Response(null, {
        status: 303,
        headers: { location: "/account?noticeKind=success&notice=Two-factor+authentication+disabled" },
      });
    }

    if (request.method === "POST" && path === "/account/recovery-codes/regenerate") {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      const newCodes = issueRecoveryCodes(auth.userId);
      const user = getUser(auth.userId)!;
      const html = renderAccountPage({
        user,
        sessions: listSessions(auth.userId),
        currentSessionId: auth.session?.id,
        remainingRecoveryCodes: newCodes.length,
        newRecoveryCodes: newCodes,
        notice: { kind: "success", message: "New recovery codes generated. Store them safely." },
        dashboardUrl: defaultPostLogin(),
      });
      return new Response(html, { headers: ONBOARDING_HEADERS });
    }

    const sessionRevokeMatch = path.match(/^\/account\/sessions\/([^/]+)\/revoke$/);
    if (request.method === "POST" && sessionRevokeMatch) {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      const [, targetSessionId] = sessionRevokeMatch;
      const targetSession = getSession(targetSessionId);
      if (targetSession && targetSession.userId === auth.userId) {
        revokeSession(targetSessionId);
        audit({ event: "session_revoked", userId: auth.userId, deviceId: targetSession.deviceId });
      }
      if (targetSessionId === auth.session?.id) {
        return new Response(null, {
          status: 303,
          headers: { location: "/login", "set-cookie": clearedSessionCookie() },
        });
      }
      return new Response(null, {
        status: 303,
        headers: { location: "/account?noticeKind=success&notice=Device+session+signed+out" },
      });
    }

    if (request.method === "POST" && path === "/account/sessions/revoke-others") {
      if (!auth) return new Response(null, { status: 303, headers: { location: "/login" } });
      for (const s of listSessions(auth.userId)) {
        if (s.id !== auth.session?.id && !s.revoked) {
          revokeSession(s.id);
        }
      }
      return new Response(null, {
        status: 303,
        headers: { location: "/account?noticeKind=success&notice=Signed+out+of+all+other+devices" },
      });
    }

    // ── Login ──
    // ── Session -> short-lived, audience-scoped identity token ──
    //
    // The proxy calls this with the caller's session cookie and the host the
    // request is bound for. Apps verify the result against the JWKS this
    // service already publishes, so they learn who the user is without
    // calling back here.
    if (request.method === "POST" && path === "/api/v1/auth/identity-token") {
      if (!auth) return jsonResponse({ error: "unauthenticated" }, { status: 401 });

      const body = await parseBody(request);
      const audience = typeof body.audience === "string" ? body.audience.trim() : "";
      if (!audience) {
        return jsonResponse({ error: "audience is required" }, { status: 400 });
      }

      const identityUser = getUser(auth.userId);
      // A session can outlive the account's right to use it — suspension takes
      // effect here rather than only at next login.
      if (!identityUser || identityUser.status !== "active") {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }

      return jsonResponse({
        token: signJwt(buildIdentityClaims(identityUser, audience) as unknown as Record<string, unknown>),
        expiresIn: IDENTITY_TOKEN_TTL_SECONDS,
      });
    }

    // ── Public: request access ──
    if (request.method === "POST" && path === "/api/v1/auth/access-requests") {
      const body = await parseBody(request);
      const username = typeof body.username === "string" ? body.username.trim() : "";
      const email = typeof body.email === "string" ? body.email.trim() : "";
      const note = typeof body.note === "string" ? body.note : undefined;

      if (!username || !email) {
        return jsonResponse({ error: "username and email are required" }, { status: 400 });
      }
      try {
        const result = createAccessRequest({ username, email, note });
        // claimCode is returned exactly once, here. It is never stored in
        // plaintext and there is no way to retrieve it again.
        return jsonResponse(result, { status: 201 });
      } catch {
        // One answer for either clash. Naming which one would let anyone test
        // whether an email address has an account here.
        return jsonResponse({ error: "username_or_email_taken" }, { status: 409 });
      }
    }

    // ── Public: claim an approved account ──
    if (request.method === "POST" && path === "/api/v1/auth/claim") {
      const tag = clientTag(request);
      const limit = checkRateLimit("claim", tag);
      if (!limit.allowed) {
        return jsonResponse({ error: "too_many_attempts" }, {
          status: 429,
          headers: { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) },
        });
      }

      const body = await parseBody(request);
      const result = claimAccount({
        email: typeof body.email === "string" ? body.email : "",
        claimCode: typeof body.claimCode === "string" ? body.claimCode : "",
        password: typeof body.password === "string" ? body.password : "",  // pragma: allowlist secret
      });

      if (!result.ok) {
        recordFailure("claim", tag);
        return jsonResponse({ error: result.reason }, { status: 400 });
      }
      clearFailures("claim", tag);
      return jsonResponse({ user: result.user, recoveryCodes: result.recoveryCodes });
    }

    // ── Public: log in with a recovery code ──
    if (request.method === "POST" && path === "/api/v1/auth/recover") {
      const tag = clientTag(request);
      const limit = checkRateLimit("recover", tag);
      if (!limit.allowed) {
        return jsonResponse({ error: "too_many_attempts" }, {
          status: 429,
          headers: { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) },
        });
      }

      const body = await parseBody(request);
      const email = typeof body.email === "string" ? body.email : "";
      const code = typeof body.code === "string" ? body.code : "";
      const user = findUserByEmail(email);

      if (!user || user.status !== "active" || !consumeRecoveryCode(user.id, code)) {
        recordFailure("recover", tag);
        return jsonResponse({ error: "invalid_code" }, { status: 400 });
      }
      clearFailures("recover", tag);

      const session = createSession({
        userId: user.id,
      });
      return jsonResponse({
        user: sanitizeUser(user),
        token: session.token,
        remainingRecoveryCodes: countRemainingRecoveryCodes(user.id),
      });
    }

    // ── Public: redeem an invite ──
    if (request.method === "POST" && path === "/api/v1/auth/invites/redeem") {
      const tag = clientTag(request);
      const limit = checkRateLimit("invite", tag);
      if (!limit.allowed) {
        return jsonResponse({ error: "too_many_attempts" }, {
          status: 429,
          headers: { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) },
        });
      }

      const body = await parseBody(request);
      const result = redeemInvite({
        code: typeof body.code === "string" ? body.code : "",
        username: typeof body.username === "string" ? body.username : "",
        email: typeof body.email === "string" ? body.email : "",
        password: typeof body.password === "string" ? body.password : "",  // pragma: allowlist secret
      });

      if (!result.ok) {
        recordFailure("invite", tag);
        return jsonResponse({ error: result.reason }, { status: 400 });
      }
      clearFailures("invite", tag);
      return jsonResponse({ user: result.user, recoveryCodes: result.recoveryCodes }, { status: 201 });
    }

    // ── Operator: the approval queue ──
    if (request.method === "GET" && path === "/api/v1/auth/access-requests") {
      if (!auth || !requirePermission(auth.userId, "users:approve")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      return jsonResponse({ requests: listByStatus("pending") });
    }

    // ── Operator: approve / reject ──
    const decision = path.match(/^\/api\/v1\/auth\/access-requests\/([^/]+)\/(approve|reject)$/);
    if (request.method === "POST" && decision) {
      if (!auth || !requirePermission(auth.userId, "users:approve")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const [, targetId, action] = decision;
      const target = getUser(targetId!);
      if (!target) return jsonResponse({ error: "not_found" }, { status: 404 });
      if (target.status !== "pending") {
        return jsonResponse({ error: "not_pending" }, { status: 409 });
      }

      const updated = setUserStatus(
        targetId!,
        action === "approve" ? "approved" : "rejected",
        auth.userId,
      );
      if (action === "approve" && updated) {
        void provisionMailbox({ subject: updated.id, username: updated.username, email: updated.email });
      }
      // Rejection kills any existing sessions — a previously approved user
      // who re-requested and was rejected must not keep working.
      if (action === "reject") revokeAllUserSessions(targetId!);
      // Tell the person they can claim their account. The approval stands
      // whatever happens to the notice; the operator sees the outcome.
      const origin = authPublicOrigin();
      const notice = action === "approve" && target.email
        ? origin
          ? await sendSystemMail({
            template: "access-approved",
            to: target.email,
            link: `${origin}/claim?email=${encodeURIComponent(target.email)}`,
          })
          : "disabled"
        : "not_requested";
      return jsonResponse({ user: updated, notice });
    }

    // ── Operator: mint an invite ──
    if (request.method === "POST" && path === "/api/v1/auth/invites") {
      if (!auth || !requirePermission(auth.userId, "users:create")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const body = await parseBody(request);
      const days = typeof body.expiresInDays === "number" ? body.expiresInDays : undefined;
      const email = typeof body.email === "string" ? body.email.trim() : "";
      if (email && !isPlausibleEmail(email)) {
        return jsonResponse({ error: "invalid_email" }, { status: 400 });
      }
      if (email) {
        // Every emailed invite counts, not only failures: this caps sending.
        const limit = checkRateLimit("invite-email", auth.userId, { max: EMAILED_INVITES_PER_DAY, windowMs: 86_400_000 });
        if (!limit.allowed) {
          return jsonResponse({ error: "daily_invite_limit" }, {
            status: 429,
            headers: { "retry-after": String(Math.ceil(limit.retryAfterMs / 1000)) },
          });
        }
        recordFailure("invite-email", auth.userId);
      }

      const invite = createInvite(auth.userId, days, email || undefined);
      const origin = authPublicOrigin();
      const link = origin ? `${origin}/join?code=${invite.code}` : `/join?code=${invite.code}`;
      const delivery = !email
        ? "not_requested"
        : origin
          ? await sendSystemMail({ template: "invite", to: email, link, expiresAt: invite.expiresAt })
          : "disabled";
      return jsonResponse({ ...invite, link, delivery }, { status: 201 });
    }

    if (request.method === "POST" && path === "/api/v1/auth/login") {
      const body = await parseBody(request);
      const username = typeof body.username === "string" ? body.username.trim() : "";
      const password = typeof body.password === "string" ? body.password : "";  // pragma: allowlist secret

      if (!username || !password) {
        return jsonResponse({ error: "username and password are required" }, { status: 400 });
      }

      // Same brute-force gate as the HTML login above — one bucket per
      // (IP, username) so an attacker can't lock out their target remotely.
      const apiTag = clientTag(request);
      const apiLimit = checkRateLimit("login", `${apiTag}:${username}`);
      if (!apiLimit.allowed) {
        return jsonResponse({ error: "too_many_attempts" }, {
          status: 429,
          headers: { "retry-after": String(Math.ceil(apiLimit.retryAfterMs / 1000)) },
        });
      }

      const user = authenticateUser(username, password);
      if (!user) {
        recordFailure("login", `${apiTag}:${username}`);
        auditLoginFailure(username);
        return jsonResponse({ success: false, reason: "invalid credentials" } as LoginResult, { status: 401 });
      }

      if (user.totpEnabled) {
        const totpToken = typeof body.totpToken === "string" ? body.totpToken.trim() : typeof body.totpCode === "string" ? body.totpCode.trim() : typeof body.code === "string" ? body.code.trim() : "";
        if (!totpToken) {
          return jsonResponse({ error: "totp_required", requires2fa: true, userId: user.id }, { status: 403 });
        }
        const rawUser = getRawUser(user.id);
        if (!rawUser?.totpSecret || !verifyTotpCode(rawUser.totpSecret, totpToken)) {
          recordFailure("login", `${apiTag}:${username}`);
          auditLoginFailure(username);
          return jsonResponse({ success: false, reason: "invalid totp code" } as LoginResult, { status: 401 });
        }
      }

      clearFailures("login", `${apiTag}:${username}`);

      const session = createSession({
        userId: user.id,
      });
      void provisionMailbox({ subject: user.id, username: user.username, email: user.email });
      audit({ event: "login_success", userId: user.id, deviceId: session.deviceId });

      // Body keeps the token for API clients; the cookie is what gives a
      // browser single sign-on across every app on the parent domain.
      const maxAge = Math.max(
        60,
        Math.floor((new Date(session.expiresAt).getTime() - Date.now()) / 1000),
      );
      return jsonResponse(
        {
          success: true,
          token: session.token,
          user,
          sessionId: session.id,
        } as LoginResult,
        { headers: { "set-cookie": sessionCookie(session.token, maxAge) } },
      );
    }

    // ── Logout (POST — API callers) ──
    if (request.method === "POST" && path === "/api/v1/auth/logout") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });

      if (auth.session) {
        revokeSession(auth.session.id);
        audit({ event: "logout", userId: auth.userId, deviceId: auth.session.deviceId });
      }

      // Revoking server-side is what actually ends the session; clearing the
      // cookie stops the browser replaying a dead token to every subdomain.
      return jsonResponse(
        { success: true },
        { headers: { "set-cookie": clearedSessionCookie() } },
      );
    }

    // ── Profile Management API ──
    if (request.method === "GET" && path === "/api/v1/auth/profile") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      const user = getUser(auth.userId);
      if (!user) return jsonResponse({ error: "not_found" }, { status: 404 });
      return jsonResponse({ user });
    }

    if (request.method === "PATCH" && path === "/api/v1/auth/profile") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      const body = await parseBody(request);
      const updated = updateUser(auth.userId, {
        displayName: typeof body.displayName === "string" ? body.displayName : undefined,
        avatar: typeof body.avatar === "string" ? body.avatar : undefined,
        email: typeof body.email === "string" ? body.email : undefined,
      });
      return jsonResponse({ user: updated });
    }

    // ── 2FA TOTP API ──
    if (request.method === "POST" && path === "/api/v1/auth/2fa/setup") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      const user = getUser(auth.userId);
      if (!user) return jsonResponse({ error: "not_found" }, { status: 404 });
      const secret = generateTotpSecret();
      setTotpSecret(auth.userId, secret);
      const uri = buildTotpUri(user.username, secret);
      return jsonResponse({ secret, manualKey: secret, uri });
    }

    if (request.method === "POST" && path === "/api/v1/auth/2fa/verify") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      const body = await parseBody(request);
      const code = typeof body.code === "string" ? body.code : typeof body.token === "string" ? body.token : "";
      const rawUser = getRawUser(auth.userId);
      if (!rawUser?.totpSecret || !verifyTotpCode(rawUser.totpSecret, code)) {
        return jsonResponse({ error: "invalid_totp_code" }, { status: 400 });
      }
      enableTotp(auth.userId);
      audit({ event: "totp_enabled", userId: auth.userId, deviceId: auth.session?.deviceId });
      return jsonResponse({ success: true, enabled: true });
    }

    if (request.method === "POST" && path === "/api/v1/auth/2fa/disable") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      disableTotp(auth.userId);
      audit({ event: "totp_disabled", userId: auth.userId, deviceId: auth.session?.deviceId });
      return jsonResponse({ success: true, enabled: false });
    }

    if (request.method === "POST" && path === "/api/v1/auth/sessions/revoke-others") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      let count = 0;
      for (const s of listSessions(auth.userId)) {
        if (s.id !== auth.session?.id && !s.revoked) {
          revokeSession(s.id);
          count++;
        }
      }
      return jsonResponse({ success: true, revokedCount: count });
    }

    // ── OIDC End-Session (GET — browser redirect from OIDC clients) ──
    // Handles the RP-Initiated Logout flow: clear the ecosystem session if
    // present, then redirect to post_logout_redirect_uri (validated to be
    // a safe https URL) or the auth root.
    if (request.method === "GET" && path === "/api/v1/auth/logout") {
      const redirectRaw = url.searchParams.get("post_logout_redirect_uri") || "";
      let redirectTarget = "/";
      try {
        const parsed = new URL(redirectRaw);
        if (parsed.protocol === "https:" || parsed.protocol === "http:") {
          redirectTarget = parsed.href;
        }
      } catch {
        // malformed or missing — fall back to root
      }

      if (auth?.session) {
        revokeSession(auth.session.id);
        audit({ event: "logout", userId: auth.userId, deviceId: auth.session.deviceId });
      }

      return new Response(null, {
        status: 302,
        headers: {
          location: redirectTarget,
          "set-cookie": clearedSessionCookie(),
        },
      });
    }

    // ── Me ──
    if (request.method === "GET" && path === "/api/v1/auth/me") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });

      const user = getUser(auth.userId);
      if (!user) return jsonResponse({ error: "user not found" }, { status: 404 });

      return jsonResponse({ user });
    }

    // ── Account catalog preferences ──
    // Preferences are presentation only. They are deliberately self-scoped:
    // neither the path nor the body accepts a user id, and they never grant
    // access to an app.
    if (request.method === "GET" && path === "/api/v1/auth/preferences/catalog") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      return jsonResponse({ preferences: getCatalogPreferences(auth.userId) });
    }

    if ((request.method === "PUT" || request.method === "PATCH") && path === "/api/v1/auth/preferences/catalog") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      const body = await parseBody(request);
      try {
        return jsonResponse({ preferences: saveCatalogPreferences(auth.userId, body) });
      } catch {
        return jsonResponse({ error: "invalid_preferences" }, { status: 400 });
      }
    }

    // ── Link DID (proof-of-possession) ──
    if (request.method === "POST" && path === "/api/v1/account/link-did") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      const body = await parseBody(request);
      const did = typeof body.did === "string" ? body.did.trim() : "";
      const signature = typeof body.signature === "string" ? body.signature : "";
      const nonce = typeof body.nonce === "string" ? body.nonce : "";
      if (!did || !signature || !nonce) return jsonResponse({ error: "did, signature and nonce required" }, { status: 400 });

      // Verify the proof-of-possession signature
      const verified = await verifyDidSignature(did, nonce, signature).catch(() => false);
      if (!verified) return jsonResponse({ error: "invalid_signature" }, { status: 400 });

      // Call DID Mapper service to create mapping
      const base = process.env.DID_MAPPER_URL || process.env.DID_MAPPER_BASE || "http://localhost:4001";
      const key = process.env.DID_MAPPER_KEY || process.env.X_API_KEY || process.env.DID_MAPPER_API_KEY || "";
      try {
        const mapRes = await fetch(`${base.replace(/\/+$/, '')}/v1/dids`, {
          method: "POST",
          headers: { "content-type": "application/json", "x-api-key": key },
          body: JSON.stringify({ did, user_id: auth.userId }),
        });
        if (!mapRes.ok) {
          const text = await mapRes.text().catch(() => "");
          return jsonResponse({ error: "did_mapper_error", details: text }, { status: 502 });
        }
      } catch (err) {
        return jsonResponse({ error: "did_mapper_unreachable" }, { status: 502 });
      }

      // Update local user
      const updated = updateUser(auth.userId, { phantom_did: did });
      if (!updated) return jsonResponse({ error: "user_not_found" }, { status: 404 });

      return jsonResponse({ ok: true });
    }

    // ── Users CRUD ──
    if (request.method === "GET" && path === "/api/v1/auth/users") {
      if (!auth || !requirePermission(auth.userId, "users:read")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      return jsonResponse({ users: listUsers() });
    }

    if (request.method === "POST" && path === "/api/v1/auth/users") {
      if (!auth || !requirePermission(auth.userId, "users:create")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }

      const body = await parseBody(request);
      const username = typeof body.username === "string" ? body.username.trim() : "";
      const email = typeof body.email === "string" ? body.email.trim() : "";
      const password = typeof body.password === "string" ? body.password : "";  // pragma: allowlist secret

      if (!username || !email || !password) {
        return jsonResponse({ error: "username, email, and password are required" }, { status: 400 });
      }

      try {
        const user = createUser({ username, email, password, role: "user" });
        void provisionMailbox({ subject: user.id, username: user.username, email: user.email });
        return jsonResponse({ user }, { status: 201 });
      } catch (err) {
        return jsonResponse({ error: (err as Error).message }, { status: 409 });
      }
    }

    const userByIdMatch = path.match(/^\/api\/v1\/auth\/users\/([^/]+)$/);
    if (request.method === "GET" && userByIdMatch) {
      if (!auth || !requirePermission(auth.userId, "users:read")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const [, userId] = userByIdMatch;
      const user = getUser(userId);
      if (!user) return jsonResponse({ error: "not found" }, { status: 404 });
      return jsonResponse({ user });
    }

    if (request.method === "PATCH" && userByIdMatch) {
      if (!auth || !requirePermission(auth.userId, "users:update")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const [, userId] = userByIdMatch;
      const before = getUser(userId);
      const body = await parseBody(request);
      const updated = updateUser(userId, {
        email: typeof body.email === "string" ? body.email : undefined,
        role: typeof body.role === "string" ? body.role as any : undefined,
        status: typeof body.status === "string" ? (body.status as AccountStatus) : undefined,
      });
      if (!updated) return jsonResponse({ error: "not found" }, { status: 404 });

      // Session rotation on privilege change. If role changed, every existing
      // session is stale — it was minted with the old role baked in, and apps
      // read the role from Auth per-request but the *session* is the proof of
      // identity. Revoking forces re-login so the next session carries the
      // new role. Same for suspension: a dead account must not keep working.
      //
      // This was the gap: promoting an attacker who already had a session gave
      // them admin on their next request, and suspending a compromised account
      // left its sessions alive until natural expiry.
      const privilegeChanged =
        (before && body.role !== undefined && before.role !== updated.role) ||
        (before && body.status !== undefined && before.status !== updated.status &&
         (body.status === "suspended" || body.status === "rejected"));
      if (privilegeChanged) {
        revokeAllUserSessions(userId);
        audit({
          event: body.role !== undefined ? "role_change" : "status_change",
          userId,
          actorId: auth?.userId,
          detail: { before: before?.role ?? before?.status, after: updated.role ?? updated.status },
        });
      }

      return jsonResponse({ user: updated });
    }

    if (request.method === "DELETE" && userByIdMatch) {
      if (!auth || !requirePermission(auth.userId, "users:delete")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const [, userId] = userByIdMatch;
      revokeAllUserSessions(userId);
      const deleted = deleteUser(userId);
      if (!deleted) return jsonResponse({ error: "not found" }, { status: 404 });
      return jsonResponse({ deleted: true });
    }

    // ── Change password ──
    const changePwMatch = path.match(/^\/api\/v1\/auth\/users\/([^/]+)\/password$/);
    if (request.method === "POST" && changePwMatch) {
      const [, userId] = changePwMatch;
      // Changing your own password is self-service and must not require an
      // admin permission — this previously demanded users:update, which only
      // founder and admin hold, so ordinary users could not change their own
      // password while an admin could change anyone's. The current password is
      // still required either way, which is what actually authorises it.
      const isSelf = auth?.userId === userId;
      if (!auth || (!isSelf && !requirePermission(auth.userId, "users:update"))) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }

      const body = await parseBody(request);
      const currentPassword = typeof body.currentPassword === "string" ? body.currentPassword : "";  // pragma: allowlist secret
      const newPassword = typeof body.newPassword === "string" ? body.newPassword : "";  // pragma: allowlist secret

      if (!currentPassword || !newPassword) {
        return jsonResponse({ error: "currentPassword and newPassword are required" }, { status: 400 });
      }
      // Enforced here as well as at claim time. Without it the 12-character
      // rule is trivially bypassed: claim with a strong password, then change
      // it to one character.
      if (newPassword.length < MIN_PASSWORD_LENGTH) {
        return jsonResponse({ error: "weak_password" }, { status: 400 });
      }

      const success = changePassword(userId, currentPassword, newPassword);
      if (!success) return jsonResponse({ error: "password change failed" }, { status: 400 });
      audit({ event: "password_change", userId, actorId: auth?.userId, deviceId: auth?.session?.deviceId });
      return jsonResponse({ success: true });
    }

    // ── Recovery codes: how many are left, and issue a fresh set ──
    //
    // Always scoped to the caller's own account. There is no way to read or
    // regenerate someone else's — recovery codes are the one credential that
    // bypasses the password, so an admin path here would be a backdoor.
    if (request.method === "GET" && path === "/api/v1/auth/recovery-codes") {
      if (!auth) return jsonResponse({ error: "unauthenticated" }, { status: 401 });
      return jsonResponse({ remaining: countRemainingRecoveryCodes(auth.userId) });
    }

    if (request.method === "POST" && path === "/api/v1/auth/recovery-codes/regenerate") {
      if (!auth) return jsonResponse({ error: "unauthenticated" }, { status: 401 });
      // Replaces the previous set wholesale, so a leaked old code stops working.
      return jsonResponse({ recoveryCodes: issueRecoveryCodes(auth.userId) });
    }

    // ── Service Tokens ──
    if (request.method === "POST" && path === "/api/v1/auth/tokens/issue") {
      if (!auth || !requirePermission(auth.userId, "tokens:issue")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }

      const body = await parseBody(request);
      const serviceId = typeof body.serviceId === "string" ? body.serviceId.trim() : "";
      if (!serviceId) {
        return jsonResponse({ error: "serviceId is required" }, { status: 400 });
      }

      const issued = issueServiceToken({
        serviceId,
        audience: typeof body.audience === "string" ? body.audience : undefined,
        scopes: Array.isArray(body.scopes) ? body.scopes.filter((s): s is string => typeof s === "string") : undefined,
        expiresInSeconds: typeof body.expiresInSeconds === "number" ? body.expiresInSeconds : undefined,
      });

      return jsonResponse({ status: "ok", token: issued.token, payload: issued.payload });
    }

    if (request.method === "POST" && path === "/api/v1/auth/tokens/validate") {
      const body = await parseBody(request);
      const token = typeof body.token === "string" ? body.token.trim() : "";
      if (!token) return jsonResponse({ error: "token is required" }, { status: 400 });

      const result = validateServiceToken(token, typeof body.expectedAudience === "string" ? body.expectedAudience : undefined);
      if (!result.valid) return jsonResponse({ valid: false, reason: result.reason }, { status: 401 });
      return jsonResponse(result);
    }

    // ── API Keys ──
    if (request.method === "GET" && path === "/api/v1/auth/api-keys") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });

      const isAdmin = requirePermission(auth.userId, "apikeys:read");
      const keys = isAdmin ? listApiKeys() : listApiKeys(auth.userId);
      return jsonResponse({ apiKeys: keys.map(({ keyHash, ...safe }) => safe) });
    }

    if (request.method === "POST" && path === "/api/v1/auth/api-keys") {
      if (!auth || !requirePermission(auth.userId, "apikeys:create")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }

      const body = await parseBody(request);
      const name = typeof body.name === "string" ? body.name.trim() : "";
      if (!name) return jsonResponse({ error: "name is required" }, { status: 400 });

      const result = createApiKey({
        userId: auth.userId,
        name,
        scopes: Array.isArray(body.scopes) ? body.scopes.filter((s): s is string => typeof s === "string") : undefined,
        expiresInDays: typeof body.expiresInDays === "number" ? body.expiresInDays : undefined,
      });

      const { keyHash, ...safeKey } = result.apiKey;
      return jsonResponse({ apiKey: safeKey, rawKey: result.rawKey }, { status: 201 });
    }

    const revokeKeyMatch = path.match(/^\/api\/v1\/auth\/api-keys\/([^/]+)\/revoke$/);
    if (request.method === "POST" && revokeKeyMatch) {
      if (!auth || !requirePermission(auth.userId, "apikeys:revoke")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const [, keyId] = revokeKeyMatch;
      const revoked = revokeApiKey(keyId);
      if (!revoked) return jsonResponse({ error: "api key not found or already revoked" }, { status: 404 });
      return jsonResponse({ revoked: true });
    }

    // ── Sessions ──
    if (request.method === "GET" && path === "/api/v1/auth/sessions") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });

      const isAdmin = requirePermission(auth.userId, "sessions:read");
      const sessions = isAdmin ? listSessions() : listSessions(auth.userId);
      return jsonResponse({
        sessions: sessions.map((x) => ({
          id: x.id,
          deviceId: x.deviceId,
          createdAt: x.createdAt,
          expiresAt: x.expiresAt,
          current: x.id === auth.session?.id,
        })),
      });
    }

    // ── Own recent activity (30 days, no addresses) ──
    if (request.method === "GET" && path === "/api/v1/auth/activity") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });
      return jsonResponse({ events: await recentActivity(auth.userId) });
    }

    const revokeSessionMatch = path.match(/^\/api\/v1\/auth\/sessions\/([^/]+)\/revoke$/);
    if (request.method === "POST" && revokeSessionMatch) {
      if (!auth) return jsonResponse({ error: "forbidden" }, { status: 403 });
      const [, sessionId] = revokeSessionMatch;
      const target = getSession(sessionId);
      if (!target) return jsonResponse({ error: "session not found" }, { status: 404 });
      // Own device: always allowed. Someone else's: needs sessions:revoke.
      if (target.userId !== auth.userId && !requirePermission(auth.userId, "sessions:revoke")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const revoked = revokeSession(sessionId);
      if (!revoked) return jsonResponse({ error: "session not found" }, { status: 404 });
      return jsonResponse({ revoked: true });
    }

    // ── Auth check (for other services) ──
    if (request.method === "GET" && path === "/api/v1/auth/check") {
      if (!auth) return jsonResponse({ authorized: false }, { status: 401 });
      const user = getUser(auth.userId);
      if (!user || user.status !== "active") return jsonResponse({ authorized: false }, { status: 401 });
      return jsonResponse({ authorized: true, userId: auth.userId, user, permissions: getUserPermissions(auth.userId) });
    }

    // ── OAuth JWKS ──
    // Public key only. This endpoint is unauthenticated by design, which is
    // precisely why it must never carry signing material: it previously
    // published NEXUS_AUTH_TOKEN_SECRET as an `oct` key's `k`, so anyone who
    // could reach it could mint tokens for the entire ecosystem.
    if (request.method === "GET" && path === "/api/v1/auth/oauth/jwks") {
      return jsonResponse({ keys: [publicJwk()] });
    }

    // ── OAuth Userinfo ──
    if (request.method === "GET" && path === "/api/v1/auth/oauth/userinfo") {
      if (!auth) {
        return jsonResponse({ error: "invalid_token" }, { status: 401, headers: { "www-authenticate": "Bearer" } });
      }
      const user = getUser(auth.userId);
      if (!user) return jsonResponse({ error: "user not found" }, { status: 404 });
      return jsonResponse({ sub: user.id, username: user.username, email: user.email, role: user.role });
    }

    // ── Guardian trust check ──
    if (request.method === "GET" && path === "/api/v1/auth/trust") {
      const token = extractToken(request);
      if (!token) return jsonResponse({ trusted: false, reason: "no token" }, { status: 401 });

      const session = validateSession(token);
      if (session) {
        const user = getUser(session.userId);
        return jsonResponse({ trusted: true, type: "session", userId: session.userId, user });
      }

      const apiKey = validateApiKey(token);
      if (apiKey) {
        return jsonResponse({ trusted: true, type: "apikey", userId: apiKey.userId, scopes: apiKey.scopes });
      }

      const jwtResult = validateServiceToken(token);
      if (jwtResult.valid) {
        return jsonResponse({ trusted: true, type: "jwt", sub: jwtResult.payload.sub });
      }

      return jsonResponse({ trusted: false, reason: jwtResult.valid === false ? jwtResult.reason : "invalid" }, { status: 401 });
    }

    return jsonResponse({ error: "not found" }, { status: 404 });
}

export function createAuthServer() {
  // Same values handleRequest uses — read once at module scope above.
  const port = AUTH_PORT;
  const baseUrl = AUTH_BASE_URL;

  seedDefaultUsers();

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-auth",
  name: "Nexus Auth",
  description: "Identity provider and service authentication",
  port: 4310,
  capabilities: ["identity","service-auth","token-issuance","user-management","api-key-management","session-management","oauth2","rbac"],
}));

const stopNexusHeartbeat = nexusClient.startCloudHeartbeat();
const stopNexusMonitor = nexusClient.startMonitorHeartbeat();
const server = Bun.serve({
    port,
    // Loopback unless told otherwise. Bun.serve binds every interface by
    // default, which put the identity service — sessions, tokens, user
    // administration — directly on the LAN, reachable without passing the
    // proxy's login gate. Only the proxy should be able to reach this.
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    fetch: handleRequest,
  });

  console.log(`[nexus-auth] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusAuthCloudRegistrationHeartbeat(baseUrl);

  return {
    server,
    port,
    baseUrl,
    stop: () => { stopHeartbeat(); server.stop(); },
  };
}
