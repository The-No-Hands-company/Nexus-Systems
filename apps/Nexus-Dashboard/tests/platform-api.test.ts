import { afterAll, beforeAll, describe, expect, it } from "bun:test";

const originalEnv = new Map(
  [
    "NEXUS_API_INTERNAL_URL",
    "NEXUS_AUTH_INTERNAL_URL",
    "NEXUS_CLOUD_URL",
    "NEXUS_EMAIL_URL",
    "NEXUS_TERMINAL_URL",
  ].map((name) => [name, process.env[name]]),
);
process.env.NEXUS_API_INTERNAL_URL = "http://127.0.0.1:35513";
process.env.NEXUS_AUTH_INTERNAL_URL = "http://127.0.0.1:35511";
process.env.NEXUS_CLOUD_URL = "http://127.0.0.1:35512";
process.env.NEXUS_EMAIL_URL = "http://127.0.0.1:35514";
process.env.NEXUS_TERMINAL_URL = "http://127.0.0.1:35515";

const { handleRequest } = await import("../src/server");

function serveOnAvailable(
  start: number,
  fetch: (request: Request) => Response | Promise<Response>,
): ReturnType<typeof Bun.serve> {
  for (let attempt = 0; attempt < 100; attempt += 1) {
    const port = start + Math.floor(Math.random() * 400);
    try {
      return Bun.serve({ hostname: "127.0.0.1", port, fetch });
    } catch (error) {
      if (!(error instanceof Error) || !/EADDRINUSE|port .* in use/i.test(error.message)) {
        throw error;
      }
    }
  }
  throw new Error("unable to allocate a Dashboard integration port");
}

describe("Dashboard platform API integration", () => {
  let auth: ReturnType<typeof Bun.serve> | undefined;
  let cloud: ReturnType<typeof Bun.serve> | undefined;
  let api: ReturnType<typeof Bun.serve> | undefined;
  let mail: ReturnType<typeof Bun.serve> | undefined;
  let terminal: ReturnType<typeof Bun.serve> | undefined;
  const calls: string[] = [];

  beforeAll(() => {
    auth = serveOnAvailable(35511, (request) => {
      calls.push(`auth:${new URL(request.url).pathname}`);
      return Response.json({ user: { id: "user-1", role: "member" } });
    });
    cloud = serveOnAvailable(35512, (request) => {
      calls.push(`cloud:${new URL(request.url).pathname}`);
      return Response.json({ apps: [] });
    });
    api = serveOnAvailable(35513, (request) => {
      const path = new URL(request.url).pathname;
      calls.push(`api:${path}`);
      if (path === "/api/v1/me") return Response.json({ user: { id: "user-1", role: "member" } });
      if (path === "/api/v1/apps") {
        return Response.json({
          apps: [
            {
              id: "nexus-mail",
              name: "Nexus Mail",
              publicUrl: "https://mail.tnhc.dev",
              health: "healthy",
              surface: "user-app",
              releaseState: "released",
              primaryShelf: "everyday",
              secondaryShelves: [],
              audiences: ["general"],
              capabilities: ["email"],
              maturity: "stable",
              visibility: "default",
              reviewStatus: "approved",
            },
          ],
        });
      }
      return Response.json({ error: "not found" }, { status: 404 });
    });
    mail = serveOnAvailable(35514, () => Response.json({ status: "ok" }));
    terminal = serveOnAvailable(35515, () => Response.json({ status: "ok" }));
    process.env.NEXUS_API_INTERNAL_URL = `http://127.0.0.1:${api.port}`;
    process.env.NEXUS_AUTH_INTERNAL_URL = `http://127.0.0.1:${auth.port}`;
    process.env.NEXUS_CLOUD_URL = `http://127.0.0.1:${cloud.port}`;
    process.env.NEXUS_EMAIL_URL = `http://127.0.0.1:${mail.port}`;
    process.env.NEXUS_TERMINAL_URL = `http://127.0.0.1:${terminal.port}`;
  });

  afterAll(() => {
    auth?.stop(true);
    cloud?.stop(true);
    api?.stop(true);
    mail?.stop(true);
    terminal?.stop(true);
    for (const [name, value] of originalEnv) {
      if (value === undefined) Reflect.deleteProperty(process.env, name);
      else process.env[name] = value;
    }
  });

  it("uses Nexus API for identity and catalog instead of direct Cloud calls", async () => {
    const response = await handleRequest(
      new Request("http://app.test/ipa/apps", {
        headers: { cookie: "nexus_session=valid" },
      }),
    );
    expect(response.status).toBe(200);
    const body = (await response.json()) as { apps: Array<{ id: string }> };
    expect(body.apps.map((app) => app.id)).toEqual(["nexus-email"]);
    expect(calls).toContain("api:/api/v1/me");
    expect(calls).toContain("api:/api/v1/apps");
    expect(calls.some((call) => call.startsWith("cloud:"))).toBe(false);
  });
});
