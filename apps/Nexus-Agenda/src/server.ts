import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { AgendaEngine } from "./agenda-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() },
  });
}

export function createServer() {
  const port = Number(process.env.PORT || "3116");
  const baseUrl = process.env.NEXUS_NEXUS_AGENDA_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new AgendaEngine("data/nexus-agenda.sqlite");

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health")
        return json({
          service: "nexus-agenda",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
        });

      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "nexus-agenda",
            status: "ready",
            capabilities: ["meeting-agenda", "action-items", "scheduling"],
            cloudIntegration: {
              enabled: (process.env["NEXUS_AGENDA_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
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
        return json(engine.create(b.name, b.items, b.meetings, b.tasks), 201);
      }

      const im = p.match(/^\/api\/v1\/items\/([^/]+)$/);
      if (req.method === "GET" && im) {
        const item = engine.get(im[1]!);
        return item ? json(item) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-agenda] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); server.stop(); } };
}
