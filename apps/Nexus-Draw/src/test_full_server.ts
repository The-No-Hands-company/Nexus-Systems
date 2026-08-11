import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { DrawEngine } from "./draw-engine";
import { CollabServer } from "./collab";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

console.log("Creating server...");

const port = 3077;
const baseUrl = `http://localhost:${port}`;
const startedAt = Date.now();
const engine = new DrawEngine("data/nexus-draw.sqlite");
const phantom = new PhantomApp("nexus-draw");
await phantom.start();
const discovery = new NexusDiscovery({ cloudUrl: "http://localhost:8787", apiKey: undefined, ttlMs: 30000 });
const collab = new CollabServer(engine);

console.log("Creating Bun.serve...");

const server = Bun.serve({
  port,
  async fetch(req) {
    const url = new URL(req.url); const p = url.pathname || "";
    if (req.method === "GET" && p === "/health") return new Response(JSON.stringify({ status: "ok" }), { status: 200, headers: { "content-type": "application/json" } });
    return new Response("not found", { status: 404 });
  },
  websocket: {
    open(ws) { collab.open(ws, (ws.data as any).boardId); },
    message(ws, data) { collab.message(ws, data); },
    close(ws) { collab.close(ws); },
  },
});

console.log(`Server listening on port ${server.port}`);
const stopHeartbeat = () => {};
console.log("Server created, waiting...");
await new Promise(() => {}); // wait forever
