import { afterEach, beforeEach, describe, expect, it } from "bun:test";

// Must match the other proxy tests: AUTH_INTERNAL_URL is captured into a
// top-level const the first time src/server is imported in a `bun test` run,
// so every test file sharing that module graph must agree on the value.
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:4399";
process.env.NEXUS_CALENDAR_URL = "http://127.0.0.1:4396";
process.env.NEXUS_CALENDAR_DASHBOARD_SECRET = "hop-secret-for-tests";  // pragma: allowlist secret
const { handleRequest } = await import("../src/server");

type BunServer = ReturnType<typeof Bun.serve>;
let auth: BunServer | null = null;
let calendar: BunServer | null = null;

let authUser: string | null = "usr-alice";
let calendarSaw: Headers | null = null;
let calendarPath: string | null = null;
let calendarMethod: string | null = null;
let calendarBody: string | null = null;
/** When true the calendar service is "down" and never answers. */
let calendarDown = false;

beforeEach(() => {
  authUser = "usr-alice";
  calendarSaw = null;
  calendarPath = null;
  calendarMethod = null;
  calendarBody = null;
  calendarDown = false;

  auth = Bun.serve({
    port: 4399,
    fetch(req) {
      const u = new URL(req.url);
      if (u.pathname !== "/api/v1/auth/me") return new Response("nope", { status: 404 });
      if (authUser === null) return Response.json({ error: "unauthenticated" }, { status: 401 });
      return Response.json({ user: { id: authUser, username: "alice", role: "user" } });
    },
  });

  calendar = Bun.serve({
    port: 4396,
    async fetch(req) {
      if (calendarDown) return new Response("boom", { status: 500 });
      calendarSaw = req.headers;
      const u = new URL(req.url);
      calendarPath = u.pathname + u.search;
      calendarMethod = req.method;
      calendarBody = req.method === "GET" || req.method === "DELETE" ? null : await req.text();
      return Response.json({ ok: true, echoedFrom: u.pathname }, { status: 201 });
    },
  });
});

afterEach(() => {
  auth?.stop(true);
  calendar?.stop(true);
});

const signedIn = { headers: { cookie: "nexus_session=whatever" } };

describe("authentication comes before anything else", () => {
  it("refuses an unauthenticated caller before contacting calendar at all", async () => {
    authUser = null;
    const res = await handleRequest(new Request("http://app.test/ipa/calendar/events"));
    expect(res.status).toBe(401);
    expect(calendarSaw).toBeNull();
  });
});

describe("identity is asserted by Dashboard, never relayed from the browser", () => {
  it("attaches the Auth-derived subject and the private hop secret", async () => {
    await handleRequest(new Request("http://app.test/ipa/calendar/events", signedIn));
    expect(calendarSaw!.get("x-nexus-subject")).toBe("usr-alice");
    expect(calendarSaw!.get("x-nexus-dashboard-secret")).toBe("hop-secret-for-tests");  // pragma: allowlist secret
  });

  it("strips a client-supplied subject rather than forwarding it", async () => {
    await handleRequest(new Request("http://app.test/ipa/calendar/events", {
      headers: { ...signedIn.headers, "x-nexus-subject": "usr-victim" },
    }));
    expect(calendarSaw!.get("x-nexus-subject")).toBe("usr-alice");
  });

  it("strips a client-supplied hop secret rather than forwarding it", async () => {
    await handleRequest(new Request("http://app.test/ipa/calendar/events", {
      headers: { ...signedIn.headers, "x-nexus-dashboard-secret": "guessed" },  // pragma: allowlist secret
    }));
    expect(calendarSaw!.get("x-nexus-dashboard-secret")).toBe("hop-secret-for-tests");  // pragma: allowlist secret
  });

  it("never forwards the browser's cookie to the calendar service", async () => {
    await handleRequest(new Request("http://app.test/ipa/calendar/events", signedIn));
    expect(calendarSaw!.get("cookie")).toBeNull();
  });
});

describe("only the calendar API surface is reachable", () => {
  it.each([
    "/ipa/calendar/events",
    "/ipa/calendar/events/abc-123",
  ])("forwards %s", async (path) => {
    const res = await handleRequest(new Request(`http://app.test${path}`, signedIn));
    expect(res.status).toBe(201);
    expect(calendarPath).toBe(path.replace("/ipa/calendar/", "/api/v1/calendar/"));
  });

  it.each([
    "/ipa/calendar/events/abc/extra",
    "/ipa/calendar/admin",
    "/ipa/calendar/health",
    "/ipa/calendar/",
    // Percent-encoded traversal: this one survives URL normalisation and does
    // reach the handler, so the allow-list is what has to refuse it.
    "/ipa/calendar/events/..%2f..%2fetc%2fpasswd",
    "/ipa/calendar/%2e%2e%2fadmin",
  ])("refuses %s without contacting calendar", async (path) => {
    const res = await handleRequest(new Request(`http://app.test${path}`, signedIn));
    expect(res.status).toBe(404);
    expect(calendarSaw).toBeNull();
  });

  it("never reaches calendar with a dot-segment path", async () => {
    // `new Request()` resolves `../..` before anything routes on it, so this
    // never even matches the calendar prefix. Asserting the status here would
    // be asserting the SPA fallback's behaviour; what matters is the upstream.
    await handleRequest(new Request("http://app.test/ipa/calendar/../../etc/passwd", signedIn));
    expect(calendarSaw).toBeNull();
  });

  it.each(["PUT", "HEAD", "OPTIONS"])("refuses the %s method", async (method) => {
    const res = await handleRequest(new Request("http://app.test/ipa/calendar/events", {
      method, ...signedIn,
    }));
    expect(res.status).toBe(405);
    expect(calendarSaw).toBeNull();
  });

  it.each(["GET", "POST", "PATCH", "DELETE"])("allows the %s method", async (method) => {
    const init: RequestInit = { method, ...signedIn };
    if (method === "POST" || method === "PATCH") init.body = JSON.stringify({ title: "x" });
    const res = await handleRequest(new Request("http://app.test/ipa/calendar/events", init));
    expect(res.status).toBe(201);
    expect(calendarMethod).toBe(method);
  });
});

describe("the request survives the hop intact", () => {
  it("carries the query string", async () => {
    await handleRequest(new Request("http://app.test/ipa/calendar/events?from=2026-09-01&to=2026-09-30", signedIn));
    expect(calendarPath).toBe("/api/v1/calendar/events?from=2026-09-01&to=2026-09-30");
  });

  it("carries the body", async () => {
    await handleRequest(new Request("http://app.test/ipa/calendar/events", {
      method: "POST", body: JSON.stringify({ title: "Standup" }), ...signedIn,
    }));
    expect(calendarBody).toBe(JSON.stringify({ title: "Standup" }));
  });

  it("relays the upstream status rather than flattening it", async () => {
    const res = await handleRequest(new Request("http://app.test/ipa/calendar/events", signedIn));
    expect(res.status).toBe(201);
  });
});

describe("upstream failure is a stable envelope", () => {
  it("answers 503 when calendar cannot be reached", async () => {
    calendar?.stop(true);
    calendar = null;
    const res = await handleRequest(new Request("http://app.test/ipa/calendar/events", signedIn));
    expect(res.status).toBe(503);
    expect(await res.json()).toEqual({ error: "calendar_unavailable" });
  });

  it("does not leak upstream detail on a 5xx", async () => {
    calendarDown = true;
    const res = await handleRequest(new Request("http://app.test/ipa/calendar/events", signedIn));
    expect(await res.text()).not.toContain("boom");
  });
});
