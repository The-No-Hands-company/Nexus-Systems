import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";
import { randomUUID, createHash } from "node:crypto";
import { startNexusFilesHeartbeat } from "./cloud";
import {
  type StorageBackend,
  type FileRecord,
  type FileVersionRecord,
  type FileStatus,
  detectBackend,
  getDefaultDiskRoot,
} from "./storage";

interface ServerHandle {
  server: ReturnType<typeof Bun.serve>;
  close: () => void;
}

function jsonResponse(payload: unknown, status: number, headers?: Record<string, string>): Response {
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

function sha256(buffer: Buffer): string {
  return createHash("sha256").update(buffer).digest("hex");
}

export function createFilesServer(): ServerHandle {
  const port = Number(process.env.PORT || "3033");
  const baseUrl = process.env.NEXUS_FILES_BASE_URL || `http://localhost:${port}`;

  const backend: StorageBackend = detectBackend(getDefaultDiskRoot());

  let files: FileRecord[] = [];
  let versions: FileVersionRecord[] = [];
  let nextSeq = 1;

  function generateFileId(): string {
    return `file-${String(nextSeq++).padStart(6, "0")}`;
  }

  async function init(): Promise<void> {
    await backend.init();
    const meta = await backend.loadMetadata();
    files = meta.files;
    versions = meta.versions;
    const maxSeq = files.reduce((max, f) => {
      const m = f.id.match(/^file-(\d+)$/);
      return m ? Math.max(max, parseInt(m[1]!)) : max;
    }, 0);
    nextSeq = maxSeq + 1;
  }

  async function saveMeta(): Promise<void> {
    await backend.saveMetadata(files, versions);
  }

  init().catch((err) => {
    console.error(`[nexus-files] Storage init failed: ${(err as Error).message}`);
  });

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-files",
  name: "Nexus Files",
  description: "File storage service",
  port: 3033,
  capabilities: ["file-storage","content-upload","content-download","versioning","search","s3-backend"],
}));

