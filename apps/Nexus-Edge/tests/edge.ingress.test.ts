import { afterEach, beforeEach, describe, expect, test } from "bun:test";
import { createEdgeServer } from "../src/server";
import { clearThreats } from "../src/state";

let edgeHandle: ReturnType<typeof createEdgeServer> | null = null;
let upstreamServer: ReturnType<typeof Bun.serve> | null = null;

beforeEach(() => {
  clearThreats();
  process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION = "false";
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

  edgeHandle = createEdgeServer();
});

afterEach(() => {
  edgeHandle?.stop();
  upstreamServer?.stop();
  edgeHandle = null;
  upstreamServer = null;
  clearThreats();
  delete process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION;
  delete process.env.PORT;
});

describe("nexus-edge ingress routing", () => {
  test("routes host-header traffic to configured upstream", async () => {
    const edgePort = edgeHandle!.server.port;
    const upstreamUrl = `http://127.0.0.1:${upstreamServer!.port}`;

    const createRoute = await fetch(`http://127.0.0.1:${edgePort}/api/v1/edge/routes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        host: "app.nexus.local",
        upstreamUrl,
        targetType: "direct",
        trustState: "trusted",
      }),
    });
    expect(createRoute.status).toBe(201);

    const proxyResponse = await fetch(`http://127.0.0.1:${edgePort}/v1/ping?x=1`, {
      headers: {
        "x-forwarded-host": "app.nexus.local",
      },
    });

    expect(proxyResponse.status).toBe(200);
    const payload = await proxyResponse.json();
    expect(payload.path).toBe("/v1/ping?x=1");
  });

  test("blocks ingress when route trust state is quarantined", async () => {
    const edgePort = edgeHandle!.server.port;
    const upstreamUrl = `http://127.0.0.1:${upstreamServer!.port}`;

    await fetch(`http://127.0.0.1:${edgePort}/api/v1/edge/routes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        host: "blocked.nexus.local",
        upstreamUrl,
        trustState: "quarantined",
      }),
    });

    const response = await fetch(`http://127.0.0.1:${edgePort}/healthz`, {
      headers: {
        "x-forwarded-host": "blocked.nexus.local",
      },
    });

    expect(response.status).toBe(403);
    const payload = await response.json();
    expect(payload.error).toContain("trust state");
  });

  test("returns 503 when ingress route is disabled", async () => {
    const edgePort = edgeHandle!.server.port;
    const upstreamUrl = `http://127.0.0.1:${upstreamServer!.port}`;

    await fetch(`http://127.0.0.1:${edgePort}/api/v1/edge/routes`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({
        host: "disabled.nexus.local",
        upstreamUrl,
        trustState: "trusted",
      }),
    });

    const disable = await fetch(
      `http://127.0.0.1:${edgePort}/api/v1/edge/routes/${encodeURIComponent("disabled.nexus.local")}/disable`,
      { method: "POST" },
    );
    expect(disable.status).toBe(200);

    const response = await fetch(`http://127.0.0.1:${edgePort}/v1/ping`, {
      headers: {
        "x-forwarded-host": "disabled.nexus.local",
      },
    });

    expect(response.status).toBe(503);
    const payload = await response.json();
    expect(payload.error).toContain("disabled");
  });
});
