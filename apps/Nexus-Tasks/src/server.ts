import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { TasksEngine } from "./tasks-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3108"); const baseUrl = process.env.NEXUS_NEXUS_TASKS_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new TasksEngine("data/tasks.sqlite")
  const phantom = new PhantomApp("nexus-tasks");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-tasks", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString(), phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-tasks", status: "ready", capabilities: ["tasks","lists","assignments"], cloudIntegration: { enabled: (process.env["NEXUS_TASKS_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/tasks") { const lid = url.searchParams.get("listId") || undefined; const pri = url.searchParams.get("priority") || undefined; return json(engine.listTasks(lid, pri), 200); }
    if (request.method === "POST" && path === "/api/v1/tasks") { const b = await request.json().catch(() => ({})) as any; if (!b.title) return json({ error: "title required" }, 400); return json(engine.addTask(b.title, b.description || "", b.listId || "", b.assignee || "", b.priority || "medium", b.dueDate || ""), 201); }
    if (request.method === "POST" && path === "/api/v1/tasks/complete") { const b = await request.json().catch(() => ({})) as any; if (!b.id) return json({ error: "id required" }, 400); engine.completeTask(b.id); return json({ ok: true }, 200); }
    if (request.method === "GET" && path === "/api/v1/tasks/lists") return json(engine.listLists(), 200);
    if (request.method === "POST" && path === "/api/v1/tasks/lists") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addList(b.name, b.description), 201); }
    const tkm = path.match(/^\/api\/v1\/tasks\/([^/]+)$/); if (request.method === "GET" && tkm) { const t = engine.getTask(tkm[1]!); return t ? json(t, 200) : json({ error: "not found" }, 404); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-tasks] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
