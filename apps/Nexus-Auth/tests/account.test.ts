import { describe, it, expect, beforeEach } from "bun:test";
import * as users from "../src/users";
import * as recovery from "../src/recovery";
import { handleRequest } from "../src/server";

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret
const BASE = "http://auth.test";

function req(path: string, init: RequestInit = {}) {
  return handleRequest(new Request(`${BASE}${path}`, {
    headers: { "content-type": "application/json", ...(init.headers ?? {}) },
    ...init,
  }));
}

async function signIn(username: string): Promise<{ token: string; id: string }> {
  users.createUser({ username, email: `${username}@x.dev`, password: PASSWORD });
  const res = await req("/api/v1/auth/login", {
    method: "POST",
    body: JSON.stringify({ username, password: PASSWORD }),
  });
  const body = await res.json() as { token: string; user: { id: string } };
  return { token: body.token, id: body.user.id };
}

describe("recovery code management", () => {
  beforeEach(() => {
    users.clearUsers();
    recovery.clearRecoveryStore();
  });

  it("refuses an unauthenticated caller", async () => {
    expect((await req("/api/v1/auth/recovery-codes")).status).toBe(401);
    expect((await req("/api/v1/auth/recovery-codes/regenerate", { method: "POST" })).status).toBe(401);
  });

  it("reports how many codes remain", async () => {
    const { token, id } = await signIn("ada");
    recovery.issueRecoveryCodes(id);

    const res = await req("/api/v1/auth/recovery-codes", {
      headers: { authorization: `Bearer ${token}` },
    });
    expect(res.status).toBe(200);
    expect((await res.json() as { remaining: number }).remaining).toBe(10);
  });

  it("regenerates ten codes and invalidates the old set", async () => {
    const { token, id } = await signIn("bo");
    const [oldCode] = recovery.issueRecoveryCodes(id);

    const res = await req("/api/v1/auth/recovery-codes/regenerate", {
      method: "POST",
      headers: { authorization: `Bearer ${token}` },
    });
    expect(res.status).toBe(200);
    const { recoveryCodes } = await res.json() as { recoveryCodes: string[] };
    expect(recoveryCodes).toHaveLength(10);

    // A regenerated set must retire the previous one, or a leaked old code
    // keeps working forever.
    expect(recovery.consumeRecoveryCode(id, oldCode!)).toBe(false);
    expect(recovery.consumeRecoveryCode(id, recoveryCodes[0]!)).toBe(true);
  });

  it("only ever reports the caller's own codes", async () => {
    const ada = await signIn("ada2");
    const bo = await signIn("bo2");
    recovery.issueRecoveryCodes(ada.id);
    // bo has none; asking with bo's session must not see ada's.
    const res = await req("/api/v1/auth/recovery-codes", {
      headers: { authorization: `Bearer ${bo.token}` },
    });
    expect((await res.json() as { remaining: number }).remaining).toBe(0);
  });
});

describe("password change", () => {
  beforeEach(() => {
    users.clearUsers();
    recovery.clearRecoveryStore();
  });

  it("changes the password when the current one is right", async () => {
    const { token, id } = await signIn("cy");
    const res = await req(`/api/v1/auth/users/${id}/password`, {
      method: "POST",
      headers: { authorization: `Bearer ${token}` },
      body: JSON.stringify({ currentPassword: PASSWORD, newPassword: "another-good-password-1" }),  // pragma: allowlist secret
    });
    expect(res.status).toBe(200);
    expect(users.authenticateUser("cy", "another-good-password-1")).not.toBeNull();
  });

  it("refuses a new password below the minimum length", async () => {
    // Otherwise the 12-character rule enforced at claim time is trivially
    // bypassed: claim strong, then immediately change to one character.
    const { token, id } = await signIn("di");
    const res = await req(`/api/v1/auth/users/${id}/password`, {
      method: "POST",
      headers: { authorization: `Bearer ${token}` },
      body: JSON.stringify({ currentPassword: PASSWORD, newPassword: "short" }),  // pragma: allowlist secret
    });
    expect(res.status).toBe(400);
    // The old password must still work — a rejected change must not half-apply.
    expect(users.authenticateUser("di", PASSWORD)).not.toBeNull();
  });

  it("refuses when the current password is wrong", async () => {
    const { token, id } = await signIn("erin");
    const res = await req(`/api/v1/auth/users/${id}/password`, {
      method: "POST",
      headers: { authorization: `Bearer ${token}` },
      body: JSON.stringify({ currentPassword: "not-it-at-all", newPassword: "another-good-password-1" }),  // pragma: allowlist secret
    });
    expect(res.status).toBe(400);
    expect(users.authenticateUser("erin", PASSWORD)).not.toBeNull();
  });
});
