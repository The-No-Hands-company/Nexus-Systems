import { describe, it, expect, beforeEach, afterEach } from "bun:test";

// A distinctive value so a test can grep for it in a response body/headers
// and be confident that *would* have caught a leak, not just failed to look.
const CLOUD_API_KEY = "sekret-operator-key-do-not-leak"; // pragma: allowlist secret

// Same ports server.test.ts uses. AUTH_INTERNAL_URL is captured into a
// top-level const the first time src/server is imported anywhere in this
// `bun test` run, and both test files import it — whichever file's module
// graph loads first "wins" for that constant, so the two files must agree on
// its value rather than each picking their own.
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_CLOUD_URL = "http://127.0.0.1:4398";
process.env.NEXUS_CLOUD_API_KEY = CLOUD_API_KEY;
const { handleRequest } = await import("../src/server");

type BunServer = ReturnType<typeof Bun.serve>;
let auth: BunServer | null = null;
let cloud: BunServer | null = null;

/** Role the mock Auth server's /api/v1/auth/me hands back for this test. */
let authRole: string | null = "user";
/** Headers the mock Cloud server most recently saw, so a test can assert on them. */
let cloudSawHeaders: Headers | null = null;

beforeEach(() => {
  authRole = "user";
  cloudSawHeaders = null;

  auth = Bun.serve({
    port: 4399,
    fetch(req) {
      const u = new URL(req.url);
      if (u.pathname !== "/api/v1/auth/me") return new Response("not found", { status: 404 });
      if (authRole === null) return Response.json({ error: "unauthenticated" }, { status: 401 });
      return Response.json({ user: { id: "u1", username: "x", email: "x@y.dev", role: authRole } });
    },
  });

  cloud = Bun.serve({
    port: 4398,
    fetch(req) {
      cloudSawHeaders = req.headers;
      const u = new URL(req.url);
      if (u.pathname === "/api/v1/audit") {
        return Response.json({ events: [{ id: "e1", eventType: "node-trust-action" }] });
      }
      if (u.pathname === "/api/v1/users") {
        return Response.json({ users: [{ id: "u1", username: "founder" }] });
      }
      if (u.pathname === "/v1/federation/peers") {
        return Response.json({ peers: [{ id: "p1" }] });
      }
      return Response.json({ error: "not_found" }, { status: 404 });
    },
  });
});

afterEach(() => {
  auth?.stop(true);
  cloud?.stop(true);
});

describe("the /api/cloud proxy", () => {
  it("forwards an allow-listed path to Cloud and returns its body", async () => {
    const res = await handleRequest(new Request("http://app.test/api/cloud/audit"));
    expect(res.status).toBe(200);
    const body = await res.json() as { events: Array<{ id: string }> };
    expect(body.events[0]?.id).toBe("e1");
  });

  it("refuses a path that is not on the allow-list — no forwarding at all", async () => {
    const res = await handleRequest(new Request("http://app.test/api/cloud/tools/nexus-chat/restart"));
    expect(res.status).toBe(404);
    // Not merely rejected — Cloud must never have seen this request.
    expect(cloudSawHeaders).toBeNull();
  });

  it("never proxies a mutating method, even to an allow-listed name", async () => {
    const res = await handleRequest(new Request("http://app.test/api/cloud/audit", { method: "POST" }));
    expect(res.status).toBe(404);
    expect(cloudSawHeaders).toBeNull();
  });

  it("attaches the operator's key to the upstream request", async () => {
    await handleRequest(new Request("http://app.test/api/cloud/audit"));
    expect(cloudSawHeaders?.get("x-api-key")).toBe(CLOUD_API_KEY);
  });

  it("never lets the operator's key reach the response the browser sees", async () => {
    const res = await handleRequest(new Request("http://app.test/api/cloud/audit"));
    const text = await res.text();
    expect(text).not.toContain(CLOUD_API_KEY);
    for (const [, value] of res.headers) {
      expect(value).not.toContain(CLOUD_API_KEY);
    }
  });

  it("blocks a non-admin caller from the users endpoint", async () => {
    authRole = "user";
    const res = await handleRequest(
      new Request("http://app.test/api/cloud/users", { headers: { cookie: "nexus_session=zzz" } }),
    );
    expect(res.status).toBe(403);
    expect(cloudSawHeaders).toBeNull(); // Cloud is never even asked.
  });

  it("blocks a signed-out caller from the users endpoint", async () => {
    authRole = null; // Auth's /me answers 401
    const res = await handleRequest(new Request("http://app.test/api/cloud/users"));
    expect(res.status).toBe(403);
  });

  it("lets a founder read the users endpoint", async () => {
    authRole = "founder";
    const res = await handleRequest(
      new Request("http://app.test/api/cloud/users", { headers: { cookie: "nexus_session=zzz" } }),
    );
    expect(res.status).toBe(200);
    const body = await res.json() as { users: Array<{ id: string }> };
    expect(body.users[0]?.id).toBe("u1");
  });

  it("lets an admin read the users endpoint too", async () => {
    authRole = "admin";
    const res = await handleRequest(new Request("http://app.test/api/cloud/users"));
    expect(res.status).toBe(200);
  });

  it("does not gate a non-admin-only path behind a role check", async () => {
    // Reading audit data needs no Auth round trip: only users is admin-only.
    await handleRequest(new Request("http://app.test/api/cloud/audit"));
    expect(cloudSawHeaders).not.toBeNull();
  });

  it("reaches a nested allow-listed name unchanged", async () => {
    const res = await handleRequest(new Request("http://app.test/api/cloud/federation/peers"));
    expect(res.status).toBe(200);
    const body = await res.json() as { peers: Array<{ id: string }> };
    expect(body.peers[0]?.id).toBe("p1");
  });

  it("degrades instead of 500ing when Cloud is unreachable", async () => {
    cloud?.stop(true);
    cloud = null;
    const res = await handleRequest(new Request("http://app.test/api/cloud/audit"));
    expect(res.status).not.toBe(500);
    // The UI must be able to render this as JSON, not choke on a stack trace.
    const body = await res.json() as { error: string };
    expect(typeof body.error).toBe("string");
  });

  it("degrades on the admin-only path too when Cloud is unreachable", async () => {
    authRole = "founder";
    cloud?.stop(true);
    cloud = null;
    const res = await handleRequest(new Request("http://app.test/api/cloud/users"));
    expect(res.status).not.toBe(500);
  });
});
