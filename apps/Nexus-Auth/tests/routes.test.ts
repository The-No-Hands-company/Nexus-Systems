import { describe, it, expect, beforeEach } from "bun:test";
import * as users from "../src/users";
import * as recovery from "../src/recovery";
import * as invites from "../src/invites";
import * as rl from "../src/ratelimit";
import { handleRequest } from "../src/server";

const PASSWORD = "correct-horse-battery-staple";  // pragma: allowlist secret
const BASE = "http://auth.test";

function post(path: string, body: unknown, headers: Record<string, string> = {}) {
  return handleRequest(new Request(`${BASE}${path}`, {
    method: "POST",
    headers: { "content-type": "application/json", ...headers },
    body: JSON.stringify(body),
  }));
}

function get(path: string, headers: Record<string, string> = {}) {
  return handleRequest(new Request(`${BASE}${path}`, { method: "GET", headers }));
}

/** Logs in a founder account and returns its session token. */
async function founderToken(): Promise<string> {
  users.createUser({ username: "boss", email: "boss@x.dev", password: PASSWORD, role: "founder" });
  const res = await post("/api/v1/auth/login", { username: "boss", password: PASSWORD });
  const body = await res.json() as { token: string };
  return body.token;
}

describe("identity routes", () => {
  beforeEach(() => {
    users.clearUsers();
    recovery.clearRecoveryStore();
    invites.clearInviteStore();
    rl.resetRateLimits();
  });

  it("accepts a public access request and returns a claim code once", async () => {
    const res = await post("/api/v1/auth/access-requests", {
      username: "sam", email: "s@x.dev", note: "curious",
    });
    expect(res.status).toBe(201);
    const body = await res.json() as { user: { status: string }; claimCode: string };
    expect(body.user.status).toBe("pending");
    expect(body.claimCode).toMatch(/^[0-9a-f]{32}$/);
  });

  it("requires authorisation to list or approve requests", async () => {
    expect((await get("/api/v1/auth/access-requests")).status).toBe(403);
    expect((await post("/api/v1/auth/access-requests/usr-x/approve", {})).status).toBe(403);
  });

  it("walks the full request -> approve -> claim -> login path", async () => {
    const token = await founderToken();
    const auth = { authorization: `Bearer ${token}` };

    const reqRes = await post("/api/v1/auth/access-requests", { username: "tia", email: "t@x.dev" });
    const { user, claimCode } = await reqRes.json() as { user: { id: string }; claimCode: string };

    const listRes = await get("/api/v1/auth/access-requests", auth);
    expect(listRes.status).toBe(200);
    expect((await listRes.json() as { requests: unknown[] }).requests).toHaveLength(1);

    const approveRes = await post(`/api/v1/auth/access-requests/${user.id}/approve`, {}, auth);
    expect(approveRes.status).toBe(200);

    const claimRes = await post("/api/v1/auth/claim", {
      email: "t@x.dev", claimCode, password: PASSWORD,
    });
    expect(claimRes.status).toBe(200);
    const claimed = await claimRes.json() as { recoveryCodes: string[] };
    expect(claimed.recoveryCodes).toHaveLength(10);

    const loginRes = await post("/api/v1/auth/login", { username: "tia", password: PASSWORD });
    expect(loginRes.status).toBe(200);
  });

  it("rejects a bad claim with 400 and does not leak the reason for unknown emails", async () => {
    const res = await post("/api/v1/auth/claim", {
      email: "ghost@x.dev", claimCode: "0".repeat(32), password: PASSWORD,
    });
    expect(res.status).toBe(400);
    expect((await res.json() as { error: string }).error).toBe("invalid_code");
  });

  it("blocks repeated claim guesses from one address", async () => {
    const bad = { email: "ghost@x.dev", claimCode: "0".repeat(32), password: PASSWORD };
    const ip = { "x-forwarded-for": "9.9.9.9" };
    for (let i = 0; i < 5; i++) await post("/api/v1/auth/claim", bad, ip);

    const res = await post("/api/v1/auth/claim", bad, ip);
    expect(res.status).toBe(429);
  });

  it("logs in with a recovery code and burns it", async () => {
    const token = await founderToken();
    const auth = { authorization: `Bearer ${token}` };
    const { user, claimCode } = await (await post("/api/v1/auth/access-requests", {
      username: "uma", email: "u@x.dev",
    })).json() as { user: { id: string }; claimCode: string };
    await post(`/api/v1/auth/access-requests/${user.id}/approve`, {}, auth);
    const { recoveryCodes } = await (await post("/api/v1/auth/claim", {
      email: "u@x.dev", claimCode, password: PASSWORD,
    })).json() as { recoveryCodes: string[] };

    const first = await post("/api/v1/auth/recover", { email: "u@x.dev", code: recoveryCodes[0] });
    expect(first.status).toBe(200);

    const reuse = await post("/api/v1/auth/recover", { email: "u@x.dev", code: recoveryCodes[0] });
    expect(reuse.status).toBe(400);
  });

  it("suspends an account through the existing user route and blocks its login", async () => {
    const token = await founderToken();
    const auth = { authorization: `Bearer ${token}` };
    users.createUser({ username: "wes", email: "w@x.dev", password: PASSWORD });
    expect((await post("/api/v1/auth/login", { username: "wes", password: PASSWORD })).status).toBe(200);

    const target = users.findUserByUsername("wes")!;
    const patch = await handleRequest(new Request(`${BASE}/api/v1/auth/users/${target.id}`, {
      method: "PATCH",
      headers: { "content-type": "application/json", ...auth },
      body: JSON.stringify({ status: "suspended" }),
    }));
    expect(patch.status).toBe(200);

    const denied = await post("/api/v1/auth/login", { username: "wes", password: PASSWORD });
    expect(denied.status).toBe(401);
  });

  it("mints and redeems an invite", async () => {
    const token = await founderToken();
    const res = await post("/api/v1/auth/invites", {}, { authorization: `Bearer ${token}` });
    expect(res.status).toBe(201);
    const { code } = await res.json() as { code: string };

    const redeem = await post("/api/v1/auth/invites/redeem", {
      code, username: "vic", email: "v@x.dev", password: PASSWORD,
    });
    expect(redeem.status).toBe(201);
    expect((await post("/api/v1/auth/login", { username: "vic", password: PASSWORD })).status).toBe(200);
  });
});
