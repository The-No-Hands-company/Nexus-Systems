import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { DrawEngine } from "./draw-engine";
import { CollabServer } from "./collab";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
      // Draw sent no framing headers at all, so any site on the internet could frame
      // it — a clickjacking exposure. Naming the shell explicitly closes that while
      // enabling the embed.
      "content-security-policy": "frame-ancestors 'self' https://app.tnhc.dev",
    },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3075");
  const baseUrl = process.env.NEXUS_NEXUS_DRAW_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new DrawEngine("data/nexus-draw.sqlite");
  const phantom = new PhantomApp("nexus-draw");
  await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
  const collab = new CollabServer(engine);

  const server = Bun.serve({
    port,
    // Loopback unless told otherwise — Bun.serve binds every interface by
    // default, and the proxy is the only intended client.
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      
      // Handle WebSocket upgrade for collab
      const wsMatch = p.match(/^\/api\/v1\/draw\/ws\/([^/]+)$/);
      if (wsMatch && req.method === "GET" && req.headers.get("upgrade") === "websocket") {
        if (collab.upgrade(req, server, wsMatch[1]!)) {
          return undefined; // upgraded
        }
        return new Response("Upgrade failed", { status: 400 });
      }
      
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-draw", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), phantom: phantom.status() });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-draw", status: "ready", capabilities: ["whiteboard", "diagramming", "collaboration"], cloudIntegration: { enabled: (process.env["NEXUS_DRAW_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      if (req.method === "GET" && p === "/api/v1/draw/boards") return json(engine.listBoards());
      if (req.method === "POST" && p === "/api/v1/draw/boards") { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.createBoard(b.name), 201); }
      const dm = p.match(/^\/api\/v1\/draw\/boards\/([^/]+)$/);
      if (req.method === "GET" && dm) { const brd = engine.getBoard(dm[1]!); return brd ? json(brd) : json({ error: "not found" }, 404); }
      if (req.method === "PATCH" && dm) { const body = (await req.json().catch(()=>({}))) as Record<string, unknown>; const id = dm[1]!; const patch: Record<string, unknown> = {}; for (const key of ["name", "description", "width", "height", "background", "isPublic", "defaultStyleMode", "gridSnap"] as const) { if (body[key] !== undefined) patch[key] = body[key]; } return engine.updateBoardMeta(id, patch) ? json({ updated: true }) : json({ error: "not found" }, 404); }
      if (req.method === "DELETE" && dm) { return engine.deleteBoard(dm[1]!) ? json({ deleted: true }) : json({ error: "not found" }, 404); }
      if (req.method === "PUT" && dm && p.endsWith("/elements")) { const b = await req.json().catch(()=>({})) as any; engine.updateElements(dm[1]!, Array.isArray(b.elements) ? b.elements : []); return json({ updated: true }); }
      
      // AI generate endpoint (sync, deterministic)
      if (req.method === "POST" && p === "/api/v1/draw/ai/generate") {
        const body = (await req.json().catch(()=>({}))) as { prompt?: string; board_id?: string };
        if (!body.prompt || body.prompt.length === 0) return json({ error: "prompt is required" }, 400);
        const { synthesizeDiagram } = await import("./ai");
        const elements = synthesizeDiagram(body.prompt);
        if (body.board_id) {
          const board = engine.getBoard(body.board_id);
          if (board) engine.updateElements(body.board_id, [...board.elements, ...elements]);
        }
        return json({ elements, board_id: body.board_id });
      }
      
      return json({ error: "not found" }, 404);
    },
    websocket: {
      open(ws) { collab.open(ws, (ws.data as any).boardId); },
      // yjs speaks binary only. Bun delivers binary frames as Buffer (a
      // Uint8Array subclass, so it passes straight through) and text frames as
      // string — drop those rather than feeding one to the sync decoder.
      message(ws, data) { if (typeof data !== "string") collab.message(ws, data); },
      close(ws) { collab.close(ws); },
    },
  });

  console.log(`[nexus-draw] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
