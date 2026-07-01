import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { CodeEngine } from "./code-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3126");
  const baseUrl = process.env.NEXUS_NEXUS_CODE_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new CodeEngine("data/nexus-code.sqlite");
  const phantom = new PhantomApp("code");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({
    cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
    apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined,
    ttlMs: 30000,
  });

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health")
        return json({
          service: "nexus-code",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
        },
          phantom: phantom.status() };

      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "nexus-code",
            status: "ready",
            capabilities: ["code-snippets", "repositories", "version-control"],
            cloudIntegration: {
              enabled: (process.env["NEXUS_CODE_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
          },
          200
        );

      if (req.method === "GET" && p === "/api/v1/items")
        return json(engine.list());

      if (req.method === "POST" && p === "/api/v1/items") {
        const b = await req.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.create(b.name, b.snippets, b.repos, b.files), 201);
      }

      const im = p.match(/^\/api\/v1\/items\/([^/]+)$/);
      if (req.method === "GET" && im) {
        const item = engine.get(im[1]!);
        return item ? json(item) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-code] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
