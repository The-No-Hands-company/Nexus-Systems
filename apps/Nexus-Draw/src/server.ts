import { randomUUID } from "node:crypto";
import type { ServerWebSocket } from "bun";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { startHeartbeat } from "./cloud";
import { CollabServer } from "./collab";
import { DrawEngine } from "./draw-engine";

type SocketData = { boardId: string };
type JsonObject = Record<string, unknown>;

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
      "content-security-policy": "frame-ancestors 'self' https://app.tnhc.dev",
    },
  });
}

function getSubject(req: Request, url?: URL): string | null {
  // Header takes precedence (set by the proxy for all proxied requests).
  // Query param is the fallback for WebSocket upgrades: the browser WS API
  // cannot send custom headers, so the proxy passes the subject as ?subject=
  // before forwarding the upgrade. Direct connections use the same param in tests.
  return req.headers.get("x-nexus-subject") || url?.searchParams.get("subject") || null;
}

export async function createServer() {
  const port = Number(process.env.PORT || "3075");
  const baseUrl = process.env.NEXUS_NEXUS_DRAW_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const dbPath = process.env.NEXUS_DRAW_DB || "data/nexus-draw.sqlite";
  const engine = new DrawEngine(dbPath);
  const phantom = new PhantomApp("nexus-draw");
  await phantom.start();
  const discovery = new NexusDiscovery({
    cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
    apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined,
    ttlMs: 30000,
  });
  const collab = new CollabServer(engine);

  const server = Bun.serve<SocketData>({
    port,
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      // WebSocket upgrade for collab — auth-gated
      const wsMatch = p.match(/^\/api\/v1\/draw\/ws\/([^/]+)$/);
      if (wsMatch && req.method === "GET" && req.headers.get("upgrade") === "websocket") {
        const subject = getSubject(req, url);
        if (!subject) return new Response("Unauthorized", { status: 401 });
        const boardId = wsMatch[1];
        if (!boardId) return new Response("Not Found", { status: 404 });
        const brd = engine.getBoard(boardId);
        if (!brd) return new Response("Not Found", { status: 404 });
        if (brd.ownerSubject !== subject && !brd.isPublic)
          return new Response("Forbidden", { status: 403 });
        if (collab.upgrade(req, server, boardId)) return undefined;
        return new Response("Upgrade failed", { status: 400 });
      }

      if (req.method === "GET" && p === "/health")
        return json({
          service: "nexus-draw",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          phantom: phantom.status(),
        });
      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "nexus-draw",
            status: "ready",
            capabilities: ["whiteboard", "diagramming", "collaboration"],
            cloudIntegration: {
              enabled: (process.env.NEXUS_DRAW_ENABLE_CLOUD_INTEGRATION || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
            phantom: phantom.status(),
          },
          200,
        );

      if (req.method === "GET" && p === "/api/v1/draw/boards") {
        const subject = getSubject(req);
        if (!subject) return json({ error: "unauthorized" }, 401);
        return json(engine.listBoards(subject));
      }

      if (req.method === "POST" && p === "/api/v1/draw/boards") {
        const subject = getSubject(req);
        if (!subject) return json({ error: "unauthorized" }, 401);
        const body = (await req.json().catch(() => ({}))) as JsonObject;
        const name = typeof body.name === "string" ? body.name : "";
        if (!name) return json({ error: "name required" }, 400);
        return json(engine.createBoard(name, subject), 201);
      }

      const dm = p.match(/^\/api\/v1\/draw\/boards\/([^/]+)$/);

      if (req.method === "GET" && dm) {
        const subject = getSubject(req);
        if (!subject) return json({ error: "unauthorized" }, 401);
        const boardId = dm[1];
        if (!boardId) return json({ error: "not found" }, 404);
        const brd = engine.getBoard(boardId);
        if (!brd) return json({ error: "not found" }, 404);
        if (brd.ownerSubject !== subject && !brd.isPublic) return json({ error: "forbidden" }, 403);
        return json(brd);
      }

      if (req.method === "PATCH" && dm) {
        const subject = getSubject(req);
        if (!subject) return json({ error: "unauthorized" }, 401);
        const body = (await req.json().catch(() => ({}))) as Record<string, unknown>;
        const id = dm[1];
        if (!id) return json({ error: "not found" }, 404);
        const patch: Record<string, unknown> = {};
        for (const key of [
          "name",
          "description",
          "width",
          "height",
          "background",
          "isPublic",
          "defaultStyleMode",
          "gridSnap",
        ] as const) {
          if (body[key] !== undefined) patch[key] = body[key];
        }
        return engine.updateBoardMeta(id, subject, patch)
          ? json({ updated: true })
          : json({ error: "not found" }, 404);
      }

      if (req.method === "DELETE" && dm) {
        const subject = getSubject(req);
        if (!subject) return json({ error: "unauthorized" }, 401);
        const boardId = dm[1];
        if (!boardId) return json({ error: "not found" }, 404);
        return engine.deleteBoard(boardId, subject)
          ? json({ deleted: true })
          : json({ error: "not found" }, 404);
      }

      const emMatch = p.match(/^\/api\/v1\/draw\/boards\/([^/]+)\/elements$/);
      if (req.method === "PUT" && emMatch) {
        const subject = getSubject(req);
        if (!subject) return json({ error: "unauthorized" }, 401);
        const boardId = emMatch[1];
        if (!boardId) return json({ error: "not found" }, 404);
        const brd = engine.getBoard(boardId);
        if (!brd) return json({ error: "not found" }, 404);
        if (brd.ownerSubject !== subject && !brd.isPublic) return json({ error: "forbidden" }, 403);
        const body = (await req.json().catch(() => ({}))) as JsonObject;
        engine.updateElements(boardId, Array.isArray(body.elements) ? body.elements : []);
        return json({ updated: true });
      }

      // AI generate endpoint
      if (req.method === "POST" && p === "/api/v1/draw/ai/generate") {
        const subject = getSubject(req);
        if (!subject) return json({ error: "unauthorized" }, 401);
        const body = (await req.json().catch(() => ({}))) as { prompt?: string; board_id?: string };
        if (!body.prompt || body.prompt.length === 0)
          return json({ error: "prompt is required" }, 400);
        const { synthesizeDiagram } = await import("./ai");
        const elements = synthesizeDiagram(body.prompt);
        if (body.board_id) {
          const board = engine.getBoard(body.board_id);
          // Only append if subject owns the board or it's public
          if (board && (board.ownerSubject === subject || board.isPublic)) {
            engine.updateElements(body.board_id, [...board.elements, ...elements]);
          }
        }
        return json({ elements, board_id: body.board_id });
      }

      return json({ error: "not found" }, 404);
    },
    websocket: {
      open(ws: ServerWebSocket<SocketData>) {
        collab.open(ws, ws.data.boardId);
      },
      message(ws, data) {
        if (typeof data !== "string") collab.message(ws, data);
      },
      close(ws) {
        collab.close(ws);
      },
    },
  });

  console.log(`[nexus-draw] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    close: () => {
      stopHeartbeat();
      phantom.stop();
      server.stop();
    },
  };
}
