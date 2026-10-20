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

describe("stable platform API", () => {
  let api: ReturnType<typeof createApiServer>;
  let auth: ReturnType<typeof Bun.serve>;
  let cloud: ReturnType<typeof Bun.serve>;
  let mail: ReturnType<typeof Bun.serve>;
  let base = "";
  const previous = new Map<string, string | undefined>();

  beforeAll(async () => {
    for (const name of [
      "PORT",
      "NODE_ENV",
      "NEXUS_API_ENABLE_CLOUD_INTEGRATION",
      "NEXUS_API_AUTH_URL",
      "NEXUS_API_CLOUD_URL",
      "NEXUS_API_CLOUD_API_KEY",
      "NEXUS_API_MAIL_URL",
    ])
      previous.set(name, process.env[name]);

    process.env.NODE_ENV = "test";
    process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION = "false";

    auth = serveOnAvailable(34301, (request) => {
      if (new URL(request.url).pathname !== "/api/v1/auth/me")
        return new Response("not found", { status: 404 });
      if (request.headers.get("cookie") !== "nexus_session=valid")
        return Response.json({ error: "unauthorized" }, { status: 401 });
      return Response.json({ user: { id: "user-1", role: "member", username: "test-user" } });
    });
    cloud = serveOnAvailable(34701, (request) => {
      const url = new URL(request.url);
      if (url.pathname !== "/api/v1/catalog") return new Response("not found", { status: 404 });
      if (request.headers.get("x-api-key") !== "cloud-test-key")
        return Response.json({ error: "forbidden" }, { status: 403 });
      return Response.json({
        apps: [
          {
            id: "nexus-mail",
            releaseState: "released",
            surface: "user-app",
            maturity: "stable",
            reviewStatus: "approved",
            visibility: "default",
            health: "healthy",
          },
          {
            id: "nexus-staged",
            releaseState: "staged",
            surface: "user-app",
            maturity: "stable",
            reviewStatus: "approved",
            visibility: "default",
            health: "healthy",
          },
        ],
      });
    });
    mail = serveOnAvailable(34801, (request) => {
      if (new URL(request.url).pathname !== "/api/v1/folders")
        return new Response("not found", { status: 404 });
      if (request.headers.get("x-nexus-subject") !== "user-1")
        return Response.json({ error: "missing subject" }, { status: 403 });
      return Response.json({ folders: [{ id: "inbox", name: "Inbox" }] });
    });
    process.env.NEXUS_API_AUTH_URL = `http://127.0.0.1:${auth.port}`;
    process.env.NEXUS_API_CLOUD_URL = `http://127.0.0.1:${cloud.port}`;
    process.env.NEXUS_API_CLOUD_API_KEY = "cloud-test-key"; // pragma: allowlist secret — test fixture
    process.env.NEXUS_API_MAIL_URL = `http://127.0.0.1:${mail.port}`;
    for (let attempt = 0; attempt < 100; attempt += 1) {
      process.env.PORT = String(34901 + Math.floor(Math.random() * 400));
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
    cloud.stop();
    mail.stop();
    for (const [name, value] of previous) {
      if (value === undefined) Reflect.deleteProperty(process.env, name);
      else process.env[name] = value;
    }
  });

  it("returns the versioned contract and OpenAPI description publicly", async () => {
    const contract = (await (await fetch(`${base}/api/v1/contract`)).json()) as {
      service: { version: string };
      routes: Array<{ path: string }>;
    };
    expect(contract.service.version).toBe("1.0.0");
    expect(contract.routes.some((route) => route.path === "/api/v1/apps")).toBe(true);

    const openapi = (await (await fetch(`${base}/api/v1/openapi.json`)).json()) as {
      openapi: string;
      paths: Record<string, unknown>;
    };
    expect(openapi.openapi).toBe("3.1.0");
    expect(openapi.paths).toHaveProperty("/api/v1/me");
    expect(openapi.paths).toHaveProperty("/api/v1/apps");
    expect(openapi.paths).toHaveProperty("/api/v1/mail/folders");
  });

  it("delegates identity to Auth and preserves the caller credential", async () => {
    const unauthenticated = await fetch(`${base}/api/v1/me`);
    expect(unauthenticated.status).toBe(401);

    const authenticated = await fetch(`${base}/api/v1/me`, {
      headers: { cookie: "nexus_session=valid" },
    });
    expect(authenticated.status).toBe(200);
    expect(await authenticated.json()).toEqual({
      user: { id: "user-1", role: "member", username: "test-user" },
    });
  });

  it("returns the Cloud catalog without exposing the Cloud key", async () => {
    const response = await fetch(`${base}/api/v1/apps`, {
      headers: { cookie: "nexus_session=valid" },
    });
    expect(response.status).toBe(200);
    expect(await response.json()).toEqual({
      apps: [
        {
          id: "nexus-mail",
          releaseState: "released",
          surface: "user-app",
          maturity: "stable",
          reviewStatus: "approved",
          visibility: "default",
          health: "healthy",
        },
        {
          id: "nexus-staged",
          releaseState: "staged",
          surface: "user-app",
          maturity: "stable",
          reviewStatus: "approved",
          visibility: "default",
          health: "healthy",
        },
      ],
    });
  });

  it("forwards the first released app through the API with an Auth-owned subject", async () => {
    const response = await fetch(`${base}/api/v1/mail/folders`, {
      headers: { cookie: "nexus_session=valid" },
    });
    expect(response.status).toBe(200);
    expect(await response.json()).toEqual({ folders: [{ id: "inbox", name: "Inbox" }] });
  });

  describe("CORS boundary", () => {
    const ALLOWED = "https://app.tnhc.dev";
    const BLOCKED = "https://evil.example.com";

    it("responds to a preflight from the allowed origin with 204 and CORS headers", async () => {
      const res = await fetch(`${base}/api/v1/me`, {
        method: "OPTIONS",
        headers: {
          origin: ALLOWED,
          "access-control-request-method": "GET",
          "access-control-request-headers": "content-type",
        },
      });
      expect(res.status).toBe(204);
      expect(res.headers.get("access-control-allow-origin")).toBe(ALLOWED);
      expect(res.headers.get("access-control-allow-credentials")).toBe("true");
      expect(res.headers.get("vary")).toMatch(/origin/i);
    });

    it("rejects a preflight from a disallowed origin with 403", async () => {
      const res = await fetch(`${base}/api/v1/me`, {
        method: "OPTIONS",
        headers: {
          origin: BLOCKED,
          "access-control-request-method": "GET",
        },
      });
      expect(res.status).toBe(403);
      expect(res.headers.get("access-control-allow-origin")).toBeNull();
    });

    it("adds CORS response headers on credentialed requests from the allowed origin", async () => {
      const res = await fetch(`${base}/api/v1/me`, {
        headers: { cookie: "nexus_session=valid", origin: ALLOWED },
      });
      expect(res.status).toBe(200);
      expect(res.headers.get("access-control-allow-origin")).toBe(ALLOWED);
      expect(res.headers.get("access-control-allow-credentials")).toBe("true");
    });

    it("omits CORS headers on requests from a disallowed origin", async () => {
      const res = await fetch(`${base}/api/v1/me`, {
        headers: { cookie: "nexus_session=valid", origin: BLOCKED },
      });
      expect(res.status).toBe(200);
      expect(res.headers.get("access-control-allow-origin")).toBeNull();
    });
  });
});
