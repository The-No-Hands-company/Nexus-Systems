import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { DrawEngine } from "./draw-engine";
import { CollabServer } from "./collab";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

console.log("1. Starting...");
const port = 3079;
const baseUrl = `http://localhost:${port}`;
const startedAt = Date.now();
const engine = new DrawEngine("data/nexus-draw.sqlite");
console.log("2. Engine created");
const phantom = new PhantomApp("nexus-draw");
const phantomId = await phantom.start();
console.log("3. Phantom started");
const discovery = new NexusDiscovery({ cloudUrl: "http://localhost:8787", apiKey: undefined, ttlMs: 30000 });
console.log("4. Discovery created");
const collab = new (await import("./collab")).CollabServer(engine);
console.log("5. Collab created");

const server = Bun.serve({
  port,
  async fetch(req) {
    return new Response("ok", { status: 200 });
  },
  websocket: {
    open(ws) { collab.open(ws, (ws.data as any).boardId); },
    message(ws, data) { collab.message(ws, data); },
    close(ws) { collab.close(ws); },
  },
});

console.log(`Server listening on port ${server.port}`);
const stopHeartbeat = () => {};
await new Promise(() => {});
