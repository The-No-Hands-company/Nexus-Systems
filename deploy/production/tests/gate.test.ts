import { describe, it, expect, beforeEach } from "bun:test";
import { isRedirectAllowed, loginRedirect, gate, AUTH_HOST, __resetGateForTest } from "../gate";

const PUBLIC = { upstream: "http://127.0.0.1:9", requiresAuth: false };
const GATED = { upstream: "http://127.0.0.1:9", requiresAuth: true };

function req(url: string, headers: Record<string, string> = {}) {
  return new Request(url, { headers });
}

describe("redirect validation", () => {
  it("allows a return to any host on the configured domain", () => {
    expect(isRedirectAllowed("https://chat.tnhc.dev/rooms/1", "tnhc.dev")).toBe(true);
    expect(isRedirectAllowed("https://tnhc.dev/", "tnhc.dev")).toBe(true);
  });

  it("refuses anything off-domain — this is the phishing case", () => {
    for (const bad of [
      "https://evil.example.com/",
      "https://tnhc.dev.evil.example.com/",   // suffix trick
      "https://eviltnhc.dev/",                // no dot boundary
      "//evil.example.com",                   // protocol-relative
      "http://chat.tnhc.dev/",                // downgrade to plaintext
      "javascript:alert(1)",                  // pragma: allowlist secret
      "not a url",
    ]) {
      expect(isRedirectAllowed(bad, "tnhc.dev")).toBe(false);
    }
  });

  it("never emits an unvalidated Location", () => {
    const res = loginRedirect("https://evil.example.com/steal");
    expect(res.headers.get("location") ?? "").not.toContain("evil.example.com");
  });

  it("preserves a legitimate return address so the user lands where they started", () => {
    const res = loginRedirect("https://chat.tnhc.dev/rooms/1?x=2");
    const loc = res.headers.get("location")!;
    expect(loc).toContain(encodeURIComponent("https://chat.tnhc.dev/rooms/1?x=2"));
  });
});

describe("the gate", () => {
  beforeEach(() => __resetGateForTest());

  it("lets a public route through untouched", async () => {
    const r = await gate(req("https://draw.tnhc.dev/"), PUBLIC);
    expect(r.allow).toBe(true);
    if (r.allow) expect(r.identityToken).toBeNull();
  });

  it("redirects a gated route with no cookie to the login page", async () => {
    const r = await gate(req("https://chat.tnhc.dev/rooms/1"), GATED);
    expect(r.allow).toBe(false);
    if (!r.allow) {
      expect(r.response.status).toBe(302);
      const loc = r.response.headers.get("location")!;
      expect(loc).toContain(AUTH_HOST);
      expect(loc).toContain(encodeURIComponent("https://chat.tnhc.dev/rooms/1"));
    }
  });

  it("redirects a gated route whose session Auth rejects", async () => {
    // The preload points Auth at a dead port, so the exchange fails.
    const r = await gate(req("https://chat.tnhc.dev/", { cookie: "nexus_session=bogus" }), GATED);
    expect(r.allow).toBe(false);
  });

  it("never gates the auth host, whatever the policy says", async () => {
    const r = await gate(req(`https://${AUTH_HOST}/login`), GATED);
    expect(r.allow).toBe(true);
  });

  it("never gates the auth host's API either", async () => {
    const r = await gate(req(`https://${AUTH_HOST}/api/v1/auth/login`), GATED);
    expect(r.allow).toBe(true);
  });

  it("ignores a session cookie on a public route rather than minting for it", async () => {
    // A public route must not pay a round trip to Auth on every request.
    const r = await gate(req("https://draw.tnhc.dev/", { cookie: "nexus_session=x" }), PUBLIC);
    expect(r.allow).toBe(true);
    if (r.allow) expect(r.identityToken).toBeNull();
  });
});
