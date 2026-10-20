/**
 * Trust boundary tests for the Calendar delegation path.
 *
 * Three invariants that must never regress:
 * 1. The facade resolves identity from Auth and sets x-nexus-subject to the
 *    Auth-resolved id. A browser-supplied x-nexus-subject is never forwarded.
 * 2. The facade sets x-nexus-dashboard-secret from its own env, not from
 *    anything the browser supplies. The secret never appears in any response.
 * 3. When Calendar is unreachable the facade returns 503 calendar_unavailable.
 */
import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createApiServer } from "../src/server";

function serveOnAvailable(
  start: number,
  fetch: (request: Request) => Response | Promise<Response>,
): ReturnType<typeof Bun.serve> {
  for (let attempt = 0; attempt < 100; attempt += 1) {
    const port = start + Math.floor(Math.random() * 400);
    try {
      return Bun.serve({ hostname: "127.0.0.1", port, fetch });
    } catch (error) {
      if (!(error instanceof Error) || !/EADDRINUSE|port .* in use/i.test(error.message))
        throw error;
    }
  }
  throw new Error("unable to allocate a test port");
}

describe("calendar delegation trust boundary", () => {
  let api: ReturnType<typeof createApiServer>;
  let auth: ReturnType<typeof Bun.serve>;
  let calendar: ReturnType<typeof Bun.serve>;
  let base = "";
  const calendarSecret = "calendar-boundary-test-secret"; // pragma: allowlist secret
  const previous = new Map<string, string | undefined>();
  const capturedCalendarSubjects: string[] = [];
  const capturedCalendarSecrets: string[] = [];

  beforeAll(async () => {
    for (const name of [
      "PORT",
      "NODE_ENV",
      "NEXUS_API_AUTH_URL",
      "NEXUS_API_CALENDAR_URL",
      "NEXUS_CALENDAR_DASHBOARD_SECRET",
    ])
      previous.set(name, process.env[name]);

    process.env.NODE_ENV = "test";

    auth = serveOnAvailable(36100, (request) => {
      if (new URL(request.url).pathname !== "/api/v1/auth/me")
        return new Response("not found", { status: 404 });
      if (request.headers.get("cookie") !== "nexus_session=valid")
        return Response.json({ error: "unauthorized" }, { status: 401 });
      return Response.json({ user: { id: "resolved-calendar-user", role: "member" } });
    });

    calendar = serveOnAvailable(36600, (request) => {
      const subject = request.headers.get("x-nexus-subject") ?? "";
      const secret = request.headers.get("x-nexus-dashboard-secret") ?? "";
      if (subject) capturedCalendarSubjects.push(subject);
      if (secret) capturedCalendarSecrets.push(secret);
      const p = new URL(request.url).pathname;
      if (p !== "/api/v1/calendar/events") return new Response("not found", { status: 404 });
      if (subject !== "resolved-calendar-user" || secret !== calendarSecret)
        return Response.json({ error: "forbidden" }, { status: 403 });
      return Response.json({ events: [] });
    });

    process.env.NEXUS_API_AUTH_URL = `http://127.0.0.1:${auth.port}`;
    process.env.NEXUS_API_CALENDAR_URL = `http://127.0.0.1:${calendar.port}`;
    process.env.NEXUS_CALENDAR_DASHBOARD_SECRET = calendarSecret;

    for (let attempt = 0; attempt < 100; attempt += 1) {
      process.env.PORT = String(36400 + Math.floor(Math.random() * 200));
      try {
        api = createApiServer();
        break;
      } catch (error) {
        if (!(error instanceof Error) || !/EADDRINUSE|port .* in use/i.test(error.message))
          throw error;
      }
    }
    if (!api) throw new Error("unable to allocate the API test port");
    await new Promise((resolve) => setTimeout(resolve, 50));
    base = `http://127.0.0.1:${api.server.port}`;
  });

  afterAll(() => {
    api.close();
    auth.stop();
    calendar.stop();
    for (const [name, value] of previous) {
      if (value === undefined) Reflect.deleteProperty(process.env, name);
      else process.env[name] = value;
    }
  });

  it("forwards Auth-resolved subject to Calendar, never the browser-supplied value", async () => {
    capturedCalendarSubjects.length = 0;

    const res = await fetch(`${base}/api/v1/calendar/events?from=2026-10-01&to=2026-10-31`, {
      headers: {
        cookie: "nexus_session=valid",
        "x-nexus-subject": "attacker-injected-subject",
      },
    });
    expect(res.status).toBe(200);
    expect(capturedCalendarSubjects).toEqual(["resolved-calendar-user"]);
    expect(capturedCalendarSubjects).not.toContain("attacker-injected-subject");
  });

  it("returns 401 when the caller has no valid session", async () => {
    const res = await fetch(`${base}/api/v1/calendar/events?from=2026-10-01&to=2026-10-31`);
    expect(res.status).toBe(401);
  });

  it("returns 503 calendar_unavailable when Calendar is unreachable", async () => {
    const prev = process.env.NEXUS_API_CALENDAR_URL;
    process.env.NEXUS_API_CALENDAR_URL = "http://127.0.0.1:1";
    try {
      const res = await fetch(`${base}/api/v1/calendar/events?from=2026-10-01&to=2026-10-31`, {
        headers: { cookie: "nexus_session=valid" },
      });
      expect(res.status).toBe(503);
      const body = (await res.json()) as { error?: string };
      expect(body.error).toBe("calendar_unavailable");
    } finally {
      process.env.NEXUS_API_CALENDAR_URL = prev;
    }
  });

  it("serves a public event without session authentication", async () => {
    // The public route should bypass Auth — the token alone gates access.
    const fakeToken = "A".repeat(43);
    const res = await fetch(`${base}/api/v1/calendar/public/${fakeToken}`);
    // Calendar stub doesn't have a public handler, so it 404s —
    // what matters is the facade did NOT return 401 or 503 for missing session.
    expect(res.status).not.toBe(401);
    expect(res.status).not.toBe(503);
  });
});
