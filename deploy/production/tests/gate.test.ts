import { describe, it, expect, beforeEach } from "bun:test";
import {
  isRedirectAllowed,
  loginRedirect,
  gate,
  publicUrl,
  readCookies,
  AUTH_HOST,
  STALE_MS,
  FRESH_MS,
  __resetGateForTest,
} from "../gate";
import type { RouteTarget } from "../proxy";

/**
 * An application host. Default-deny: only hashed build output is readable
 * without a session.
 */
const APP: RouteTarget = { upstream: "http://127.0.0.1:9", requiresAuth: true, kind: "app" };

/**
 * A user-deployed site behind Hosting's site-proxy. Public by design — this is
 * the concept that replaced the old `requiresAuth: false` notion of "public",
 * which the gate no longer honours because a per-tool flag could not be trusted
 * as policy.
 */
const SITE: RouteTarget = { upstream: "http://127.0.0.1:9", requiresAuth: false, kind: "site" };

function req(url: string, headers: Record<string, string> = {}) {
  return new Request(url, { headers });
}

/**
 * proxy.test.ts sets GATE_SKIP_AUTH for its whole file to exercise forwarding
 * without the gate in the way. That variable makes gate() allow everything, so
 * if it were still set when this file ran, every "requires a session"
 * expectation below would pass for the wrong reason and this suite would become
 * unable to fail. Cleared before each test rather than assumed absent.
 */
beforeEach(() => {
  delete process.env.GATE_SKIP_AUTH;
  __resetGateForTest();
});

describe("the test harness itself", () => {
  it("runs with the auth gate active", () => {
    expect(process.env.GATE_SKIP_AUTH).toBeUndefined();
  });
});

// ── Redirect validation ─────────────────────────────────────────────────────

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

// ── The gate ────────────────────────────────────────────────────────────────

describe("the gate", () => {
  beforeEach(() => __resetGateForTest());

  it("lets a site through untouched", async () => {
    const r = await gate(req("https://mysite.tnhc.dev/"), SITE);
    expect(r.allow).toBe(true);
    if (r.allow) expect(r.identityToken).toBeNull();
  });

  it("redirects an app host with no cookie to the login page", async () => {
    const r = await gate(req("https://chat.tnhc.dev/rooms/1"), APP);
    expect(r.allow).toBe(false);
    if (!r.allow) {
      expect(r.response.status).toBe(302);
      const loc = r.response.headers.get("location")!;
      expect(loc).toContain(AUTH_HOST);
      expect(loc).toContain(encodeURIComponent("https://chat.tnhc.dev/rooms/1"));
    }
  });

  it("redirects an app host whose session Auth rejects", async () => {
    // The preload points Auth at a dead port, so the exchange fails.
    const r = await gate(req("https://chat.tnhc.dev/", { cookie: "nexus_session=bogus" }), APP);
    expect(r.allow).toBe(false);
  });

  it("never gates the auth host, whatever the policy says", async () => {
    const r = await gate(req(`https://${AUTH_HOST}/login`), APP);
    expect(r.allow).toBe(true);
  });

  it("never gates the auth host's API either", async () => {
    const r = await gate(req(`https://${AUTH_HOST}/api/v1/auth/login`), APP);
    expect(r.allow).toBe(true);
  });

  it("ignores a session cookie on a site rather than minting for it", async () => {
    // A public site must not pay a round trip to Auth on every request.
    const r = await gate(req("https://mysite.tnhc.dev/", { cookie: "nexus_session=x" }), SITE);
    expect(r.allow).toBe(true);
    if (r.allow) expect(r.identityToken).toBeNull();
  });

  it("exempts the health paths on any host", async () => {
    for (const p of ["/health", "/health/live", "/health/ready"]) {
      const r = await gate(req(`https://app.tnhc.dev${p}`), APP);
      expect(r.allow).toBe(true);
    }
  });
});

// ── What is public on an application host, exactly ──────────────────────────

describe("app hosts are default-deny", () => {
  beforeEach(() => __resetGateForTest());

  // The regression this suite exists for. The old rule allowed any path with no
  // dot in it, so every one of these was readable with no cookie at all — on
  // cloud.tnhc.dev, verified against the running proxy. Naming them
  // individually is the point: a future change that reopens any single one of
  // them fails here rather than in production.
  it.each([
    "/",
    "/index.html",
    "/v1/users",
    "/gateway",
    "/graphql",
    "/rest/tools",
    "/admin/users/export",
    "/internal/metrics",
    "/webhooks/github",
    "/api/v1/routes",
    "/ipa/secrets",
    "/dashboard/settings",
  ])("requires a session for %s", async (path) => {
    const r = await gate(req(`https://cloud.tnhc.dev${path}`), APP);
    expect(r.allow).toBe(false);
    if (!r.allow) expect(r.response.status).toBe(302);
  });

  it("serves hashed build output without a session", async () => {
    // Vite's output. No user data in it, and keeping it readable means a
    // signed-in page still renders during a brief Auth outage.
    const r = await gate(req("https://app.tnhc.dev/assets/index-BsdR7088.js"), APP);
    expect(r.allow).toBe(true);
    if (r.allow) expect(r.identityToken).toBeNull();
  });

  // The other half of the old rule's failure: a dot meant "gated", so ordinary
  // static files at the document root were redirected to a login page. On a
  // site that made the CSS and the favicon unreachable while the homepage
  // rendered, which is the shape of the bug that hid it.
  it.each(["/style.css", "/app.js", "/favicon.ico", "/robots.txt"])(
    "serves %s on a site without a session",
    async (path) => {
      const r = await gate(req(`https://mysite.tnhc.dev${path}`), SITE);
      expect(r.allow).toBe(true);
    },
  );

  it("does not let a site's public-ness leak to an app host", async () => {
    const r = await gate(req("https://app.tnhc.dev/style.css"), APP);
    expect(r.allow).toBe(false);
  });

  it("ignores Cloud's requiresAuth flag on an app host", async () => {
    // buildTool() drops the field on registration and there is no setter, so
    // the route table cannot be the source of truth. kind is.
    const permissive: RouteTarget = { upstream: "http://127.0.0.1:9", requiresAuth: false, kind: "app" };
    const r = await gate(req("https://cloud.tnhc.dev/"), permissive);
    expect(r.allow).toBe(false);
  });
});

