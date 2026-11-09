import { randomUUID } from "node:crypto";
import { join } from "node:path";
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

export async function createServer(dbPath = process.env.VIDEO_DB_PATH || "data/video.sqlite") {
  const port = Number(process.env.PORT || "3113");
  const baseUrl = process.env.NEXUS_NEXUS_VIDEO_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new VideoEngine(dbPath);
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
            capabilities: [
              "video",
              "channels",
              "playlists",
              "streaming",
              "hls-adaptive",
              "webvtt-subtitles",
              "video-chapters",
              "playback-resume",
              "chunked-upload",
            ],
            cloudIntegration: {
              enabled: (process.env.NEXUS_VIDEO_ENABLE_CLOUD_INTEGRATION || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
            phantom: phantom.status(),
          },
          200,
        );
      }

      // Chunked Upload API
      if (request.method === "POST" && path === "/api/v1/video/upload/init") {
        const b = object(await request.json().catch(() => ({})));
        const filename = stringField(b, "filename") || "unnamed.mp4";
        const totalChunks = numberField(b, "totalChunks") || 1;
        const fileSize = numberField(b, "fileSize") || 0;
        const session = engine.initChunkedUpload(filename, totalChunks, fileSize);
        return json(session, 201);
      }

      if (request.method === "POST" && path === "/api/v1/video/upload/chunk") {
        const b = object(await request.json().catch(() => ({})));
        const uploadId = stringField(b, "uploadId");
        const chunkIndex = numberField(b, "chunkIndex");
        if (!uploadId || chunkIndex === undefined) {
          return json({ error: "uploadId and chunkIndex required" }, 400);
        }
        try {
          const session = engine.recordChunk(uploadId, chunkIndex);
          return json(
            {
              uploadId: session.uploadId,
              uploadedChunks: session.uploadedChunks.length,
              totalChunks: session.totalChunks,
              progress: Math.min(100, Math.round((session.uploadedChunks.length / session.totalChunks) * 100)),
              status: session.status,
            },
            200,
          );
        } catch (e) {
          return json({ error: (e as Error).message }, 404);
        }
      }

      if (request.method === "POST" && path === "/api/v1/video/upload/complete") {
        const b = object(await request.json().catch(() => ({})));
        const uploadId = stringField(b, "uploadId");
        const title = stringField(b, "title") || "Untitled Upload";
        const channelId = stringField(b, "channelId") || "default";
        const description = stringField(b, "description") || "";
        if (!uploadId) return json({ error: "uploadId required" }, 400);
        try {
          const video = engine.completeChunkedUpload(uploadId, title, channelId, description);
          return json(video, 201);
        } catch (e) {
          return json({ error: (e as Error).message }, 404);
        }
      }

      // Videos listing & addition
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

      // HLS Master Playlist
      const hlsMasterMatch = path.match(/^\/api\/v1\/video\/videos\/([^/]+)\/hls\/master\.m3u8$/);
      if (request.method === "GET" && hlsMasterMatch) {
        const videoId = hlsMasterMatch[1];
        const video = engine.getVideo(videoId);
        if (!video) return json({ error: "video not found" }, 404);
        const manifest = engine.generateHlsMasterPlaylist(videoId);
        return new Response(manifest, {
          status: 200,
          headers: {
            "content-type": "application/vnd.apple.mpegurl; charset=utf-8",
            "cache-control": "no-cache",
          },
        });
      }

      // HLS Variant Playlists (1080p, 720p, 480p)
      const hlsVariantMatch = path.match(/^\/api\/v1\/video\/videos\/([^/]+)\/hls\/([^/]+)\.m3u8$/);
      if (request.method === "GET" && hlsVariantMatch) {
        const videoId = hlsVariantMatch[1];
        const res = hlsVariantMatch[2];
        const video = engine.getVideo(videoId);
        if (!video) return json({ error: "video not found" }, 404);
        const variantManifest = `#EXTM3U
#EXT-X-VERSION:4
#EXT-X-TARGETDURATION:6
#EXT-X-MEDIA-SEQUENCE:0
#EXTINF:6.000,
segment0.ts
#EXTINF:6.000,
segment1.ts
#EXT-X-ENDLIST
`;
        return new Response(variantManifest, {
          status: 200,
          headers: {
            "content-type": "application/vnd.apple.mpegurl; charset=utf-8",
            "cache-control": "no-cache",
          },
        });
      }

      // WebVTT Subtitles
      const vttMatch = path.match(/^\/api\/v1\/video\/videos\/([^/]+)\/subtitles\.vtt$/);
      if (request.method === "GET" && vttMatch) {
        const videoId = vttMatch[1];
        const vtt = engine.generateWebVttSubtitles(videoId);
        return new Response(vtt, {
          status: 200,
          headers: {
            "content-type": "text/vtt; charset=utf-8",
            "cache-control": "public, max-age=86400",
          },
        });
      }

      // Video Chapters
      const chaptersMatch = path.match(/^\/api\/v1\/video\/videos\/([^/]+)\/chapters$/);
      if (request.method === "GET" && chaptersMatch) {
        const videoId = chaptersMatch[1];
        const video = engine.getVideo(videoId);
        if (!video) return json({ error: "video not found" }, 404);
        return json(video.chapters, 200);
      }

      // Playback Resume Progress
      const progressMatch = path.match(/^\/api\/v1\/video\/videos\/([^/]+)\/progress$/);
      if (progressMatch) {
        const videoId = progressMatch[1];
        if (request.method === "GET") {
          const userId = url.searchParams.get("userId") || "user-default";
          const progress = engine.getPlaybackPosition(videoId, userId);
          return progress ? json(progress, 200) : json({ positionSeconds: 0, durationSeconds: 0, completed: false }, 200);
        }
        if (request.method === "PUT") {
          const b = object(await request.json().catch(() => ({})));
          const userId = stringField(b, "userId") || "user-default";
          const pos = numberField(b, "position") ?? 0;
          const dur = numberField(b, "duration") ?? 0;
          const saved = engine.savePlaybackPosition(videoId, userId, pos, dur);
          return json(saved, 200);
        }
      }

      // Video Details
      const vm = path.match(/^\/api\/v1\/video\/videos\/([^/]+)$/);
      if (request.method === "GET" && vm) {
        const videoId = vm[1];
        if (!videoId) return json({ error: "not found" }, 404);
        const v = engine.getVideo(videoId);
        return v ? json(v, 200) : json({ error: "not found" }, 404);
      }

      // Channels
      if (request.method === "GET" && path === "/api/v1/video/channels")
        return json(engine.listChannels(), 200);
      if (request.method === "POST" && path === "/api/v1/video/channels") {
        const b = object(await request.json().catch(() => ({})));
        const name = stringField(b, "name");
        if (!name) return json({ error: "name required" }, 400);
        return json(engine.addChannel(name, stringField(b, "description")), 201);
      }

      // Playlists
      if (request.method === "GET" && path === "/api/v1/video/playlists")
        return json(engine.listPlaylists(), 200);
      if (request.method === "POST" && path === "/api/v1/video/playlists") {
        const b = object(await request.json().catch(() => ({})));
        const name = stringField(b, "name");
        if (!name) return json({ error: "name required" }, 400);
        return json(engine.addPlaylist(name, stringField(b, "description")), 201);
      }

      // Static UI files
      if (request.method === "GET" && !path.startsWith("/api/") && path !== "/health") {
        const webRoot = process.env.WEB_ROOT || join(import.meta.dir, "..", "frontend", "dist");
        if (path !== "/") {
          const asset = Bun.file(join(webRoot, path));
          if (await asset.exists()) {
            const isHashed = path.startsWith("/assets/") && /\.(js|css|woff2?|svg|png|jpg)$/.test(path);
            const headers: Record<string, string> = { "X-Content-Type-Options": "nosniff" };
            if (isHashed) headers["cache-control"] = "public, max-age=31536000, immutable";
            return new Response(asset, { headers });
          }
        }
        const shell = Bun.file(join(webRoot, "index.html"));
        if (await shell.exists()) {
          return new Response(shell, {
            headers: {
              "content-type": "text/html; charset=utf-8",
              "cache-control": "no-store, must-revalidate",
              "X-Content-Type-Options": "nosniff",
            },
          });
        }
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
