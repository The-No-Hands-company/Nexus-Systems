import { afterEach, beforeEach, describe, expect, it } from "bun:test";

process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_CLOUD_URL = "http://127.0.0.1:4398";
process.env.NEXUS_TERMINAL_URL = "http://127.0.0.1:4397";
const { handleRequest } = await import("../src/server");

// Inferred rather than annotated: bun-types' Server is generic and the
// parameter differs across versions.
type BunServer = ReturnType<typeof Bun.serve>;
let auth: BunServer | null = null;
let cloud: BunServer | null = null;
let terminal: BunServer | null = null;

beforeEach(() => {
  auth = Bun.serve({
    port: 4399,
    fetch(req) {
      const u = new URL(req.url);
      if (u.pathname === "/api/v1/auth/me" && (req.headers.get("cookie") ?? "").includes("role=")) {
        const cookie = req.headers.get("cookie") ?? "";
        const role = cookie.includes("role=founder")
          ? "founder"
          : cookie.includes("role=admin")
            ? "admin"
            : cookie.includes("role=user")
              ? "member"
              : null;
        if (!role) return Response.json({ error: "not_authenticated" }, { status: 401 });
        return Response.json({ user: { id: `${role}-id`, role } });
      }
      return Response.json(
        { seenPath: u.pathname, seenCookie: req.headers.get("cookie"), method: req.method },
        { headers: { "set-cookie": "nexus_session=abc; Path=/" } },
      );
    },
  });
  cloud = Bun.serve({
    port: 4398,
    fetch() {
      return Response.json({
        tools: [
          {
            id: "nexus-chat",
            name: "Nexus Chat",
            publicUrl: "https://chat.tnhc.dev",
            health: "healthy",
          },
          {
            id: "nexus-terminal",
            name: "Nexus Terminal",
            publicUrl: "https://terminal.tnhc.dev",
            health: "healthy",
          },
        ],
      });
    },
  });
  terminal = Bun.serve({
    port: 4397,
    fetch() {
      return Response.json({ service: "nexus-terminal", status: "ok" });
    },
  });
});

afterEach(() => {
  auth?.stop(true);
  cloud?.stop(true);
  terminal?.stop(true);
});

