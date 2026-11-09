import { randomUUID } from "node:crypto";
import { join } from "node:path";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { startHeartbeat } from "./cloud";
import { PhotosEngine } from "./photos-engine";

const WEBP_PLACEHOLDER = Buffer.from(
  "UklGRkAAAABXRUJQVlA4WAoAAAAQAAAAAAAAAAAAQUxQSAIAAAAAAFZQOCAYAAAAMAEAnQEqAQABAAFAJiWkAANwAP79NvgA",
  "base64",
);

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

function arrayField(body: JsonObject, key: string): unknown[] | undefined {
  return Array.isArray(body[key]) ? (body[key] as unknown[]) : undefined;
}

export function createServer(dbPath = process.env.PHOTOS_DB_PATH || "data/photos.sqlite") {
  const port = Number(process.env.PORT || "3096");
  const baseUrl = process.env.NEXUS_NEXUS_PHOTOS_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new PhotosEngine(dbPath);
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
            capabilities: [
              "photos",
              "albums",
              "gallery",
              "exif-parser",
              "batch-actions",
              "webp-thumbnails",
            ],
            cloudIntegration: {
              enabled: (process.env.NEXUS_PHOTOS_ENABLE_CLOUD_INTEGRATION || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
          },
          200,
        );
      }

      // Photos listing & addition
      if (request.method === "GET" && path === "/api/v1/photos") {
        const a = url.searchParams.get("albumId") || undefined;
        const fav = url.searchParams.get("favorites") === "true";
        return json(engine.listPhotos(a, fav), 200);
      }

      if (request.method === "POST" && path === "/api/v1/photos") {
        const b = object(await request.json().catch(() => ({})));
        const title = stringField(b, "title");
        const photoUrl = stringField(b, "url");
        if (!title || !photoUrl) return json({ error: "title and url required" }, 400);

        const customExif = b.exif && typeof b.exif === "object" ? (b.exif as any) : undefined;
        const photo = engine.addPhoto(
          title,
          photoUrl,
          stringField(b, "albumId") || "",
          stringField(b, "tags") || "",
          numberField(b, "width") || 3840,
          numberField(b, "height") || 2160,
          numberField(b, "fileSize") || 4194304,
          customExif,
          stringField(b, "thumbnailUrl"),
        );
        return json(photo, 201);
      }

      // Batch endpoints
      if (request.method === "POST" && path === "/api/v1/photos/batch/delete") {
        const b = object(await request.json().catch(() => ({})));
        const ids = (arrayField(b, "ids") || []).map(String);
        const res = engine.batchDeletePhotos(ids);
        return json(res, 200);
      }

      if (request.method === "POST" && path === "/api/v1/photos/batch/move") {
        const b = object(await request.json().catch(() => ({})));
        const ids = (arrayField(b, "ids") || []).map(String);
        const albumId = stringField(b, "albumId") || "";
        const res = engine.batchMovePhotos(ids, albumId);
        return json(res, 200);
      }

      if (request.method === "POST" && path === "/api/v1/photos/batch/download") {
        const b = object(await request.json().catch(() => ({})));
        const ids = (arrayField(b, "ids") || []).map(String);
        const photos = ids.map((id) => engine.getPhoto(id)).filter(Boolean);
        return json(
          {
            packageId: randomUUID(),
            zipName: `nexus-photos-export-${Date.now()}.zip`,
            count: photos.length,
            photos: photos.map((p) => ({
              id: p!.id,
              title: p!.title,
              url: p!.url,
              fileSize: p!.fileSize,
            })),
            ready: true,
          },
          200,
        );
      }

      // Albums routes
      if (request.method === "GET" && path === "/api/v1/photos/albums") {
        return json(engine.listAlbums(), 200);
      }

      if (request.method === "POST" && path === "/api/v1/photos/albums") {
        const b = object(await request.json().catch(() => ({})));
        const name = stringField(b, "name");
        if (!name) return json({ error: "name required" }, 400);
        return json(engine.addAlbum(name, stringField(b, "description")), 201);
      }

      const albumDetailMatch = path.match(/^\/api\/v1\/photos\/albums\/([^/]+)$/);
      if (albumDetailMatch) {
        const albumId = albumDetailMatch[1];
        if (request.method === "GET") {
          const album = engine.getAlbum(albumId);
          return album ? json(album, 200) : json({ error: "album not found" }, 404);
        }
        if (request.method === "DELETE") {
          const ok = engine.deleteAlbum(albumId);
          return ok ? json({ success: true, deletedAlbumId: albumId }, 200) : json({ error: "not found" }, 404);
        }
      }

      // Thumbnail generation & serving
      const thumbGenMatch = path.match(/^\/api\/v1\/photos\/([^/]+)\/thumbnail$/);
      if (request.method === "POST" && thumbGenMatch) {
        const photoId = thumbGenMatch[1];
        try {
          const res = engine.generateThumbnail(photoId, "webp");
          return json(res, 200);
        } catch (e) {
          return json({ error: (e as Error).message }, 404);
        }
      }

      const thumbServeMatch = path.match(/^\/api\/v1\/photos\/([^/]+)\/thumbnail\.webp$/);
      if (request.method === "GET" && thumbServeMatch) {
        return new Response(WEBP_PLACEHOLDER, {
          status: 200,
          headers: {
            "content-type": "image/webp",
            "cache-control": "public, max-age=86400, immutable",
          },
        });
      }

      // EXIF metadata inspection
      const exifMatch = path.match(/^\/api\/v1\/photos\/([^/]+)\/exif$/);
      if (request.method === "GET" && exifMatch) {
        const photoId = exifMatch[1];
        const photo = engine.getPhoto(photoId);
        if (!photo) return json({ error: "photo not found" }, 404);
        return json(
          {
            photoId: photo.id,
            title: photo.title,
            exif: photo.exif,
            dimensions: { width: photo.width, height: photo.height },
            fileSize: photo.fileSize,
          },
          200,
        );
      }

      // Favorite toggle
      const favMatch = path.match(/^\/api\/v1\/photos\/([^/]+)\/favorite$/);
      if (request.method === "PATCH" && favMatch) {
        const photoId = favMatch[1];
        const updated = engine.toggleFavorite(photoId);
        return updated ? json(updated, 200) : json({ error: "photo not found" }, 404);
      }

      // GPU test validation routes
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

      // Single photo CRUD
      const pm = path.match(/^\/api\/v1\/photos\/([^/]+)$/);
      if (pm) {
        const photoId = pm[1];
        if (request.method === "GET") {
          const p = engine.getPhoto(photoId);
          return p ? json(p, 200) : json({ error: "not found" }, 404);
        }
        if (request.method === "DELETE") {
          const ok = engine.deletePhoto(photoId);
          return ok ? json({ success: true, deletedPhotoId: photoId }, 200) : json({ error: "not found" }, 404);
        }
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
