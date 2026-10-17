import { randomUUID } from "node:crypto";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { startHeartbeat } from "./cloud";
import { VideoEngine } from "./video-engine";
function json(payload: unknown, status: number, headers?: Record<string, string>): Response {
  const body = JSON.stringify(payload);
  return new Response(body, {
    status,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
      ...headers,
    },
  });
}

type JsonObject = Record<string, unknown>;

function object(value: unknown): JsonObject {
  return value !== null && typeof value === "object" && !Array.isArray(value)
    ? (value as JsonObject)
    : {};
}

function stringField(body: JsonObject, key: string): string | undefined {
  return typeof body[key] === "string" ? body[key] : undefined;
}

function numberField(body: JsonObject, key: string): number | undefined {
  return typeof body[key] === "number" ? body[key] : undefined;
}
export async function createServer() {
  const port = Number(process.env.PORT || "3113");
  const baseUrl = process.env.NEXUS_NEXUS_VIDEO_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new VideoEngine("data/video.sqlite");
  const phantom = new PhantomApp("nexus-video");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({
    cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
    apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined,
    ttlMs: 30000,
  });
  const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";
      if (request.method === "GET" && path === "/health") {
        return json(
          {
            service: "nexus-video",
            status: "ok",
            version: "v1",
            uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
            timestamp: new Date().toISOString(),
            phantom: phantom.status(),
          },
          200,
        );
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json(
          {
            service: "nexus-video",
            status: "ready",
            capabilities: ["video", "channels", "playlists", "streaming"],
            cloudIntegration: {
              enabled: (process.env.NEXUS_VIDEO_ENABLE_CLOUD_INTEGRATION || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
            phantom: phantom.status(),
          },
          200,
        );
      }
      if (request.method === "GET" && path === "/api/v1/video/videos") {
        const ch = url.searchParams.get("channelId") || undefined;
        return json(engine.listVideos(ch), 200);
      }
      if (request.method === "POST" && path === "/api/v1/video/videos") {
        const b = object(await request.json().catch(() => ({})));
        const title = stringField(b, "title");
        const videoUrl = stringField(b, "url");
        const channelId = stringField(b, "channelId");
        if (!title || !videoUrl || !channelId)
          return json({ error: "title, url, and channelId required" }, 400);
        return json(
          engine.addVideo(
            title,
            stringField(b, "description") || "",
            videoUrl,
            numberField(b, "duration") || 0,
            channelId,
            stringField(b, "tags") || "",
          ),
          201,
        );
      }
      const vm = path.match(/^\/api\/v1\/video\/videos\/([^/]+)$/);
      if (request.method === "GET" && vm) {
        const videoId = vm[1];
        if (!videoId) return json({ error: "not found" }, 404);
        const v = engine.getVideo(videoId);
        return v ? json(v, 200) : json({ error: "not found" }, 404);
      }
      if (request.method === "GET" && path === "/api/v1/video/channels")
        return json(engine.listChannels(), 200);
      if (request.method === "POST" && path === "/api/v1/video/channels") {
        const b = object(await request.json().catch(() => ({})));
        const name = stringField(b, "name");
        if (!name) return json({ error: "name required" }, 400);
        return json(engine.addChannel(name, stringField(b, "description")), 201);
      }
      if (request.method === "GET" && path === "/api/v1/video/playlists")
        return json(engine.listPlaylists(), 200);
      if (request.method === "POST" && path === "/api/v1/video/playlists") {
        const b = object(await request.json().catch(() => ({})));
        const name = stringField(b, "name");
        if (!name) return json({ error: "name required" }, 400);
        return json(engine.addPlaylist(name, stringField(b, "description")), 201);
      }
      return json({ error: "not found" }, 404);
    },
  });
  console.log(`[nexus-video] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    engine,
    close: () => {
      stopHeartbeat();
      phantom.stop();
      server.stop();
    },
  };
}
