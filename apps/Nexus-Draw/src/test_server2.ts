import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { randomUUID } from "node:crypto";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

const phantom = new PhantomApp("nexus-draw");
await phantom.start();
const discovery = new NexusDiscovery({ cloudUrl: "http://localhost:8787", apiKey: undefined, ttlMs: 30000 });

const server = Bun.serve({
  port: 3076,
  async fetch(req) {
    const url = new URL(req.url); const p = url.pathname || "";
    if (req.method === "GET" && p === "/health") return new Response(JSON.stringify({ status: "ok" }), { status: 200, headers: { "content-type": "application/json" } });
    return new Response("not found", { status: 404 });
  },
});

console.log(`Server listening on port ${server.port}`);
