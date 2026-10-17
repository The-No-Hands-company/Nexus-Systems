import { randomUUID } from "node:crypto";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { startHeartbeat } from "./cloud";
import { PhotosEngine } from "./photos-engine";
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
export function createServer() {
  const port = Number(process.env.PORT || "3096");
  const baseUrl = process.env.NEXUS_NEXUS_PHOTOS_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new PhotosEngine("data/photos.sqlite");
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
            service: "nexus-photos",
            status: "ok",
            version: "v1",
            uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
            timestamp: new Date().toISOString(),
          },
          200,
        );
      }
      if (request.method === "GET" && path === "/api/v1/status") {
        return json(
          {
            service: "nexus-photos",
            status: "ready",
            capabilities: ["photos", "albums", "gallery"],
            cloudIntegration: {
              enabled: (process.env.NEXUS_PHOTOS_ENABLE_CLOUD_INTEGRATION || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
          },
          200,
        );
      }
      if (request.method === "GET" && path === "/api/v1/photos") {
        const a = url.searchParams.get("albumId") || undefined;
        return json(engine.listPhotos(a), 200);
      }
      if (request.method === "POST" && path === "/api/v1/photos") {
        const b = object(await request.json().catch(() => ({})));
        const title = stringField(b, "title");
        const url = stringField(b, "url");
        if (!title || !url) return json({ error: "title and url required" }, 400);
        return json(
          engine.addPhoto(
            title,
            url,
            stringField(b, "albumId") || "",
            stringField(b, "tags") || "",
            numberField(b, "width") || 0,
            numberField(b, "height") || 0,
            numberField(b, "fileSize") || 0,
          ),
          201,
        );
      }
      if (request.method === "GET" && path === "/api/v1/photos/albums")
        return json(engine.listAlbums(), 200);
      if (request.method === "POST" && path === "/api/v1/photos/albums") {
        const b = object(await request.json().catch(() => ({})));
        const name = stringField(b, "name");
        if (!name) return json({ error: "name required" }, 400);
        return json(engine.addAlbum(name, stringField(b, "description")), 201);
      }
      if (request.method === "POST" && path === "/api/v1/photos/validate") {
        const b = object(await request.json().catch(() => ({})));
        const photoId = stringField(b, "photoId");
        if (!photoId) return json({ error: "photoId required" }, 400);
        const gpu = await discovery.resolve("nexus-gpu-test");
        if (!gpu) return json({ error: "gpu test service not registered in Cloud" }, 502);
        try {
          const r = await fetch(`${gpu.url}/api/v1/gpu-test/jobs`, {
            method: "POST",
            headers: { "content-type": "application/json" },
            body: JSON.stringify({
              name: `photo-validation-${photoId}`,
              tolerance: numberField(b, "tolerance") ?? 2.3,
            }),
          });
          if (!r.ok) return json({ error: "gpu test service error", status: r.status }, 502);
          const job = object(await r.json());
          return json({ jobId: job.id, status: job.status, photoId }, 201);
        } catch (e) {
          return json({ error: "gpu test unreachable", detail: (e as Error).message }, 502);
        }
      }
      const vpm = path.match(/^\/api\/v1\/photos\/validate\/([^/]+)$/);
      if (request.method === "GET" && vpm) {
        const jobId = vpm[1];
        if (!jobId) return json({ error: "job not found" }, 404);
        const gpu = await discovery.resolve("nexus-gpu-test");
        if (!gpu) return json({ error: "gpu test not available" }, 502);
        try {
          const r = await fetch(`${gpu.url}/api/v1/gpu-test/jobs/${encodeURIComponent(jobId)}`);
          if (!r.ok) return json({ error: "job not found", status: r.status }, 404);
          const job = object(await r.json());
          return json(
            {
              jobId: job.id,
              status: job.status,
              name: job.name,
              workerId: job.worker_id,
              tolerance: job.tolerance,
              viewport: { width: job.viewport_width, height: job.viewport_height },
            },
            200,
          );
        } catch (e) {
          return json({ error: "gpu test unreachable", detail: (e as Error).message }, 502);
        }
      }
      const pm = path.match(/^\/api\/v1\/photos\/([^/]+)$/);
      if (request.method === "GET" && pm) {
        const photoId = pm[1];
        if (!photoId) return json({ error: "not found" }, 404);
        const p = engine.getPhoto(photoId);
        return p ? json(p, 200) : json({ error: "not found" }, 404);
      }
      return json({ error: "not found" }, 404);
    },
  });
  console.log(`[nexus-photos] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    engine,
    close: () => {
      stopHeartbeat();
      server.stop();
    },
  };
}