const stopNexusHeartbeat = nexusClient.startCloudHeartbeat();
const stopNexusMonitor = nexusClient.startMonitorHeartbeat();
const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return jsonResponse({
          service: "nexus-files",
          status: "ok",
          version: "v2",
          fileCount: files.filter((f) => f.status === "active").length,
          storage: backend.getStorageInfo(),
          timestamp: new Date().toISOString(),
        }, 200);
      }

      // ── Root Web File Explorer UI ──
      if (request.method === "GET" && (path === "/" || path === "/index.html")) {
        const html = await Bun.file(new URL("../web/index.html", import.meta.url).pathname).text();
        return new Response(html, {
          headers: { "content-type": "text/html; charset=utf-8" },
        });
      }

      // ── Presigned share token generation ──
      const shareMatch = path.match(/^\/api\/v1\/files\/([^/]+)\/share$/);
      if (request.method === "POST" && shareMatch) {
        const fileId = shareMatch[1]!;
        const file = files.find((f) => f.id === fileId);
        if (!file || file.status === "deleted") return jsonResponse({ error: "file not found" }, 404);
        const expiresAt = Date.now() + 24 * 60 * 60 * 1000;
        const token = Buffer.from(JSON.stringify({ id: fileId, exp: expiresAt })).toString("base64url");
        return jsonResponse({
          status: "ok",
          token,
          expiresAt: new Date(expiresAt).toISOString(),
          shareUrl: `${baseUrl}/share/${token}`,
        }, 200);
      }

      // ── Access shared file ──
      const accessShareMatch = path.match(/^\/share\/([^/]+)$/);
      if (request.method === "GET" && accessShareMatch) {
        const token = accessShareMatch[1]!;
        try {
          const payload = JSON.parse(Buffer.from(token, "base64url").toString());
          if (payload.exp && Date.now() > payload.exp) {
            return new Response("Share link expired", { status: 410 });
          }
          const file = files.find((f) => f.id === payload.id);
          if (!file || file.status === "deleted") return new Response("File not found", { status: 404 });
          const stream = await backend.getFileStream(file.id);
          if (!stream) return new Response("Content not found", { status: 404 });
          return new Response(stream as any, {
            headers: {
              "content-type": file.contentType,
              "content-disposition": `inline; filename="${file.name}"`,
            },
          });
        } catch {
          return new Response("Invalid share link", { status: 400 });
        }
      }

      // ── List / Search files ──
      if (request.method === "GET" && path === "/api/v1/files") {
        const name = url.searchParams.get("name");
        const contentType = url.searchParams.get("type");
        let results = files.filter((f) => f.status === "active");

        if (name) {
          const lower = name.toLowerCase();
          results = results.filter((f) => f.name.toLowerCase().includes(lower));
        }
        if (contentType) {
          results = results.filter((f) => f.contentType.includes(contentType));
        }

        return jsonResponse({ status: "ok", files: results, total: results.length }, 200);
      }

      // ── Upload file (metadata + content) ──
      if (request.method === "POST" && path === "/api/v1/files") {
        const contentType = request.headers.get("content-type") || "";

        if (contentType.includes("multipart/form-data")) {
          const formData = await request.formData();
          const file = formData.get("file") as File | null;
          if (!file) return jsonResponse({ error: "file field is required" }, 400);

          const buffer = Buffer.from(await file.arrayBuffer());
          const id = generateFileId();
          const hash = sha256(buffer);
          const now = new Date().toISOString();

          await backend.storeFile(id, buffer);

          const record: FileRecord = {
            id,
            name: file.name || "unnamed",
            size: buffer.length,
            contentType: file.type || "application/octet-stream",
            sha256: hash,
            version: 1,
            status: "active",
            createdAt: now,
            updatedAt: now,
          };

          files.push(record);

          const versionRecord: FileVersionRecord = {
            fileId: id,
            version: 1,
            size: buffer.length,
            sha256: hash,
            createdAt: now,
          };
          versions.push(versionRecord);

          await saveMeta();
          nexusClient.indexInSearch({ title: record.name, content: record.contentType, source: "nexus-files", url: `http://localhost:${port}/api/v1/files/${id}` }).catch(() => {});
          return jsonResponse({ status: "ok", file: record }, 201);
        }

        // JSON metadata-only (backward compat)
        const body = await request.json().catch(() => ({})) as Record<string, unknown>;
        const name = typeof body.name === "string" ? body.name.trim() : "";
        if (!name) return jsonResponse({ error: "name is required" }, 400);

        const id = generateFileId();
        const now = new Date().toISOString();
        const record: FileRecord = {
          id,
          name,
          size: typeof body.size === "number" ? body.size : 0,
          contentType: typeof body.contentType === "string" ? body.contentType : "application/octet-stream",
          sha256: "",
          version: 1,
          status: "active",
          createdAt: now,
          updatedAt: now,
        };
        files.push(record);
        await saveMeta();
        return jsonResponse({ status: "accepted", id }, 202);
      }

      // ── Get file metadata ──
      const fileByIdMatch = path.match(/^\/api\/v1\/files\/([^/]+)$/);
      if (request.method === "GET" && fileByIdMatch) {
        const fileId = fileByIdMatch[1]!;
        const file = files.find((f) => f.id === fileId && f.status === "active");
        if (!file) return jsonResponse({ error: "file not found" }, 404);
        return jsonResponse({ status: "ok", file }, 200);
      }

      // ── Download file content ──
      const contentMatch = path.match(/^\/api\/v1\/files\/([^/]+)\/content$/);
      if (request.method === "GET" && contentMatch) {
        const fileId = contentMatch[1]!;
        const file = files.find((f) => f.id === fileId && f.status === "active");
        if (!file) return jsonResponse({ error: "file not found" }, 404);

        const data = await backend.readFile(fileId);
        if (!data) return jsonResponse({ error: "file content not found" }, 404);

        return new Response(data, {
          status: 200,
          headers: {
            "content-type": file.contentType,
            "content-length": String(data.length),
            "content-disposition": `inline; filename="${file.name}"`,
            "x-file-sha256": file.sha256,
          },
        });
      }

      // ── Upload file content for existing metadata-only file ──
      if (request.method === "PUT" && contentMatch) {
        const fileId = contentMatch[1]!;
        const file = files.find((f) => f.id === fileId);
        if (!file) return jsonResponse({ error: "file not found" }, 404);

        const buffer = Buffer.from(await request.arrayBuffer());
        const hash = sha256(buffer);
        const now = new Date().toISOString();

        await backend.storeFile(fileId, buffer);

        const newVersion = file.version + 1;
        await backend.deleteVersionFile(fileId, file.version);

        file.size = buffer.length;
        file.sha256 = hash;
        file.version = newVersion;
        file.updatedAt = now;

        versions.push({
          fileId: file.id,
          version: newVersion,
          size: buffer.length,
          sha256: hash,
          createdAt: now,
        });

        await saveMeta();
        return jsonResponse({ status: "ok", file }, 200);
      }

      // ── Delete file (soft delete) ──
      if (request.method === "DELETE" && fileByIdMatch) {
        const fileId = fileByIdMatch[1]!;
        const file = files.find((f) => f.id === fileId);
        if (!file || file.status === "deleted") return jsonResponse({ error: "file not found" }, 404);

        file.status = "deleted";
        file.updatedAt = new Date().toISOString();
        await saveMeta();
        return jsonResponse({ status: "ok", deleted: true, id: fileId }, 200);
      }

      // ── File versions ──
      const versionsMatch = path.match(/^\/api\/v1\/files\/([^/]+)\/versions$/);
      if (request.method === "GET" && versionsMatch) {
        const fileId = versionsMatch[1]!;
        const fileVersions = versions.filter((v) => v.fileId === fileId);
        return jsonResponse({ status: "ok", versions: fileVersions }, 200);
      }

      return jsonResponse({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-files] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusFilesHeartbeat(baseUrl);

  return {
    server,
    close: () => {
      stopNexusHeartbeat();
      stopNexusMonitor();
      nexusClient.stop();
      stopHeartbeat();
      server.stop();
    },
  };
}
