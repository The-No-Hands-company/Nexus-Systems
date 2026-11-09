import { describe, it, expect, beforeEach } from "bun:test";
import * as users from "../src/users";
import * as recovery from "../src/recovery";
import * as sessions from "../src/sessions";
import { generateTotpCode } from "../src/totp";
import { handleRequest } from "../src/server";

const PASSWORD = "correct-horse-battery-staple-profile";
const BASE = "http://auth.test";

function req(path: string, init: RequestInit = {}) {
  return handleRequest(new Request(`${BASE}${path}`, {
    headers: { "content-type": "application/json", ...(init.headers ?? {}) },
    ...init,
  }));
}

async function signIn(username: string): Promise<{ token: string; id: string; cookie: string }> {
  users.createUser({ username, email: `${username}@x.dev`, password: PASSWORD });
  const res = await req("/api/v1/auth/login", {
    method: "POST",
    body: JSON.stringify({ username, password: PASSWORD }),
  });
  const cookie = res.headers.get("set-cookie") || "";
  const body = (await res.json()) as { token: string; user: { id: string } };
  return { token: body.token, id: body.user.id, cookie };
}

describe("Profile, 2FA TOTP, Sessions & Logout", () => {
  beforeEach(() => {
    users.clearUsers();
    recovery.clearRecoveryStore();
    sessions.clearSessions();
  });

  it("handles explicit /logout endpoint with cookie clearance and redirect", async () => {
    const { token, id, cookie } = await signIn("logoutuser");
    expect(sessions.listSessions(id).filter((s) => !s.revoked)).toHaveLength(1);

    // Call GET /logout
    const res = await handleRequest(new Request(`${BASE}/logout?redirect=https://app.tnhc.dev/`, {
      headers: { cookie },
    }));

    expect(res.status).toBe(303);
    expect(res.headers.get("location")).toBe("https://app.tnhc.dev/");
    const clearedCookie = res.headers.get("set-cookie") || "";
    expect(clearedCookie).toContain("Max-Age=0");

    // Server-side session is revoked
    expect(sessions.listSessions(id).filter((s) => !s.revoked)).toHaveLength(0);
  });

  it("renders /account HTML page when authenticated, redirects to login when not", async () => {
    // Unauthenticated
    const unauthRes = await handleRequest(new Request(`${BASE}/account`));
    expect(unauthRes.status).toBe(303);
    expect(unauthRes.headers.get("location")).toBe("/login?redirect=%2Faccount");

    // Authenticated
    const { cookie } = await signIn("profileuser");
    const authRes = await handleRequest(new Request(`${BASE}/account`, {
      headers: { cookie },
    }));
    expect(authRes.status).toBe(200);
    const html = await authRes.text();
    expect(html).toContain("Account &amp; Security");
    expect(html).toContain("profileuser");
  });

  it("updates user profile via API and form", async () => {
    const { token, id } = await signIn("editoruser");

    // API PATCH
    const patchRes = await req("/api/v1/auth/profile", {
      method: "PATCH",
      headers: { authorization: `Bearer ${token}` },
      body: JSON.stringify({ displayName: "Editor User", email: "newemail@x.dev" }),
    });
    expect(patchRes.status).toBe(200);
    const patchedBody = (await patchRes.json()) as { user: any };
    expect(patchedBody.user.displayName).toBe("Editor User");
    expect(patchedBody.user.email).toBe("newemail@x.dev");

    // API GET
    const getRes = await req("/api/v1/auth/profile", {
      headers: { authorization: `Bearer ${token}` },
    });
    expect(getRes.status).toBe(200);
    const getBody = (await getRes.json()) as { user: any };
    expect(getBody.user.displayName).toBe("Editor User");
  });

  it("walks through 2FA TOTP setup, verification, and enforcement during login", async () => {
    const { token, id } = await signIn("totpuser");

    // Step 1: Request 2FA Setup
    const setupRes = await req("/api/v1/auth/2fa/setup", {
      method: "POST",
      headers: { authorization: `Bearer ${token}` },
    });
    expect(setupRes.status).toBe(200);
    const setupBody = (await setupRes.json()) as { secret: string; uri: string };
    expect(setupBody.secret).toBeDefined();
    expect(setupBody.uri).toContain("otpauth://totp");

    // User is not enabled yet until verified
    let user = users.getUser(id)!;
    expect(user.totpEnabled).toBe(false);

    // Step 2: Verify with correct code
    const validCode = generateTotpCode(setupBody.secret);
    const verifyRes = await req("/api/v1/auth/2fa/verify", {
      method: "POST",
      headers: { authorization: `Bearer ${token}` },
      body: JSON.stringify({ code: validCode }),
    });
    expect(verifyRes.status).toBe(200);
    const verifyBody = (await verifyRes.json()) as { success: boolean; enabled: boolean };
    expect(verifyBody.enabled).toBe(true);

    user = users.getUser(id)!;
    expect(user.totpEnabled).toBe(true);

    // Step 3: Login attempt without TOTP code must require 2FA
    const loginNoTotp = await req("/api/v1/auth/login", {
      method: "POST",
      body: JSON.stringify({ username: "totpuser", password: PASSWORD }),
    });
    expect(loginNoTotp.status).toBe(403);
    const noTotpBody = (await loginNoTotp.json()) as { error: string; requires2fa: boolean };
    expect(noTotpBody.requires2fa).toBe(true);

    // Step 4: Login attempt with wrong code
    const loginBadTotp = await req("/api/v1/auth/login", {
      method: "POST",
      body: JSON.stringify({ username: "totpuser", password: PASSWORD, totpCode: "000000" }),
    });
    expect(loginBadTotp.status).toBe(401);

    // Step 5: Login attempt with valid code
    const freshCode = generateTotpCode(setupBody.secret);
    const loginGoodTotp = await req("/api/v1/auth/login", {
      method: "POST",
      body: JSON.stringify({ username: "totpuser", password: PASSWORD, totpCode: freshCode }),
    });
    expect(loginGoodTotp.status).toBe(200);
    const goodBody = (await loginGoodTotp.json()) as { success: boolean; token: string };
    expect(goodBody.success).toBe(true);
    expect(goodBody.token).toBeDefined();

    // Step 6: Disable 2FA
    const disableRes = await req("/api/v1/auth/2fa/disable", {
      method: "POST",
      headers: { authorization: `Bearer ${goodBody.token}` },
    });
    expect(disableRes.status).toBe(200);
    expect(users.getUser(id)!.totpEnabled).toBe(false);
  });

  it("supports revoking all other sessions", async () => {
    const { token: t1, id } = await signIn("multidevice");
    const s2 = sessions.createSession({ userId: id });
    const s3 = sessions.createSession({ userId: id });

    expect(sessions.listSessions(id).filter((s) => !s.revoked)).toHaveLength(3);

    const revokeOthersRes = await req("/api/v1/auth/sessions/revoke-others", {
      method: "POST",
      headers: { authorization: `Bearer ${t1}` },
    });
    expect(revokeOthersRes.status).toBe(200);

    const activeSessions = sessions.listSessions(id).filter((s) => !s.revoked);
    expect(activeSessions).toHaveLength(1);
    expect(activeSessions[0].id).toBe(sessions.validateSession(t1)!.id);
  });
});
