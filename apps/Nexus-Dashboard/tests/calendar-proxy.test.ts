import { afterEach, beforeEach, describe, expect, it } from "bun:test";

const originalEnvironment = new Map(
  [
    "NEXUS_AUTH_INTERNAL_URL",
    "NEXUS_CALENDAR_API_URL",
    "NEXUS_CALENDAR_WEB_URL",
    "NEXUS_CALENDAR_DASHBOARD_SECRET",
  ].map((key) => [key, process.env[key]] as const),
);
const { proxyCalendarApi, proxyCalendarWeb } = await import("../src/calendar-proxy");

let apiRequests: Request[] = [];
let webRequests: Request[] = [];
let authenticated = true;
let apiAvailable = true;
const realFetch = globalThis.fetch;

beforeEach(() => {
  apiRequests = [];
  webRequests = [];
  authenticated = true;
  apiAvailable = true;
  process.env.NEXUS_AUTH_INTERNAL_URL = "http://auth.test";
  process.env.NEXUS_CALENDAR_API_URL = "http://calendar-api.test";
  process.env.NEXUS_CALENDAR_WEB_URL = "http://calendar-web.test";
  process.env.NEXUS_CALENDAR_DASHBOARD_SECRET = "calendar-dashboard-test-secret"; // pragma: allowlist secret
  globalThis.fetch = async (input, init) => {
    const request = new Request(input instanceof Request ? input : input.toString(), init);
    const url = new URL(request.url);
    if (url.origin === "http://auth.test") {
      return authenticated
        ? Response.json({ user: { id: "usr-alice", role: "member" } })
        : Response.json({ error: "not_authenticated" }, { status: 401 });
    }
    if (url.origin === "http://calendar-api.test") {
      if (!apiAvailable) throw new Error("Calendar API unavailable");
      apiRequests.push(request.clone());
      return new Response(await request.text(), {
        status: 207,
        headers: {
          "content-type": "application/json",
          "cache-control": "private, max-age=60",
          etag: '"calendar-etag"',
          "set-cookie": "must-not-reach-browser=true",
        },
      });
    }
    if (url.origin === "http://calendar-web.test") {
      webRequests.push(request.clone());
      if (url.pathname.endsWith(".js")) {
        return new Response("export {}", { headers: { "content-type": "text/javascript" } });
      }
      return new Response("<!doctype html><main>Calendar</main>", {
        headers: {
          "content-type": "text/html; charset=utf-8",
          "content-security-policy": "default-src 'self'; frame-ancestors 'self'",
          "referrer-policy": "no-referrer",
        },
      });
    }
    throw new Error(`Unexpected fetch target: ${url.origin}`);
  };
});

afterEach(() => {
  globalThis.fetch = realFetch;
  for (const [key, value] of originalEnvironment) {
    if (value === undefined) delete process.env[key];
    else process.env[key] = value;
  }
});

