import { randomUUID } from "node:crypto";
import { join } from "node:path";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { NotesEngine } from "./notes-engine";

function json(payload: unknown, status = 200, headers?: Record<string, string>): Response {
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

export async function createServer() {
  const port = process.env.PORT !== undefined ? Number(process.env.PORT) : 3093;
  const baseUrl = process.env.NEXUS_NEXUS_NOTES_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new NotesEngine("data/notes.sqlite");
  const phantom = new PhantomApp("nexus-notes");
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
        return json({
          service: "nexus-notes",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          timestamp: new Date().toISOString(),
          phantom: phantom.status(),
        });
      }

      if (request.method === "GET" && path === "/api/v1/status") {
        return json({
          service: "nexus-notes",
          status: "ready",
          capabilities: ["notes", "notebooks", "knowledge-base"],
          cloudIntegration: {
            enabled: (process.env["NEXUS_NOTES_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
          phantom: phantom.status(),
        });
      }

      // Notebooks
      if (request.method === "GET" && path === "/api/v1/notes/notebooks") {
        return json(engine.listNotebooks());
      }
      if (request.method === "POST" && path === "/api/v1/notes/notebooks") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.addNotebook(b.name, b.description || ""), 201);
      }

      // Notes Search
      if (request.method === "GET" && path === "/api/v1/notes/search") {
        const q = url.searchParams.get("q");
        if (!q) return json({ error: "q required" }, 400);
        return json(engine.searchNotes(q));
      }

      // Pin Note
      const pinMatch = path.match(/^\/api\/v1\/notes\/([^/]+)\/pin$/);
      if (request.method === "POST" && pinMatch) {
        const id = pinMatch[1]!;
        const updated = engine.togglePin(id);
        return updated ? json(updated) : json({ error: "not found" }, 404);
      }

      // Note by ID
      const nm = path.match(/^\/api\/v1\/notes\/([^/]+)$/);
      if (nm) {
        const id = nm[1]!;
        if (request.method === "GET") {
          const n = engine.getNote(id);
          return n ? json(n) : json({ error: "not found" }, 404);
        }
        if (request.method === "PATCH") {
          const b = await request.json().catch(() => ({})) as any;
          const updated = engine.updateNote(id, b);
          return updated ? json(updated) : json({ error: "not found" }, 404);
        }
        if (request.method === "DELETE") {
          const deleted = engine.deleteNote(id);
          return deleted ? json({ deleted: true }) : json({ error: "not found" }, 404);
        }
      }

      // Notes Collection
      if (request.method === "GET" && path === "/api/v1/notes") {
        const nb = url.searchParams.get("notebook") || undefined;
        return json(engine.listNotes(nb));
      }
      if (request.method === "POST" && path === "/api/v1/notes") {
        const b = await request.json().catch(() => ({})) as any;
        if (!b.title) return json({ error: "title required" }, 400);
        return json(engine.addNote(b.title, b.content || "", b.notebook || "General", b.tags || ""), 201);
      }

      // Static SPA Fallback
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

  console.log(`[nexus-notes] Listening on port ${server.port}`);
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
