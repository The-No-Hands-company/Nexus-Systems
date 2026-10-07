import { describe, it, expect, beforeEach } from "bun:test";
import { handleRequest } from "../src/server";
import { resetRateLimits } from "../src/ratelimit";
import { createUser, clearUsers } from "../src/users";
import { clearSessions, validateSession } from "../src/sessions";
import { setAuditBackendForTests, drain } from "../src/audit";

const URL_BASE = "http://localhost:4310";
const PW = "correct-horse-battery-1"; // pragma: allowlist secret

beforeEach(() => {
  resetRateLimits();
  clearSessions();
  clearUsers();
  createUser({ username: "privacy-user", email: "privacy@nexus.local", password: PW });
});

async function signIn(opts: { password?: string; expectFail?: boolean; headers?: Record<string, string> } = {}) {
  const res = await handleRequest(new Request(`${URL_BASE}/api/v1/auth/login`, {
    method: "POST",
    headers: { "content-type": "application/json", ...(opts.headers ?? {}) },
    body: JSON.stringify({ username: "privacy-user", password: opts.password ?? PW }),
  }));
  const body: any = await res.json().catch(() => ({}));
  return { status: res.status, authHeaders: { authorization: `Bearer ${body.token}` } as Record<string, string> };
}

const get = (path: string, headers: Record<string, string>) =>
  handleRequest(new Request(`${URL_BASE}${path}`, { headers }));

describe("zero retention in Auth", () => {
  it("never stores an address or user agent on a session", async () => {
    const res = await signIn({ headers: { "x-forwarded-for": "203.0.113.20", "user-agent": "Canary/1.0", "x-nexus-client-tag": "tagAAAAAAAAAAAAAAAAAAA" } });
    const list: any = await (await get("/api/v1/auth/sessions", res.authHeaders)).json();
    const text = JSON.stringify(list);
    expect(text).not.toContain("203.0.113.20");
    expect(text).not.toContain("Canary/1.0");
    expect(list.sessions[0].deviceId).toMatch(/^dev_[A-Za-z0-9_-]{16,}$/);
    expect(list.sessions[0].current).toBe(true);
  });
  it("rate-limits by tag + account, not by address", async () => {
    const h = { "x-nexus-client-tag": "tagBBBBBBBBBBBBBBBBBBB" };
    for (let i = 0; i < 10; i++) await signIn({ password: "wrong", expectFail: true, headers: h }); // pragma: allowlist secret
    const blocked = await signIn({ password: "wrong", expectFail: true, headers: h }); // pragma: allowlist secret
    expect(blocked.status).toBe(429);
    // A different tag from the same claimed address is not blocked.
    const other = await signIn({ password: "wrong", expectFail: true, headers: { "x-nexus-client-tag": "tagCCCCCCCCCCCCCCCCCCC", "x-forwarded-for": "1.2.3.4" } }); // pragma: allowlist secret
    expect(other.status).toBe(401);
  });
  it("shows a user only their own recent activity, without addresses", async () => {
    const me = await signIn({ headers: { "x-forwarded-for": "198.51.100.9" } });
    const body: any = await (await get("/api/v1/auth/activity", me.authHeaders)).json();
    expect(Array.isArray(body.events)).toBe(true);
    expect(JSON.stringify(body)).not.toContain("198.51.100.9");
  });

  it("shows a failed attempt against my account in my activity, without deviceId or caller data", async () => {
    const rows: any[] = [];
    setAuditBackendForTests({
      flush: async (entries) => { for (const e of entries) rows.push({ ...e, at: new Date() }); },
      query: async (_sql, params) => rows.filter((r) => r.userId === params[0])
        .map((r) => ({ event: r.event, device_id: r.deviceId ?? null, created_at: r.at })),
    });
    try {
      const me = await signIn();
      await signIn({ password: "wrong", expectFail: true, headers: { "x-nexus-client-tag": "tagDDDDDDDDDDDDDDDDDDD" } }); // pragma: allowlist secret
      await handleRequest(new Request(`${URL_BASE}/api/v1/auth/login`, {
        method: "POST", headers: { "content-type": "application/json" },
        body: JSON.stringify({ username: "nobody-here", password: "x" }),
      }));
      await drain();
      const body: any = await (await get("/api/v1/auth/activity", me.authHeaders)).json();
      const fails = body.events.filter((e: any) => e.event === "login_failure");
      expect(fails.length).toBe(1);
      expect(fails[0].deviceId).toBeNull();
      expect(JSON.stringify(body)).not.toContain("tagDDDD");
      expect(rows.filter((r) => r.event === "login_failure").length).toBe(1); // unknown user: no row
    } finally {
      setAuditBackendForTests(null);
    }
  });

  it("lets a user revoke their own session but not someone else's", async () => {
    createUser({ username: "other-user", email: "other@nexus.local", password: PW });
    const me = await signIn();
    const otherRes = await handleRequest(new Request(`${URL_BASE}/api/v1/auth/login`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ username: "other-user", password: PW }),
    }));
    const other: any = await otherRes.json();
    const post = (id: string) => handleRequest(new Request(`${URL_BASE}/api/v1/auth/sessions/${id}/revoke`, { method: "POST", headers: me.authHeaders }));

    expect((await post(other.sessionId)).status).toBe(403);
    expect((await post("ses-does-not-exist")).status).toBe(404);

    const list: any = await (await get("/api/v1/auth/sessions", me.authHeaders)).json();
    const own = list.sessions.find((x: any) => x.current);
    const token = me.authHeaders.authorization.slice(7);
    expect(validateSession(token)).toBeDefined();
    expect((await post(own.id)).status).toBe(200);
    expect(validateSession(token)).toBeUndefined();
  });
});