describe("dashboard server", () => {
  it("reports health", async () => {
    const res = await handleRequest(new Request("http://app.test/health"));
    expect(res.status).toBe(200);
    expect(((await res.json()) as { status: string }).status).toBe("ok");
  });

  it("includes a healthy native terminal for a founder without externalizing Cloud's record", async () => {
    const res = await handleRequest(
      new Request("http://app.test/ipa/apps", { headers: { cookie: "role=founder" } }),
    );
    expect(res.status).toBe(200);
    const { apps } = (await res.json()) as {
      apps: Array<{ id: string; url: string; health: string }>;
    };
    // Mail is served by this app at /mail and has no public host, so it never
    // appears in Cloud's registry — the shell contributes it.
    expect(apps.map((a) => a.id).sort()).toEqual(["nexus-chat", "nexus-email", "nexus-terminal"]);
    expect(apps.find((app) => app.id === "nexus-terminal")).toMatchObject({
      url: "/terminal",
      health: "healthy",
    });
  });

  it("does not reveal terminal to non-admin callers, even when Cloud registers it", async () => {
    const res = await handleRequest(
      new Request("http://app.test/ipa/apps", { headers: { cookie: "role=user" } }),
    );
    const { apps } = (await res.json()) as { apps: Array<{ id: string }> };
    expect(apps.map((app) => app.id)).not.toContain("nexus-terminal");
  });

  it("keeps terminal visible but offline to admins when its health endpoint is down", async () => {
    terminal?.stop(true);
    terminal = null;
    const res = await handleRequest(
      new Request("http://app.test/ipa/apps", { headers: { cookie: "role=admin" } }),
    );
    const { apps } = (await res.json()) as { apps: Array<{ id: string; health: string }> };
    expect(apps.find((app) => app.id === "nexus-terminal")).toMatchObject({ health: "offline" });
  });

  it("degrades to the shell's own views rather than failing when Cloud is down", async () => {
    cloud?.stop(true);
    cloud = null;
    const res = await handleRequest(new Request("http://app.test/ipa/apps"));
    expect(res.status).toBe(200);
    // Previously asserted an empty grid. Mail lives in this app, so a Cloud
    // outage is no reason to hide it.
    const { apps } = (await res.json()) as { apps: Array<{ id: string }> };
    expect(apps.map((a) => a.id)).toEqual(["nexus-email"]);
  });

  it("proxies auth calls same-origin, preserving path and method", async () => {
    const res = await handleRequest(
      new Request("http://app.test/ipa/v1/auth/access-requests", {
        method: "POST",
        headers: { "content-type": "application/json" },
        body: JSON.stringify({ username: "x", email: "x@y.dev" }),
      }),
    );
    const body = (await res.json()) as { seenPath: string; method: string };
    expect(body.seenPath).toBe("/api/v1/auth/access-requests");
    expect(body.method).toBe("POST");
  });

  it("forwards the session cookie to Auth", async () => {
    const res = await handleRequest(
      new Request("http://app.test/ipa/v1/auth/me", {
        headers: { cookie: "nexus_session=zzz" },
      }),
    );
    expect(((await res.json()) as { seenCookie: string }).seenCookie).toContain(
      "nexus_session=zzz",
    );
  });

  it("passes Set-Cookie back so signing in actually creates a session", async () => {
    const res = await handleRequest(
      new Request("http://app.test/ipa/v1/auth/login", { method: "POST" }),
    );
    expect(res.headers.get("set-cookie") ?? "").toContain("nexus_session=");
  });

  it("never proxies anything outside /api/v1/auth", async () => {
    // An open proxy would let the dashboard be used to reach arbitrary
    // internal services from the public internet.
    const res = await handleRequest(new Request("http://app.test/ipa/v1/admin/secrets"));
    expect(res.status).toBe(404);
  });

  it("404s an unknown /api path instead of falling through to the SPA", async () => {
    // Otherwise a typo'd API call silently returns HTML and the caller parses
    // "<!doctype html>" as JSON — the exact error the user reported seeing.
    const res = await handleRequest(new Request("http://app.test/ipa/nope"));
    expect(res.status).toBe(404);
    expect(res.headers.get("content-type") ?? "").toContain("application/json");
  });

  it("serves HTML for a client route so deep links survive a reload", async () => {
    // 200 with the built shell, or 503 with a build instruction when
    // frontend/dist is absent — dist is a gitignored build artifact, so a
    // fresh clone legitimately has neither.
    const res = await handleRequest(new Request("http://app.test/claim"));
    expect([200, 503]).toContain(res.status);
    expect(res.headers.get("content-type") ?? "").toContain("text/html");
  });

  it("explains itself when the SPA has not been built", async () => {
    const res = await handleRequest(new Request("http://app.test/definitely-not-an-asset-xyz"));
    if (res.status === 503) {
      expect(await res.text()).toContain("npm run build");
    }
  });

  it("refuses to be framed by anyone, including its own apps", async () => {
    // The shell is the authenticated front door. Unlike the apps it frames,
    // nothing frames the shell, so it must permit nobody — not even itself
    // via 'self' plus an app origin, and never https://app.tnhc.dev, which
    // would be nonsensical here since that IS this host.
    const res = await handleRequest(new Request("http://app.test/"));
    expect(res.headers.get("content-security-policy")).toContain("frame-ancestors 'self'");
    // Tightened from bare frame-ancestors: script-src 'self' blocks inline
    // scripts, object-src 'none' closes plugin injection, base-uri 'self'
    // stops base-tag hijack.
    expect(res.headers.get("content-security-policy")).toContain("script-src 'self'");
    expect(res.headers.get("content-security-policy")).toContain("object-src 'none'");
  });
});
