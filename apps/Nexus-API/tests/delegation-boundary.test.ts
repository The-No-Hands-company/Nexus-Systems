/**
 * Trust boundary tests for the platform facade.
 *
 * Three invariants that must never regress:
 * 1. A browser-supplied x-nexus-subject is NEVER forwarded to downstream services.
 *    The facade resolves identity from Auth and injects its own subject.
 * 2. The Cloud API key is a server-side secret; it must never appear in any
 *    response body returned to the caller.
 * 3. When Cloud is unreachable, the catalog endpoint returns 503 rather than
 *    leaking an internal error or returning a partial/empty catalog silently.
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

describe("delegation trust boundary", () => {
  let api: ReturnType<typeof createApiServer>;
  let auth: ReturnType<typeof Bun.serve>;
  let cloud: ReturnType<typeof Bun.serve>;
  let mail: ReturnType<typeof Bun.serve>;
  let base = "";
  const previous = new Map<string, string | undefined>();
  const capturedMailSubjects: string[] = [];
  const capturedMailHeaders: Record<string, string>[] = [];
  const cloudApiKey = "super-secret-cloud-key-test-fixture"; // pragma: allowlist secret

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

    auth = serveOnAvailable(35100, (request) => {
      if (new URL(request.url).pathname !== "/api/v1/auth/me")
        return new Response("not found", { status: 404 });
      if (request.headers.get("cookie") !== "nexus_session=valid")
        return Response.json({ error: "unauthorized" }, { status: 401 });
      return Response.json({ user: { id: "resolved-user-id", role: "member" } });
    });

    cloud = serveOnAvailable(35500, (request) => {
      if (new URL(request.url).pathname !== "/api/v1/catalog")
        return new Response("not found", { status: 404 });
      return Response.json({
        apps: [
          {
            id: "nexus-draw",
            releaseState: "released",
            surface: "user-app",
            maturity: "stable",
            reviewStatus: "approved",
            visibility: "default",
          },
        ],
        // Poison pill: if this key ever shows up in the API response,
        // the server is leaking the Cloud credential.
        _cloudApiKey: cloudApiKey,
      });
    });

    mail = serveOnAvailable(35900, (request) => {
      const headers: Record<string, string> = {};
      request.headers.forEach((value, key) => {
        headers[key.toLowerCase()] = value;
      });
      capturedMailHeaders.push(headers);
      if (headers["x-nexus-subject"]) capturedMailSubjects.push(headers["x-nexus-subject"]);
      if (new URL(request.url).pathname !== "/api/v1/folders")
        return new Response("not found", { status: 404 });
      if (headers["x-nexus-subject"] !== "resolved-user-id")
        return Response.json({ error: "wrong subject" }, { status: 403 });
      return Response.json({ folders: [] });
    });

    process.env.NEXUS_API_AUTH_URL = `http://127.0.0.1:${auth.port}`;
    process.env.NEXUS_API_CLOUD_URL = `http://127.0.0.1:${cloud.port}`;
    process.env.NEXUS_API_CLOUD_API_KEY = cloudApiKey;
    process.env.NEXUS_API_MAIL_URL = `http://127.0.0.1:${mail.port}`;

    for (let attempt = 0; attempt < 100; attempt += 1) {
      process.env.PORT = String(35700 + Math.floor(Math.random() * 200));
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

  it("rewrites x-nexus-subject to the Auth-resolved identity, ignoring the browser-supplied value", async () => {
    capturedMailSubjects.length = 0;
    capturedMailHeaders.length = 0;

    const res = await fetch(`${base}/api/v1/mail/folders`, {
      headers: {
        cookie: "nexus_session=valid",
        // Attacker-controlled header — must never reach Mail as-is
        "x-nexus-subject": "attacker-injected-subject",
      },
    });
    expect(res.status).toBe(200);

    // Mail must have been called with exactly the Auth-resolved id
    expect(capturedMailSubjects).toEqual(["resolved-user-id"]);
    // The attacker value must not have been forwarded
    expect(capturedMailSubjects).not.toContain("attacker-injected-subject");
  });

  it("does not expose the Cloud API key in the catalog response", async () => {
    const res = await fetch(`${base}/api/v1/apps`, {
      headers: { cookie: "nexus_session=valid" },
    });
    expect(res.status).toBe(200);
    const body = await res.text();

    // The key must not appear anywhere in the response body
    expect(body).not.toContain(cloudApiKey);
    // The _cloudApiKey field from Cloud's raw response must be stripped
    expect(body).not.toContain("_cloudApiKey");
  });

  it("returns 503 catalog_unavailable when Cloud is unreachable", async () => {
    // Point the API at a port nobody is listening on
    process.env.NEXUS_API_CLOUD_URL = "http://127.0.0.1:1";
    try {
      const res = await fetch(`${base}/api/v1/apps`, {
        headers: { cookie: "nexus_session=valid" },
      });
      expect(res.status).toBe(503);
      const body = (await res.json()) as { error?: string };
      expect(body.error).toBe("catalog_unavailable");
    } finally {
      process.env.NEXUS_API_CLOUD_URL = `http://127.0.0.1:${cloud.port}`;
    }
  });
});
