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

describe("nexus-api", () => {
  let base = "";
  let upstream: ReturnType<typeof Bun.serve>;
  let handle: ReturnType<typeof createApiServer>;
  const originalCloudIntegration = process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION;
  const originalPort = process.env.PORT;

  beforeAll(async () => {
    process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION = "false";
    upstream = serveOnAvailable(
      35301,
      () =>
        new Response(JSON.stringify({ from: "upstream", ok: true }), {
          headers: { "content-type": "application/json" },
        }),
    );
    for (let attempt = 0; attempt < 100; attempt += 1) {
      process.env.PORT = String(35401 + Math.floor(Math.random() * 400));
      try {
        handle = createApiServer();
        break;
      } catch (error) {
        if (!(error instanceof Error) || !/EADDRINUSE|port .* in use/i.test(error.message))
          throw error;
      }
    }
    if (!handle) throw new Error("unable to allocate the API test port");
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => {
    handle.close();
    upstream.stop();
    if (originalCloudIntegration === undefined)
      process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION = undefined;
    else process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION = originalCloudIntegration;
    if (originalPort === undefined) process.env.PORT = undefined;
    else process.env.PORT = originalPort;
  });

  it("GET /health returns 200", async () => {
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
  });

  it("add route and proxy request", async () => {
    const target = `http://127.0.0.1:${upstream.port}`;
    await fetch(`${base}/api/v1/gateway/routes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ path: "/test", method: "GET", target }),
    });

    const r = await fetch(`${base}/test`);
    expect(r.status).toBe(200);
    const body = (await r.json()) as { from: string };
    expect(body.from).toBe("upstream");
  });

  it("DELETE route removes it", async () => {
    await fetch(`${base}/api/v1/gateway/routes/GET/delete-me`, { method: "DELETE" });
    const r = await fetch(`${base}/delete-me`);
    expect(r.status).toBe(404);
  });

  it("GET /api/v1/gateway/status returns stats", async () => {
    const r = await fetch(`${base}/api/v1/gateway/status`);
    expect(r.status).toBe(200);
    const body = (await r.json()) as { routedTotal: number };
    expect(typeof body.routedTotal).toBe("number");
  });

  it("GET /api/v1/contract returns the versioned public API contract", async () => {
    const r = await fetch(`${base}/api/v1/contract`);
    expect(r.status).toBe(200);
    const body = (await r.json()) as {
      protocol: string;
      service: { id: string; auth: string };
      routes: Array<{ method: string; path: string; access: string }>;
    };
    expect(body.protocol).toBe("nexus-api/1");
    expect(body.service).toMatchObject({ id: "nexus-api", auth: "Bearer token or hosted session" });
    expect(body.routes).toEqual(
      expect.arrayContaining([
        expect.objectContaining({ method: "GET", path: "/api/v1/contract", access: "public" }),
        expect.objectContaining({
          method: "POST",
          path: "/api/v1/gateway/routes",
          access: "operator",
        }),
      ]),
    );
  });
});