describe("Calendar API proxy", () => {
  it("authenticates before any Calendar API connection", async () => {
    authenticated = false;

    const res = await proxyCalendarApi(
      new Request("http://app.test/ipa/calendar/events"),
      "events",
      "",
    );

    expect(res.status).toBe(401);
    expect(apiRequests).toHaveLength(0);
  });

  it("uses Auth identity and the private hop secret while preserving a request contract", async () => {
    const res = await proxyCalendarApi(
      new Request("http://app.test/ipa/calendar/events?from=2026-09-01&to=2026-09-30", {
        method: "POST",
        headers: {
          accept: "application/json",
          "content-type": "application/json",
          "x-nexus-subject": "usr-victim",
          "x-nexus-dashboard-secret": "browser-secret", // pragma: allowlist secret
          "x-nexus-identity": "browser-identity",
        },
        body: '{"title":"Planning"}',
      }),
      "events",
      "?from=2026-09-01&to=2026-09-30",
    );

    expect(res.status).toBe(207);
    expect(await res.text()).toBe('{"title":"Planning"}');
    expect(res.headers.get("content-type")).toContain("application/json");
    expect(res.headers.get("cache-control")).toBe("private, max-age=60");
    expect(res.headers.get("etag")).toBe('"calendar-etag"');
    expect(res.headers.get("set-cookie")).toBeNull();
    expect(apiRequests).toHaveLength(1);
    const upstream = apiRequests[0]!;
    expect(new URL(upstream.url).pathname).toBe("/api/v1/calendar/events");
    expect(new URL(upstream.url).search).toBe("?from=2026-09-01&to=2026-09-30");
    expect(upstream.method).toBe("POST");
    expect(upstream.headers.get("accept")).toBe("application/json");
    expect(upstream.headers.get("content-type")).toBe("application/json");
    expect(upstream.headers.get("x-nexus-subject")).toBe("usr-alice");
    expect(upstream.headers.get("x-nexus-dashboard-secret")).toBe("calendar-dashboard-test-secret");
    expect(upstream.headers.get("x-nexus-identity")).toBeNull();
  });

  it("forwards every supported mutation only to its matching Calendar resource", async () => {
    const cases = [
      ["GET", "events", ""],
      ["POST", "events", '{"title":"New"}'],
      ["PATCH", "events/event_123", '{"title":"Changed"}'],
      ["PUT", "events/event_123/shares/usr-bob", '{"permission":"viewer"}'],
      ["DELETE", "events/event_123", ""],
    ] as const;

    for (const [method, rest, body] of cases) {
      const response = await proxyCalendarApi(
        new Request(`http://app.test/ipa/calendar/${rest}`, {
          method,
          ...(body ? { headers: { "content-type": "application/json" }, body } : {}),
        }),
        rest,
        "",
      );
      expect(response.status).toBe(207);
    }

    expect(apiRequests.map((request) => `${request.method} ${new URL(request.url).pathname}`)).toEqual([
      "GET /api/v1/calendar/events",
      "POST /api/v1/calendar/events",
      "PATCH /api/v1/calendar/events/event_123",
      "PUT /api/v1/calendar/events/event_123/shares/usr-bob",
      "DELETE /api/v1/calendar/events/event_123",
    ]);
  });

  it("rejects paths and methods outside the Calendar API allow-list without upstream contact", async () => {
    const secret = await proxyCalendarApi(
      new Request("http://app.test/ipa/calendar/admin/secrets"),
      "admin/secrets",
      "",
    );
    const wrongMethod = await proxyCalendarApi(
      new Request("http://app.test/ipa/calendar/events", { method: "PUT" }),
      "events",
      "",
    );

    expect(secret.status).toBe(404);
    expect(wrongMethod.status).toBe(405);
    expect(apiRequests).toHaveLength(0);
  });

  it("returns a stable unavailable response when the private API is down", async () => {
    apiAvailable = false;

    const res = await proxyCalendarApi(
      new Request("http://app.test/ipa/calendar/events"),
      "events",
      "",
    );

    expect(res.status).toBe(503);
    expect(await res.json()).toEqual({ error: "calendar_unavailable" });
  });
});

describe("Calendar web proxy", () => {
  it("serves the Calendar web artifact for the shell root and client routes without an iframe", async () => {
    for (const relativePath of ["", "/", "/month/2026-09"] as const) {
      const res = await proxyCalendarWeb(
        new Request(`http://app.test/calendar${relativePath}`),
        relativePath,
      );
      expect(res.status).toBe(200);
      expect(await res.text()).toContain("Calendar");
      expect(res.headers.get("x-nexus-shell-context")).toBe("proxied-app");
      expect(res.headers.get("cache-control")).toBe("no-cache, no-store, must-revalidate");
      expect(res.headers.get("content-security-policy")).toBe("default-src 'self'; frame-ancestors 'self'");
      expect(res.headers.get("referrer-policy")).toBe("no-referrer");
    }

    expect(webRequests.map((request) => new URL(request.url).pathname)).toEqual([
      "/",
      "/",
      "/month/2026-09",
    ]);
  });

  it("keeps immutable caching for hashed Calendar assets", async () => {
    for (const relativePath of [
      "/assets/app.abcdef123.js",
      "/assets/index-Cx1abc234def.js",
    ]) {
      const res = await proxyCalendarWeb(
        new Request(`http://app.test/calendar${relativePath}`),
        relativePath,
      );

      expect(res.status).toBe(200);
      expect(res.headers.get("content-type")).toContain("text/javascript");
      expect(res.headers.get("cache-control")).toBe("public, max-age=31536000, immutable");
      expect(res.headers.get("x-nexus-shell-context")).toBe("proxied-app");
    }
  });

  it("rejects traversal and Calendar API paths instead of treating them as web routes", async () => {
    const traversal = await proxyCalendarWeb(
      new Request("http://app.test/calendar/%2e%2e/private"),
      "/%2e%2e/private",
    );
    const apiConfusion = await proxyCalendarWeb(
      new Request("http://app.test/calendar/api/v1/calendar/events"),
      "/api/v1/calendar/events",
    );
    const encodedTraversal = await proxyCalendarWeb(
      new Request("http://app.test/calendar/%252e%252e/private"),
      "/%252e%252e/private",
    );
    const protocolRelative = await proxyCalendarWeb(
      new Request("http://app.test/calendar//evil.example/private"),
      "//evil.example/private",
    );

    expect(traversal.status).toBe(404);
    expect(apiConfusion.status).toBe(404);
    expect(encodedTraversal.status).toBe(404);
    expect(protocolRelative.status).toBe(404);
    expect(webRequests).toHaveLength(0);
  });
});
