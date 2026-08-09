import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { NotesEngine } from "./notes-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3093"); const baseUrl = process.env.NEXUS_NEXUS_NOTES_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new NotesEngine("data/notes.sqlite")
  const phantom = new PhantomApp("nexus-notes");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-notes", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString(), phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-notes", status: "ready", capabilities: ["notes","notebooks","knowledge-base"], cloudIntegration: { enabled: (process.env["NEXUS_NOTES_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/notes") { const nb = url.searchParams.get("notebook") || undefined; return json(engine.listNotes(nb), 200); }
    if (request.method === "POST" && path === "/api/v1/notes") { const b = await request.json().catch(() => ({})) as any; if (!b.title) return json({ error: "title required" }, 400); return json(engine.addNote(b.title, b.content || "", b.notebook || "General", b.tags || ""), 201); }
    if (request.method === "GET" && path === "/api/v1/notes/search") { const q = url.searchParams.get("q"); if (!q) return json({ error: "q required" }, 400); return json(engine.searchNotes(q), 200); }
    if (request.method === "GET" && path === "/api/v1/notes/notebooks") return json(engine.listNotebooks(), 200);
    if (request.method === "POST" && path === "/api/v1/notes/notebooks") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addNotebook(b.name, b.description), 201); }
    const nm = path.match(/^\/api\/v1\/notes\/([^/]+)$/); if (request.method === "GET" && nm) { const n = engine.getNote(nm[1]!); return n ? json(n, 200) : json({ error: "not found" }, 404); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-notes] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