// ── publicUrl ───────────────────────────────────────────────────────────────

describe("publicUrl", () => {
  // Regression: this was found end-to-end, not by a unit test. The gate built
  // its return address from req.url, which behind the tunnel is always http://,
  // so isRedirectAllowed rejected it and every gated login landed on the apex
  // marketing page instead of the page the user asked for.
  it("rewrites the tunnel's http hop to the public https address", () => {
    const r = new Request("http://echo.tnhc.dev/rooms/1?x=2");
    expect(publicUrl(r)).toBe("https://echo.tnhc.dev/rooms/1?x=2");
  });

  it("drops a port the origin only sees because of the tunnel", () => {
    const r = new Request("http://echo.tnhc.dev:8080/probe");
    expect(publicUrl(r)).toBe("https://echo.tnhc.dev/probe");
  });

  it("produces an address the redirect allowlist accepts", () => {
    const r = new Request("http://chat.tnhc.dev/deep/link");
    expect(isRedirectAllowed(publicUrl(r), "tnhc.dev")).toBe(true);
  });

  it("still refuses an off-domain host after the rewrite", () => {
    // The scheme was never the protection — the host is, and it must survive.
    const r = new Request("http://evil.example.com/steal");
    expect(isRedirectAllowed(publicUrl(r), "tnhc.dev")).toBe(false);
  });

  it("carries a deep link all the way into the login redirect", async () => {
    // The two halves together: a gated deep link over the tunnel's http hop
    // must still come back as an https return address the allowlist accepts.
    const r = await gate(new Request("http://app.tnhc.dev:8080/cloud/tools?tab=2"), APP);
    expect(r.allow).toBe(false);
    if (!r.allow) {
      const loc = r.response.headers.get("location")!;
      expect(loc).toContain(encodeURIComponent("https://app.tnhc.dev/cloud/tools?tab=2"));
    }
  });
});

// ── Hardening ───────────────────────────────────────────────────────────────

describe("gate hardening", () => {
  beforeEach(() => __resetGateForTest());

  // Found by review, confirmed live: this returned 500 from the proxy.
  // decodeURIComponent throws on a malformed escape, readCookie did not catch
  // it, and gate() is called outside the proxy's try — so one header crashed
  // the path every gated request takes.
  it("treats an undecodable session cookie as no session, not a crash", async () => {
    const r = await gate(req("http://chat.tnhc.dev/", { cookie: "nexus_session=%zz" }), APP);
    expect(r.allow).toBe(false);
    if (!r.allow) expect(r.response.status).toBe(302);
  });

  it("survives a cookie header that is not key=value at all", async () => {
    const r = await gate(req("http://chat.tnhc.dev/", { cookie: "garbage" }), APP);
    expect(r.allow).toBe(false);
  });

  it("still lets a site through with a broken cookie", async () => {
    const r = await gate(req("http://mysite.tnhc.dev/", { cookie: "nexus_session=%zz" }), SITE);
    expect(r.allow).toBe(true);
  });

  it("never serves a cached token past the lifetime Auth minted it for", () => {
    // Auth's IDENTITY_TOKEN_TTL_SECONDS is 120. A stale window longer than
    // that hands out tokens the app is guaranteed to reject as expired, which
    // is worse than a clean redirect.
    expect(STALE_MS).toBeLessThan(120_000);
    expect(STALE_MS).toBeGreaterThan(FRESH_MS);
  });
});

// ── Duplicate session cookies ───────────────────────────────────────────────

describe("duplicate session cookies", () => {
  beforeEach(() => __resetGateForTest());

  // Browsers hold same-named cookies at different scopes and send them all,
  // most-specific first. Reading only the first let one stale host-scoped
  // cookie permanently shadow a valid ecosystem session.
  it("reads every value sent under the session name", () => {
    const r = new Request("http://chat.tnhc.dev/", {
      headers: { cookie: "nexus_session=stale; other=x; nexus_session=good" },
    });
    expect(readCookies(r, "nexus_session")).toEqual(["stale", "good"]);
  });

  it("skips an undecodable value instead of giving up on the rest", () => {
    const r = new Request("http://chat.tnhc.dev/", {
      headers: { cookie: "nexus_session=%zz; nexus_session=good" },
    });
    expect(readCookies(r, "nexus_session")).toEqual(["good"]);
  });

  it("returns nothing when the name is absent", () => {
    const r = new Request("http://chat.tnhc.dev/", { headers: { cookie: "other=x" } });
    expect(readCookies(r, "nexus_session")).toEqual([]);
  });
});
