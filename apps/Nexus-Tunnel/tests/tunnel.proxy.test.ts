import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { createTunnelServer } from "../src/server";
import { clearRoutes, registerRoute, recordExposure } from "../src/state";

let tunnelHandle: ReturnType<typeof createTunnelServer> | null = null;
let upstreamServer: ReturnType<typeof Bun.serve> | null = null;

beforeEach(() => {
  clearRoutes();
  process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION = "false";
  process.env.PORT = "0";

  upstreamServer = Bun.serve({
    port: 0,
    fetch: async (request) => {
      const url = new URL(request.url);
      const body = request.method === "GET" || request.method === "HEAD" ? "" : await request.text();
      return new Response(
        JSON.stringify(
          {
            method: request.method,
            path: `${url.pathname}${url.search}`,
            body,
          },
          null,
          2,
        ),
        { headers: { "content-type": "application/json" } },
      );
    },
  });

  tunnelHandle = createTunnelServer();
});

afterEach(() => {
  delete process.env.PORT;
  delete process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION;
  tunnelHandle?.stop();
  upstreamServer?.stop();
  tunnelHandle = null;
  upstreamServer = null;
  clearRoutes();
});

describe("nexus-tunnel proxy behavior", () => {
  test("proxies internal route traffic to upstream target", async () => {
    const upstreamUrl = `http://127.0.0.1:${upstreamServer!.port}`;
    registerRoute("svc-a", upstreamUrl, "internal");

    const response = await fetch(`http://127.0.0.1:${tunnelHandle!.server.port}/t/svc-a/api/health?source=tunnel`);
    expect(response.status).toBe(200);

    const payload = await response.json();
    expect(payload.method).toBe("GET");
    expect(payload.path).toBe("/api/health?source=tunnel");
  });

  test("rejects public proxy traffic without Guardian-approved exposure", async () => {
    const upstreamUrl = `http://127.0.0.1:${upstreamServer!.port}`;
    registerRoute("svc-public", upstreamUrl, "public");

    const response = await fetch(`http://127.0.0.1:${tunnelHandle!.server.port}/t/svc-public`);
    expect(response.status).toBe(403);

    const payload = await response.json();
    expect(payload.error).toContain("Guardian approval");
  });

  test("allows public proxy traffic after Guardian-approved exposure", async () => {
    const upstreamUrl = `http://127.0.0.1:${upstreamServer!.port}`;
    registerRoute("svc-public", upstreamUrl, "public");
    recordExposure("svc-public", "public", true, "approved for production traffic");

    const response = await fetch(`http://127.0.0.1:${tunnelHandle!.server.port}/t/svc-public/v1/ping`);
    expect(response.status).toBe(200);

    const payload = await response.json();
    expect(payload.path).toBe("/v1/ping");
  });

  test("returns 503 when route is disabled", async () => {
    const upstreamUrl = `http://127.0.0.1:${upstreamServer!.port}`;
    registerRoute("svc-disabled", upstreamUrl, "internal");

    const disableRes = await fetch(
      `http://127.0.0.1:${tunnelHandle!.server.port}/api/v1/tunnel/routes/svc-disabled/disable`,
      { method: "POST" },
    );
    expect(disableRes.status).toBe(200);

    const response = await fetch(`http://127.0.0.1:${tunnelHandle!.server.port}/t/svc-disabled`);
    expect(response.status).toBe(503);

    const payload = await response.json();
    expect(payload.error).toContain("disabled");
  });
});
