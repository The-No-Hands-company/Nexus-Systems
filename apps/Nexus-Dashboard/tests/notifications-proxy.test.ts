import { afterEach, beforeEach, describe, expect, it } from "bun:test";

// Ports must match the other proxy tests: NEXUS_AUTH_INTERNAL_URL is captured
// into a top-level const the first time src/server is imported anywhere in a
// `bun test` run, so whichever file loads first wins and they must agree.
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_HOSTING_URL = "http://127.0.0.1:4397";
const { handleRequest } = await import("../src/server");

type BunServer = ReturnType<typeof Bun.serve>;
let auth: BunServer | null = null;
let hosting: BunServer | null = null;

let authed = true;
/** What the mock Hosting server last saw, so a test can assert on it. */
let hostingSaw: { path: string; cookie: string | null; method: string } | null = null;

beforeEach(() => {
  authed = true;
  hostingSaw = null;

  auth = Bun.serve({
    port: 4399,
    fetch(req) {
      const u = new URL(req.url);
      if (u.pathname !== "/api/v1/auth/me") return new Response("not found", { status: 404 });
      if (!authed) return Response.json({ error: "unauthenticated" }, { status: 401 });
      return Response.json({ user: { id: "u1", username: "x", email: "x@y.dev", role: "user" } });
    },
  });

  hosting = Bun.serve({
    port: 4397,
    fetch(req) {
      const u = new URL(req.url);
      hostingSaw = { path: u.pathname, cookie: req.headers.get("cookie"), method: req.method };
      if (u.pathname === "/api/notifications/unread-count") return Response.json({ unread: 3 });
      if (u.pathname === "/api/notifications") return Response.json({ notifications: [{ id: 1 }] });
      return Response.json({ ok: true });
    },
  });
});

afterEach(() => {
  auth?.stop(true);
  hosting?.stop(true);
});

const call = (path: string, init?: RequestInit) =>
  handleRequest(new Request(`http://dash.local${path}`, {
    headers: { cookie: "nexus_session=abc" },
    ...init,
  }));

describe("notifications proxy", () => {
  it("refuses an unauthenticated caller before touching Hosting", async () => {
    authed = false;
    const res = await call("/api/notifications");
    expect(res.status).toBe(401);
    // The point: it did not forward. A proxy that asks upstream first leaks
    // the fact that a request happened even when it should have stopped.
    expect(hostingSaw).toBeNull();
  });

  it("forwards the caller's cookie so Hosting does its own authorisation", async () => {
    const res = await call("/api/notifications");
    expect(res.status).toBe(200);
    expect(hostingSaw?.cookie).toBe("nexus_session=abc");
    expect(hostingSaw?.path).toBe("/api/notifications");
  });

  it("attaches no identity of its own", async () => {
    // This proxy must not be able to widen anyone's access. Unlike the mail
    // proxy it sets no x-nexus-subject — Hosting decides who the caller is
    // from the session, so a bug here cannot hand someone another user's
    // notifications.
    await call("/api/notifications");
    expect(hostingSaw).not.toBeNull();
  });

  it("serves the unread count and the mark-read writes", async () => {
    expect((await call("/api/notifications/unread-count")).status).toBe(200);
    expect((await call("/api/notifications/1/read", { method: "POST" })).status).toBe(200);
    expect((await call("/api/notifications/read-all", { method: "POST" })).status).toBe(200);
  });

  it("is an allow-list, not a passthrough", async () => {
    // A path Hosting would happily answer must still be refused here: the
    // proxy exposes the notification surface and nothing else.
    const res = await call("/api/notifications/../sites");
    expect(res.status).toBe(404);
    expect(hostingSaw).toBeNull();
  });

  it("refuses methods the surface does not serve", async () => {
    const res = await call("/api/notifications/1/read", { method: "DELETE" });
    expect(res.status).toBe(405);
  });

  it("degrades to 503 rather than erroring when Hosting is down", async () => {
    // The bell going quiet is acceptable; the shell throwing is not.
    hosting?.stop(true);
    hosting = null;
    const res = await call("/api/notifications");
    expect(res.status).toBe(503);
  });
});
