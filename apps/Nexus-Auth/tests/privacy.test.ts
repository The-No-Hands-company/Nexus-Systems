import { describe, it, expect, beforeEach } from "bun:test";
import { handleRequest } from "../src/server";
import { resetRateLimits } from "../src/ratelimit";
import { createUser, clearUsers } from "../src/users";
import { clearSessions } from "../src/sessions";

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
});
