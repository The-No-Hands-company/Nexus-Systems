import { randomUUID } from "node:crypto"; import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index"; import { VideoEngine } from "./video-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response { const body = JSON.stringify(payload); return new Response(body, { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers } }); }
export async function createServer() { const port = Number(process.env.PORT || "3113"); const baseUrl = process.env.NEXUS_NEXUS_VIDEO_BASE_URL || `http://localhost:${port}`; const startedAt = Date.now(); const engine = new VideoEngine("data/video.sqlite")
  const phantom = new PhantomApp("nexus-video");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  const server = Bun.serve({ port, async fetch(request) { const url = new URL(request.url); const path = url.pathname || "";
    if (request.method === "GET" && path === "/health") { return json({ service: "nexus-video", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString(), phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/status") { return json({ service: "nexus-video", status: "ready", capabilities: ["video","channels","playlists","streaming"], cloudIntegration: { enabled: (process.env["NEXUS_VIDEO_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200); }
    if (request.method === "GET" && path === "/api/v1/video/videos") { const ch = url.searchParams.get("channelId") || undefined; return json(engine.listVideos(ch), 200); }
    if (request.method === "POST" && path === "/api/v1/video/videos") { const b = await request.json().catch(() => ({})) as any; if (!b.title || !b.url || !b.channelId) return json({ error: "title, url, and channelId required" }, 400); return json(engine.addVideo(b.title, b.description || "", b.url, b.duration || 0, b.channelId, b.tags || ""), 201); }
    const vm = path.match(/^\/api\/v1\/video\/videos\/([^/]+)$/); if (request.method === "GET" && vm) { const v = engine.getVideo(vm[1]!); return v ? json(v, 200) : json({ error: "not found" }, 404); }
    if (request.method === "GET" && path === "/api/v1/video/channels") return json(engine.listChannels(), 200);
    if (request.method === "POST" && path === "/api/v1/video/channels") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addChannel(b.name, b.description), 201); }
    if (request.method === "GET" && path === "/api/v1/video/playlists") return json(engine.listPlaylists(), 200);
    if (request.method === "POST" && path === "/api/v1/video/playlists") { const b = await request.json().catch(() => ({})) as any; if (!b.name) return json({ error: "name required" }, 400); return json(engine.addPlaylist(b.name, b.description), 201); }
    return json({ error: "not found" }, 404); } });
  console.log(`[nexus-video] Listening on port ${server.port}`); const stopHeartbeat = startHeartbeat(baseUrl); return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } }; }
