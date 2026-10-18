import { afterAll, beforeAll, describe, expect, it } from "bun:test";
import { createApiServer } from "../src/server";

describe("nexus-api", () => {
  let base = "";
  let upstream: ReturnType<typeof Bun.serve>;
  let handle: ReturnType<typeof createApiServer>;
  const originalCloudIntegration = process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION;

  beforeAll(async () => {
    process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION = "false";
    upstream = Bun.serve({ port: 0, fetch: () => new Response(JSON.stringify({ from: "upstream", ok: true }), { headers: { "content-type": "application/json" } }) });
    handle = createApiServer();
    await new Promise((r) => setTimeout(r, 200));
    base = `http://127.0.0.1:${handle.server.port}`;
  });
  afterAll(() => {
    handle.close();
    upstream.stop();
    if (originalCloudIntegration === undefined) delete process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION;
    else process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION = originalCloudIntegration;
  });

  it("GET /health returns 200", async () => {
    const r = await fetch(`${base}/health`);
    expect(r.status).toBe(200);
  });

  it("add route and proxy request", async () => {
    const target = `http://127.0.0.1:${upstream.port}`;
    await fetch(`${base}/api/v1/gateway/routes`, {
      method: "POST", headers: { "content-type": "application/json" },
      body: JSON.stringify({ path: "/test", method: "GET", target }),
    });

    const r = await fetch(`${base}/test`);
    expect(r.status).toBe(200);
    const body = await r.json() as { from: string };
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
    const body = await r.json() as { routedTotal: number };
    expect(typeof body.routedTotal).toBe("number");
  });

  it("GET /api/v1/contract returns the versioned public API contract", async () => {
    const r = await fetch(`${base}/api/v1/contract`);
    expect(r.status).toBe(200);
    const body = await r.json() as {
      protocol: string;
      service: { id: string; auth: string };
      routes: Array<{ method: string; path: string; access: string }>;
    };
    expect(body.protocol).toBe("nexus-api/1");
    expect(body.service).toMatchObject({ id: "nexus-api", auth: "Bearer token or hosted session" });
    expect(body.routes).toEqual(expect.arrayContaining([
      expect.objectContaining({ method: "GET", path: "/api/v1/contract", access: "public" }),
      expect.objectContaining({ method: "POST", path: "/api/v1/gateway/routes", access: "operator" }),
    ]));
  });
});
