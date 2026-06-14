import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { BroadcastEngine } from "./broadcast-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "3120");
  const baseUrl = process.env.NEXUS_NEXUS_BROADCAST_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new BroadcastEngine("data/nexus-broadcast.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health")
        return json({
          service: "nexus-broadcast",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
        });

      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "nexus-broadcast",
            status: "ready",
            capabilities: ["live-broadcasting", "streaming", "media"],
            cloudIntegration: {
              enabled: (process.env["NEXUS_BROADCAST_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
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
        return json(engine.create(b.name, b.sessions, b.streams, b.recordings), 201);
      }

      const im = p.match(/^\/api\/v1\/items\/([^/]+)$/);
      if (req.method === "GET" && im) {
        const item = engine.get(im[1]!);
        return item ? json(item) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-broadcast] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); server.stop(); } };
}
