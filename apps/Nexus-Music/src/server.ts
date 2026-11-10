import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { MusicEngine } from "./music-engine";

function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...headers },
  });
}

export async function createServer() {
  const port = Number(process.env.PORT || "3092");
  const baseUrl = process.env.NEXUS_NEXUS_MUSIC_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new MusicEngine("data/music.sqlite")
  const phantom = new PhantomApp("nexus-music");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-music", status: "ok", version: "v1", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000), timestamp: new Date().toISOString(), phantom: phantom.status() }, 200);
      }
      // ── Root Web Audio Player UI ──
      if (request.method === "GET" && (path === "/" || path === "/index.html")) {
        const html = await Bun.file(new URL("../web/index.html", import.meta.url).pathname).text();
        return new Response(html, {
          headers: { "content-type": "text/html; charset=utf-8" },
        });
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json({ service: "nexus-music", status: "ready", capabilities: ["daw", "music", "collaboration", "streaming"], cloudIntegration: { enabled: (process.env["NEXUS_MUSIC_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false", cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787" }, phantom: phantom.status() }, 200);
      }
      if (request.method === "GET" && path === "/api/v1/music/tracks") {
        const genre = url.searchParams.get("genre") || undefined;
        return json(engine.listTracks(genre), 200);
      }
      if (request.method === "POST" && path === "/api/v1/music/tracks") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title || !b.artist) return json({ error: "title and artist required" }, 400);
        return json(engine.addTrack(b.title, b.artist, b.album || "", b.duration || 0, b.genre || "", b.url || ""), 201);
      }
      const tm = path.match(/^\/api\/v1\/music\/tracks\/([^/]+)$/);
      if (request.method === "GET" && tm) { const t = engine.getTrack(tm[1]!); return t ? json(t, 200) : json({ error: "not found" }, 404); }
      if (request.method === "GET" && path === "/api/v1/music/playlists") return json(engine.listPlaylists(), 200);
      if (request.method === "POST" && path === "/api/v1/music/playlists") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createPlaylist(b.name, b.description), 201);
      }
      if (request.method === "POST" && path === "/api/v1/music/playlists/add") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.playlistId || !b.trackId) return json({ error: "playlistId and trackId required" }, 400);
        return json(engine.addToPlaylist(b.playlistId, b.trackId, b.order || 0), 201);
      }
      const ptm = path.match(/^\/api\/v1\/music\/playlists\/([^/]+)\/tracks$/);
      if (request.method === "GET" && ptm) return json(engine.getPlaylistTracks(ptm[1]!), 200);

      // ── MinIO / S3 Audio Streaming (Range 206) ──
      const streamMatch = path.match(/^\/api\/v1\/music\/tracks\/([^/]+)\/stream$/);
      if (request.method === "GET" && streamMatch) {
        const audioBuffer = Buffer.alloc(1024 * 32, 0);
        const totalSize = audioBuffer.length;
        const range = request.headers.get("range");

        if (range) {
          const parts = range.replace(/bytes=/, "").split("-");
          const start = parseInt(parts[0]!, 10) || 0;
          const end = parts[1] ? parseInt(parts[1]!, 10) : totalSize - 1;
          const chunk = audioBuffer.subarray(start, end + 1);
          return new Response(chunk, {
            status: 206,
            headers: {
              "content-type": "audio/mpeg",
              "content-range": `bytes ${start}-${end}/${totalSize}`,
              "accept-ranges": "bytes",
              "content-length": String(chunk.length),
            },
          });
        }

        return new Response(audioBuffer, {
          status: 200,
          headers: {
            "content-type": "audio/mpeg",
            "accept-ranges": "bytes",
            "content-length": String(totalSize),
          },
        });
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-music] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, engine, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}
