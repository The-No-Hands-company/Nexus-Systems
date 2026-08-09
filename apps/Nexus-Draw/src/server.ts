import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { DrawEngine } from "./draw-engine";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3075");
  const baseUrl = process.env.NEXUS_NEXUS_DRAW_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new DrawEngine("data/nexus-draw.sqlite")
  const phantom = new PhantomApp("nexus-draw");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-draw", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), phantom: phantom.status() });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-draw", status: "ready", capabilities: ["whiteboard", "diagramming", "collaboration"], cloudIntegration: { enabled: (process.env["NEXUS_DRAW_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      if (req.method === "GET" && p === "/api/v1/draw/boards") return json(engine.listBoards());
if (req.method === "POST" && p === "/api/v1/draw/boards") { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.createBoard(b.name), 201); }
const dm = p.match(/^\/api\/v1\/draw\/boards\/([^/]+)$/); if (req.method === "GET" && dm) { const brd = engine.getBoard(dm[1]!); return brd ? json(brd) : json({ error: "not found" }, 404); }
if (req.method === "PUT" && dm && p.endsWith("/elements")) { const b = await req.json().catch(()=>({})) as any; engine.updateElements(dm[1]!, Array.isArray(b.elements) ? b.elements : []); return json({ updated: true }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-draw] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
