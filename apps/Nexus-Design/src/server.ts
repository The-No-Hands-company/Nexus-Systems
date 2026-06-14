import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { DesignEngine } from "./design-engine";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), { status: s, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() } });
}

export function createServer() {
  const port = Number(process.env.PORT || "3074");
  const baseUrl = process.env.NEXUS_NEXUS_DESIGN_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new DesignEngine("data/nexus-design.sqlite");
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url); const p = url.pathname || "";
      if (req.method === "GET" && p === "/health") return json({ service: "nexus-design", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) });
      if (req.method === "GET" && p === "/api/v1/status") return json({ service: "nexus-design", status: "ready", capabilities: ["design", "prototyping", "collaboration"], cloudIntegration: { enabled: (process.env["NEXUS_DESIGN_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" } }, 200);
      if (req.method === "GET" && p === "/api/v1/design/projects") return json(engine.listProjects());
if (req.method === "POST" && p === "/api/v1/design/projects") { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.createProject(b.name, b.type || "web"), 201); }
if (req.method === "POST" && p === "/api/v1/design/expose") { const b = await req.json().catch(()=>({})) as any; if (!b.projectId) return json({ error: "projectId required" }, 400); const cloud = await discovery.resolve("nexus-cloud"); const cloudUrl = cloud?.url || (process.env.NEXUS_CLOUD_URL || "http://localhost:8787"); const h: Record<string, string> = { "content-type": "application/json", accept: "application/json" }; if (process.env.NEXUS_CLOUD_API_KEY) h["x-api-key"] = process.env.NEXUS_CLOUD_API_KEY; let lastErr: Error | null = null; for (let i = 0; i < 3; i++) { try { const r = await fetch(`${cloudUrl}/api/v1/public-url`, { method: "POST", headers: h, body: JSON.stringify({ projectId: b.projectId }) }); if (!r.ok) { const t = await r.text().catch(() => ""); throw new Error(`HTTP ${r.status}: ${t}`); } const d = await r.json() as any; return json({ projectId: b.projectId, publicUrl: d.url || d.publicUrl, publishedAt: new Date().toISOString() }, 201); } catch (e) { lastErr = e as Error; if (i < 2) await new Promise(resolve => setTimeout(resolve, 500 * (i + 1))); } } return json({ error: "public url publish failed after 3 retries", detail: lastErr?.message }, 502); }
const dp = p.match(/^\/api\/v1\/design\/projects\/([^/]+)$/); if (req.method === "GET" && dp) { const pr = engine.getProject(dp[1]!); return pr ? json(pr) : json({ error: "not found" }, 404); }
if (req.method === "POST" && dp && p.endsWith("/frames")) { const b = await req.json().catch(()=>({})) as any; if (!b.name) return json({ error: "name required" }, 400); engine.addFrame(dp[1]!, b.name); return json({ added: true }); }
      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-design] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); server.stop(); } };
}
