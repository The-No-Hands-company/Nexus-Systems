import { describe, it, expect, beforeEach } from "bun:test";
import { gate, isRedirectAllowed } from "../gate";

beforeEach(() => { delete process.env.GATE_SKIP_AUTH; });

function req(host: string, path = "/", cookie?: string): Request {
  const headers: Record<string, string> = {};
  if (cookie) headers.cookie = cookie;
  return new Request(`https://${host}${path}`, { headers });
}

describe("SSO gate", () => {
  it("allows auth.tnhc.dev unconditionally (login page must be public)", async () => {
    const d = await gate(req("auth.tnhc.dev"), { upstream: "x", requiresAuth: false });
    expect(d.allow).toBe(true);
  });

  it("redirects app.tnhc.dev without a session", async () => {
    const d = await gate(req("app.tnhc.dev"), { upstream: "x", requiresAuth: false });
    expect(d.allow).toBe(false);
    if (!d.allow) expect(d.response.status).toBe(302);
  });

  it("redirects cloud.tnhc.dev even when route says requiresAuth=false", async () => {
    // The old behaviour trusted Cloud's requiresAuth flag; now the gate
    // default-denies regardless.
    const d = await gate(req("cloud.tnhc.dev"), { upstream: "x", requiresAuth: false });
    expect(d.allow).toBe(false);
  });

  it("exempts /health paths on any host", async () => {
    const d = await gate(req("app.tnhc.dev", "/health"), { upstream: "x", requiresAuth: true });
    expect(d.allow).toBe(true);
  });

  it("redirect target preserves the original path", () => {
    expect(isRedirectAllowed("https://app.tnhc.dev/cloud/tools")).toBe(true);
    expect(isRedirectAllowed("https://evil.com")).toBe(false);
    expect(isRedirectAllowed("http://app.tnhc.dev")).toBe(false); // http, not https
  });
});
