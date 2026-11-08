import { randomUUID } from "node:crypto";
import { join } from "node:path";
import { startHeartbeat } from "./cloud";
import { DocsEngine } from "./docs-engine";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";

function json(p: unknown, s = 200, headers?: Record<string, string>): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: {
      "content-type": "application/json; charset=utf-8",
      "x-request-id": randomUUID(),
      ...headers,
    },
  });
}

export async function createServer() {
  const port = process.env.PORT !== undefined ? Number(process.env.PORT) : 3136;
  const baseUrl = process.env.NEXUS_DOCS_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new DocsEngine("data/nexus-docs.sqlite");

  const phantom = new PhantomApp("nexus-docs");
  const phantomId = await phantom.start();

  const discovery = new NexusDiscovery({
    cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
    apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined,
    ttlMs: 30000,
  });

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health") {
        return json({
          service: "nexus-docs",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          phantom: phantom.status(),
        });
      }

      if (req.method === "GET" && p === "/api/v1/status") {
        return json({
          service: "nexus-docs",
          status: "ready",
          capabilities: ["docs", "documents", "workspaces", "collaboration"],
          cloudIntegration: {
            enabled: (process.env["NEXUS_DOCS_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
          phantom: phantom.status(),
        });
      }

      // Folders
      if (req.method === "GET" && p === "/api/v1/folders") {
        return json(engine.listFolders());
      }
      if (req.method === "POST" && p === "/api/v1/folders") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createFolder(b.name, b.color, b.icon), 201);
      }

      // Document Search
      if (req.method === "GET" && p === "/api/v1/documents/search") {
        const q = url.searchParams.get("q");
        if (!q) return json({ error: "q required" }, 400);
        return json(engine.searchDocuments(q));
      }

      // Single Document by ID
      const docMatch = p.match(/^\/api\/v1\/documents\/([^/]+)$/);
      if (docMatch) {
        const id = docMatch[1]!;
        if (req.method === "GET") {
          const doc = engine.getDocument(id);
          return doc ? json(doc) : json({ error: "not found" }, 404);
        }
        if (req.method === "PATCH") {
          const b = (await req.json().catch(() => ({}))) as any;
          const updated = engine.updateDocument(id, b);
          return updated ? json(updated) : json({ error: "not found" }, 404);
        }
        if (req.method === "DELETE") {
          const deleted = engine.deleteDocument(id);
          return deleted ? json({ deleted: true }) : json({ error: "not found" }, 404);
        }
      }

      // Document Collections
      if (req.method === "GET" && p === "/api/v1/documents") {
        const folder = url.searchParams.get("folder") || undefined;
        return json(engine.listDocuments(folder));
      }
      if (req.method === "POST" && p === "/api/v1/documents") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.title) return json({ error: "title required" }, 400);
        return json(
          engine.createDocument(b.title, b.content || "", b.folder || "General", b.icon || "📄", b.coverUrl || ""),
          201
        );
      }

      // Legacy compatibility
      if (req.method === "GET" && p === "/api/v1/docs/docses") {
        return json(engine.listDocss());
      }
      if (req.method === "POST" && p === "/api/v1/docs/docses") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createDocs(b.name, b.type), 201);
      }
      const gm = p.match(/^\/api\/v1\/docs\/docses\/([^/]+)$/);
      if (req.method === "GET" && gm) {
        const entity = engine.getDocs(gm[1]!);
        return entity ? json(entity) : json({ error: "not found" }, 404);
      }

      // Static SPA Fallback
      if (req.method === "GET" && !p.startsWith("/api/") && p !== "/health") {
        const webRoot = process.env.WEB_ROOT || join(import.meta.dir, "..", "frontend", "dist");
        if (p !== "/") {
          const asset = Bun.file(join(webRoot, p));
          if (await asset.exists()) {
            const isHashed = p.startsWith("/assets/") && /\.(js|css|woff2?|svg|png|jpg)$/.test(p);
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

  console.log(`[nexus-docs] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return {
    server,
    engine,
    phantom,
    discovery,
    close: () => {
      stopHeartbeat();
      phantom.stop();
      server.stop();
    },
  };
}
