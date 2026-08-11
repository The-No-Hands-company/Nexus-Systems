import { describe, it, expect, beforeEach, afterEach } from "bun:test";

process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_CLOUD_URL = "http://127.0.0.1:4398";
const { handleRequest } = await import("../src/server");

// Inferred rather than annotated: bun-types' Server is generic and the
// parameter differs across versions.
type BunServer = ReturnType<typeof Bun.serve>;
let auth: BunServer | null = null;
let cloud: BunServer | null = null;

beforeEach(() => {
  auth = Bun.serve({
    port: 4399,
    fetch(req) {
      const u = new URL(req.url);
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
          { id: "nexus-chat", name: "Nexus Chat", publicUrl: "https://chat.tnhc.dev", health: "healthy" },
        ],
      });
    },
  });
});

afterEach(() => {
  auth?.stop(true);
  cloud?.stop(true);
});

describe("dashboard server", () => {
  it("reports health", async () => {
    const res = await handleRequest(new Request("http://app.test/health"));
    expect(res.status).toBe(200);
    expect((await res.json() as { status: string }).status).toBe("ok");
  });

  it("serves the grid from Cloud", async () => {
    const res = await handleRequest(new Request("http://app.test/api/apps"));
    expect(res.status).toBe(200);
    const { apps } = await res.json() as { apps: Array<{ id: string }> };
    expect(apps.map((a) => a.id)).toEqual(["nexus-chat"]);
  });

  it("returns an empty grid rather than failing when Cloud is down", async () => {
    cloud?.stop(true);
    cloud = null;
    const res = await handleRequest(new Request("http://app.test/api/apps"));
    expect(res.status).toBe(200);
    expect((await res.json() as { apps: unknown[] }).apps).toEqual([]);
  });

  it("proxies auth calls same-origin, preserving path and method", async () => {
    const res = await handleRequest(new Request("http://app.test/api/v1/auth/access-requests", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ username: "x", email: "x@y.dev" }),
    }));
    const body = await res.json() as { seenPath: string; method: string };
    expect(body.seenPath).toBe("/api/v1/auth/access-requests");
    expect(body.method).toBe("POST");
  });

  it("forwards the session cookie to Auth", async () => {
    const res = await handleRequest(new Request("http://app.test/api/v1/auth/me", {
      headers: { cookie: "nexus_session=zzz" },
    }));
    expect((await res.json() as { seenCookie: string }).seenCookie).toContain("nexus_session=zzz");
  });

  it("passes Set-Cookie back so signing in actually creates a session", async () => {
    const res = await handleRequest(new Request("http://app.test/api/v1/auth/login", { method: "POST" }));
    expect(res.headers.get("set-cookie") ?? "").toContain("nexus_session=");
  });

  it("never proxies anything outside /api/v1/auth", async () => {
    // An open proxy would let the dashboard be used to reach arbitrary
    // internal services from the public internet.
    const res = await handleRequest(new Request("http://app.test/api/v1/admin/secrets"));
    expect(res.status).toBe(404);
  });

  it("404s an unknown /api path instead of falling through to the SPA", async () => {
    // Otherwise a typo'd API call silently returns HTML and the caller parses
    // "<!doctype html>" as JSON — the exact error the user reported seeing.
    const res = await handleRequest(new Request("http://app.test/api/nope"));
    expect(res.status).toBe(404);
    expect(res.headers.get("content-type") ?? "").toContain("application/json");
  });

  it("serves the SPA shell for a client route", async () => {
    const res = await handleRequest(new Request("http://app.test/claim"));
    expect(res.status).toBe(200);
    expect(res.headers.get("content-type") ?? "").toContain("text/html");
  });
});
