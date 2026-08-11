import { buildSystemsApiRegistrationPayload } from "./contracts";
import { startNexusAuthCloudRegistrationHeartbeat } from "./cloud";
import {
  createUser,
  authenticateUser,
  findUserByUsername,
  getUser,
  listUsers,
  updateUser,
  changePassword,
  deleteUser,
  userHasPermission,
  seedDefaultUsers,
  sanitizeUser,
  findUserByEmail,
  createAccessRequest,
  claimAccount,
  listByStatus,
  setUserStatus,
} from "./users";
import { consumeRecoveryCode, countRemainingRecoveryCodes } from "./recovery";
import { createInvite, redeemInvite } from "./invites";
import { checkRateLimit, recordFailure, clearFailures } from "./ratelimit";
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
  issueIdToken,
  redeemAuthorizationCode,
  redirectUriAllowed,
  verifyClientSecret,
} from "./oidc";
import { issueServiceToken, validateServiceToken, signJwt } from "./token";
import { buildIdentityClaims, IDENTITY_TOKEN_TTL_SECONDS } from "./identity";
import type { AccountStatus, LoginResult, Permission } from "./types";
import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";
import { verifyDidSignature } from "../../../packages/phantom-utils/src/verify";

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

function getClientIp(request: Request): string {
  return request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ||
    request.headers.get("x-real-ip") ||
    "127.0.0.1";
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

    if (request.method === "GET" && path === "/health") {
      return jsonResponse({ status: "ok", service: "nexus-auth", timestamp: new Date().toISOString() });
    }

    if (request.method === "GET" && path === "/api/v1/auth/readiness") {
      return jsonResponse({
        status: "ready",
        contracts: {
          login: "/api/v1/auth/login",
          logout: "/api/v1/auth/logout",
          users: "/api/v1/auth/users",
          me: "/api/v1/auth/me",
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
      // typed. url.href is same-origin, so safeRedirect accepts it.
      if (!auth) {
        const login = new URL("/login", url.origin);
        login.searchParams.set("redirect", url.href);
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
        ipAddress: getClientIp(request),
        userAgent: request.headers.get("user-agent") || "oidc-client",
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

    // ── Apex sign-in page ──
    // The one place a human types a password. Apps that get an unauthenticated
    // browser navigation send it here with ?redirect=<where they were going>.
    if (request.method === "GET" && path === "/login") {
      const target = safeRedirect(url.searchParams.get("redirect"), cookieDomain());
      // Already signed in: honour the round trip immediately rather than
      // asking for a password that is not needed.
      if (auth && target) {
        return new Response(null, { status: 303, headers: { location: target } });
      }
      return new Response(renderLoginPage({ redirect: target }), {
        status: 200,
        headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" },
      });
    }

    if (request.method === "POST" && path === "/login") {
      const form = await request.formData().catch(() => null);
      const username = String(form?.get("username") ?? "").trim();
      const password = String(form?.get("password") ?? "");  // pragma: allowlist secret
      const target = safeRedirect(String(form?.get("redirect") ?? "") || null, cookieDomain());

      const user = username && password ? authenticateUser(username, password) : null;
      if (!user) {
        return new Response(
          renderLoginPage({ redirect: target, error: "Incorrect username or password." }),
          { status: 401, headers: { "content-type": "text/html; charset=utf-8", "cache-control": "no-store" } },
        );
      }

      const session = createSession({
        userId: user.id,
        ipAddress: getClientIp(request),
        userAgent: request.headers.get("user-agent") || "unknown",
      });
      const maxAge = Math.max(
        60,
        Math.floor((new Date(session.expiresAt).getTime() - Date.now()) / 1000),
      );
      // 303 so the browser follows with GET rather than re-POSTing.
      return new Response(null, {
        status: 303,
        headers: {
          location: target ?? "/login",
          "set-cookie": sessionCookie(session.token, maxAge),
          "cache-control": "no-store",
        },
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
      } catch (err) {
        return jsonResponse({ error: (err as Error).message }, { status: 409 });
      }
    }

    // ── Public: claim an approved account ──
    if (request.method === "POST" && path === "/api/v1/auth/claim") {
      const ip = getClientIp(request);
      const limit = checkRateLimit("claim", ip);
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
        recordFailure("claim", ip);
        return jsonResponse({ error: result.reason }, { status: 400 });
      }
      clearFailures("claim", ip);
      return jsonResponse({ user: result.user, recoveryCodes: result.recoveryCodes });
    }

    // ── Public: log in with a recovery code ──
    if (request.method === "POST" && path === "/api/v1/auth/recover") {
      const ip = getClientIp(request);
      const limit = checkRateLimit("recover", ip);
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
        recordFailure("recover", ip);
        return jsonResponse({ error: "invalid_code" }, { status: 400 });
      }
      clearFailures("recover", ip);

      const session = createSession({
        userId: user.id,
        ipAddress: ip,
        userAgent: request.headers.get("user-agent") || "",
      });
      return jsonResponse({
        user: sanitizeUser(user),
        token: session.token,
        remainingRecoveryCodes: countRemainingRecoveryCodes(user.id),
      });
    }

    // ── Public: redeem an invite ──
    if (request.method === "POST" && path === "/api/v1/auth/invites/redeem") {
      const ip = getClientIp(request);
      const limit = checkRateLimit("invite", ip);
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
        recordFailure("invite", ip);
        return jsonResponse({ error: result.reason }, { status: 400 });
      }
      clearFailures("invite", ip);
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
      return jsonResponse({ user: updated });
    }

    // ── Operator: mint an invite ──
    if (request.method === "POST" && path === "/api/v1/auth/invites") {
      if (!auth || !requirePermission(auth.userId, "users:create")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const body = await parseBody(request);
      const days = typeof body.expiresInDays === "number" ? body.expiresInDays : undefined;
      return jsonResponse(createInvite(auth.userId, days), { status: 201 });
    }

    if (request.method === "POST" && path === "/api/v1/auth/login") {
      const body = await parseBody(request);
      const username = typeof body.username === "string" ? body.username.trim() : "";
      const password = typeof body.password === "string" ? body.password : "";  // pragma: allowlist secret

      if (!username || !password) {
        return jsonResponse({ error: "username and password are required" }, { status: 400 });
      }

      const user = authenticateUser(username, password);
      if (!user) {
        return jsonResponse({ success: false, reason: "invalid credentials" } as LoginResult, { status: 401 });
      }

      const session = createSession({
        userId: user.id,
        ipAddress: getClientIp(request),
        userAgent: request.headers.get("user-agent") || "unknown",
      });

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

    // ── Logout ──
    if (request.method === "POST" && path === "/api/v1/auth/logout") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });

      if (auth.session) {
        revokeSession(auth.session.id);
      }

      // Revoking server-side is what actually ends the session; clearing the
      // cookie stops the browser replaying a dead token to every subdomain.
      return jsonResponse(
        { success: true },
        { headers: { "set-cookie": clearedSessionCookie() } },
      );
    }

    // ── Me ──
    if (request.method === "GET" && path === "/api/v1/auth/me") {
      if (!auth) return jsonResponse({ error: "unauthorized" }, { status: 401 });

      const user = getUser(auth.userId);
      if (!user) return jsonResponse({ error: "user not found" }, { status: 404 });

      return jsonResponse({ user });
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
      const body = await parseBody(request);
      const updated = updateUser(userId, {
        email: typeof body.email === "string" ? body.email : undefined,
        role: typeof body.role === "string" ? body.role as any : undefined,
        status: typeof body.status === "string" ? (body.status as AccountStatus) : undefined,
      });
      if (!updated) return jsonResponse({ error: "not found" }, { status: 404 });
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
      if (!auth || !requirePermission(auth.userId, "users:update")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const [, userId] = changePwMatch;
      const body = await parseBody(request);
      const currentPassword = typeof body.currentPassword === "string" ? body.currentPassword : "";  // pragma: allowlist secret
      const newPassword = typeof body.newPassword === "string" ? body.newPassword : "";  // pragma: allowlist secret

      if (!currentPassword || !newPassword) {
        return jsonResponse({ error: "currentPassword and newPassword are required" }, { status: 400 });
      }

      const success = changePassword(userId, currentPassword, newPassword);
      if (!success) return jsonResponse({ error: "password change failed" }, { status: 400 });
      return jsonResponse({ success: true });
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
      return jsonResponse({ sessions });
    }

    const revokeSessionMatch = path.match(/^\/api\/v1\/auth\/sessions\/([^/]+)\/revoke$/);
    if (request.method === "POST" && revokeSessionMatch) {
      if (!auth || !requirePermission(auth.userId, "sessions:revoke")) {
        return jsonResponse({ error: "forbidden" }, { status: 403 });
      }
      const [, sessionId] = revokeSessionMatch;
      const revoked = revokeSession(sessionId);
      if (!revoked) return jsonResponse({ error: "session not found" }, { status: 404 });
      return jsonResponse({ revoked: true });
    }

    // ── Auth check (for other services) ──
    if (request.method === "GET" && path === "/api/v1/auth/check") {
      if (!auth) return jsonResponse({ authorized: false }, { status: 401 });
      const user = getUser(auth.userId);
      return jsonResponse({ authorized: true, userId: auth.userId, user });
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
