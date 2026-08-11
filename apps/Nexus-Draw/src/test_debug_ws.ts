import { randomUUID } from "node:crypto";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { DrawEngine } from "./draw-engine";
import { CollabServer } from "./collab";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

console.log("1. Starting...");
const port = 3081;
const engine = new DrawEngine("data/nexus-draw.sqlite");
console.log("2. Engine created");
const phantom = new PhantomApp("nexus-draw");
await phantom.start();
console.log("3. Phantom started");
const discovery = new (await import("../../../packages/nexus-discovery/src/index")).NexusDiscovery({ cloudUrl: "http://localhost:8787", apiKey: undefined, ttlMs: 30000 });
console.log("4. Discovery created");
const collab = new (await import("./collab")).CollabServer(engine);
console.log("5. Collab created");

const server = Bun.serve({
  port: 3081,
  async fetch(req) {
    const url = new URL(req.url); const p = url.pathname || "";
    if (req.method === "GET" && p === "/health") return new Response(JSON.stringify({ status: "ok" }), { status: 200, headers: { "content-type": "application/json" } });
    return new Response("not found", { status: 404 });
  },
  websocket: {
    open(ws) { console.log("WS OPEN", ws.data); },
    message(ws, data) { console.log("WS MESSAGE", data.byteLength); },
    close(ws) { console.log("WS CLOSE"); },
  },
});

console.log(`Server listening on port ${server.port}`);
await new Promise(() => {});
