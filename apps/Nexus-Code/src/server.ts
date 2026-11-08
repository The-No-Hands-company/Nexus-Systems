import { randomUUID } from "node:crypto";
import { join } from "node:path";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { CodeEngine } from "./code-engine";

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
  const port = process.env.PORT !== undefined ? Number(process.env.PORT) : 3126;
  const baseUrl = process.env.NEXUS_NEXUS_CODE_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new CodeEngine("data/nexus-code.sqlite");
  const phantom = new PhantomApp("code");
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
          service: "nexus-code",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          phantom: phantom.status(),
        });
      }

      if (req.method === "GET" && p === "/api/v1/status") {
        return json(
          {
            service: "nexus-code",
            status: "ready",
            capabilities: ["code-snippets", "repositories", "version-control", "code-browser"],
            cloudIntegration: {
              enabled: (process.env["NEXUS_CODE_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
            phantom: phantom.status(),
          },
          200
        );
      }

      // Repositories
      if (req.method === "GET" && p === "/api/v1/repos") {
        return json(engine.listRepos());
      }
      if (req.method === "POST" && p === "/api/v1/repos") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.createRepo(b.name, b.description || "", b.defaultBranch || "main", b.visibility || "public"), 201);
      }

      // Single Repo
      const repoMatch = p.match(/^\/api\/v1\/repos\/([^/]+)$/);
      if (req.method === "GET" && repoMatch) {
        const repo = engine.getRepo(repoMatch[1]!);
        return repo ? json(repo) : json({ error: "not found" }, 404);
      }

      // Files in Repo
      const filesMatch = p.match(/^\/api\/v1\/repos\/([^/]+)\/files$/);
      if (filesMatch) {
        const repoId = filesMatch[1]!;
        const branch = url.searchParams.get("branch") || "main";
        if (req.method === "GET") {
          return json(engine.listFiles(repoId, branch));
        }
        if (req.method === "POST") {
          const b = (await req.json().catch(() => ({}))) as any;
          if (!b.path || b.content === undefined) return json({ error: "path and content required" }, 400);
          return json(engine.saveFile(repoId, b.path, b.content, b.message || "Update file", b.author || "operator", branch), 201);
        }
      }

      // Single File by Path in Repo
      const fileDetailMatch = p.match(/^\/api\/v1\/repos\/([^/]+)\/file\/(.+)$/);
      if (req.method === "GET" && fileDetailMatch) {
        const repoId = fileDetailMatch[1]!;
        const filePath = decodeURIComponent(fileDetailMatch[2]!);
        const branch = url.searchParams.get("branch") || "main";
        const f = engine.getFile(repoId, filePath, branch);
        return f ? json(f) : json({ error: "not found" }, 404);
      }

      // Commits in Repo
      const commitsMatch = p.match(/^\/api\/v1\/repos\/([^/]+)\/commits$/);
      if (req.method === "GET" && commitsMatch) {
        const repoId = commitsMatch[1]!;
        const branch = url.searchParams.get("branch") || "main";
        return json(engine.listCommits(repoId, branch));
      }

      // Snippets
      if (req.method === "GET" && p === "/api/v1/snippets") {
        return json(engine.listSnippets());
      }
      if (req.method === "POST" && p === "/api/v1/snippets") {
        const b = (await req.json().catch(() => ({}))) as any;
        if (!b.title || !b.code) return json({ error: "title and code required" }, 400);
        return json(engine.createSnippet(b.title, b.language || "text", b.code, b.tags || ""), 201);
      }
      const snippetMatch = p.match(/^\/api\/v1\/snippets\/([^/]+)$/);
      if (snippetMatch) {
        const sId = snippetMatch[1]!;
        if (req.method === "GET") {
          const s = engine.getSnippet(sId);
          return s ? json(s) : json({ error: "not found" }, 404);
        }
        if (req.method === "DELETE") {
          const deleted = engine.deleteSnippet(sId);
          return deleted ? json({ deleted: true }) : json({ error: "not found" }, 404);
        }
      }

      // Legacy compatibility
      if (req.method === "GET" && p === "/api/v1/items") {
        return json(engine.list());
      }
      if (req.method === "POST" && p === "/api/v1/items") {
        const b = (await req.json().catch(() => ({}))) as any;
        return json(engine.create(b.name || "Untitled", b.snippets, b.repos, b.files), 201);
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

  console.log(`[nexus-code] Listening on port ${server.port}`);
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
