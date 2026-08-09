import { randomUUID } from "node:crypto";

import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { SearchEngine } from "./search-engine";
import { startNexusSearchHeartbeat } from "./cloud";
import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";

function json(payload: unknown, status: number, h?: Record<string, string>): Response {
  return new Response(JSON.stringify(payload), { status, headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID(), ...h } });
}

// async because phantom.start() is awaited below. The Phantom rollout added that
// await without converting the function, so this file has not parsed since —
// the app could not start at all.
export async function createSearchServer() {
  const port = Number(process.env.PORT || "3034");
  const baseUrl = process.env.NEXUS_SEARCH_BASE_URL || `http://localhost:${port}`;
  const startedAt = Date.now();
  const engine = new SearchEngine(process.env.NEXUS_SEARCH_DB_PATH || "data/search.sqlite")
  const phantom = new PhantomApp("nexus-search");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787", apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;
  let syncTimer: ReturnType<typeof setInterval> | undefined;

  if (process.env.NEXUS_SEARCH_AUTO_SYNC !== "false") {
    syncTimer = setInterval(() => { engine.syncAllSources().catch(() => {}); }, 300000);
    if (typeof syncTimer.unref === "function") syncTimer.unref();
    engine.syncAllSources().catch(() => {});
  }

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-search",
  name: "Nexus Search",
  description: "Cross-ecosystem full-text search",
  port: 3034,
  capabilities: ["full-text-search","fts5-index","relevance-scoring","multi-source","federated-sources","auto-sync","facets"],
}));

const stopNexusHeartbeat = nexusClient.startCloudHeartbeat();
const stopNexusMonitor = nexusClient.startMonitorHeartbeat();
const server = Bun.serve({
    port,
    async fetch(request) {
      const url = new URL(request.url);
      const path = url.pathname || "";

      if (request.method === "GET" && path === "/health") {
        return json({ service: "nexus-search", status: "ok", version: "v2", uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000) }, 200);
      }

      if (request.method === "GET" && path === "/api/v1/search/status") {
        return json({ status: "ok", stats: engine.getStats(), sources: engine.listSources() }, 200);
      }

      // ── Search ──
      if (request.method === "POST" && path === "/api/v1/search/query") {
        const body = await request.json().catch(() => ({})) as Record<string, unknown>;
        const query = typeof body.query === "string" ? body.query.trim() : "";
        if (!query) return json({ error: "query is required" }, 400);

        const result = engine.search(query, {
          source: typeof body.source === "string" ? body.source : undefined,
          limit: typeof body.limit === "number" ? body.limit : 20,
          offset: typeof body.offset === "number" ? body.offset : 0,
          sort: typeof body.sort === "string" && body.sort === "date" ? "date" : "relevance",
          facets: typeof body.facets === "boolean" ? body.facets : false,
        });
        return json({ status: "ok", query, ...result }, 200);
      }

      // ── Index ──
      if (request.method === "POST" && path === "/api/v1/search/index") {
        const body = await request.json().catch(() => ({})) as Record<string, unknown>;
        const title = typeof body.title === "string" ? body.title.trim() : "";
        const content = typeof body.content === "string" ? body.content : "";
        if (!title || !content) return json({ error: "title and content are required" }, 400);
        const doc = engine.indexDocument({
          id: typeof body.id === "string" ? body.id : undefined,
          title, content,
          source: typeof body.source === "string" ? body.source : "api",
          url: typeof body.url === "string" ? body.url : undefined,
          metadata: (typeof body.metadata === "object" ? body.metadata : {}) as Record<string, string>,
        });
        return json({ status: "ok", document: doc }, 201);
      }

      if (request.method === "POST" && path === "/api/v1/search/index/batch") {
        const body = await request.json().catch(() => ({})) as { documents?: Array<Record<string, unknown>> };
        if (!Array.isArray(body.documents)) return json({ error: "documents array is required" }, 400);
        const docs = engine.indexDocuments(body.documents.filter((d) => typeof d.title === "string" && typeof d.content === "string").map((d) => ({
          title: d.title as string, content: d.content as string,
          source: (d.source as string) || "api",
          url: d.url as string | undefined,
          metadata: (d.metadata as Record<string, string>) || {},
          id: undefined,
        })));
        return json({ status: "ok", indexed: docs.length }, 201);
      }

      // ── Get/Delete document ──
      const docMatch = path.match(/^\/api\/v1\/search\/index\/([^/]+)$/);
      if (request.method === "GET" && docMatch) {
        const doc = engine.getDocument(docMatch[1]!);
        return doc ? json({ document: doc }, 200) : json({ error: "not found" }, 404);
      }
      if (request.method === "DELETE" && docMatch) {
        return engine.deleteDocument(docMatch[1]!) ? json({ deleted: true }, 200) : json({ error: "not found" }, 404);
      }

      // ── List documents ──
      if (request.method === "GET" && path === "/api/v1/search/index") {
        const source = url.searchParams.get("source") ?? undefined;
        const docs = engine.getDocuments({ source, limit: Number(url.searchParams.get("limit") || "50"), offset: Number(url.searchParams.get("offset") || "0") });
        return json({ documents: docs }, 200);
      }

      // ── Federated Sources ──
      if (request.method === "GET" && path === "/api/v1/search/sources") {
        return json({ sources: engine.listSources() }, 200);
      }

      if (request.method === "POST" && path === "/api/v1/search/sources") {
        const body = await request.json().catch(() => ({})) as Record<string, unknown>;
        const name = typeof body.name === "string" ? body.name.trim() : "";
        const sourceUrl = typeof body.url === "string" ? body.url.trim() : "";
        if (!name || !sourceUrl) return json({ error: "name and url are required" }, 400);
        const type = typeof body.type === "string" && ["files","wiki","notes","custom"].includes(body.type) ? body.type as "files"|"wiki"|"notes"|"custom" : "custom";
        const s = engine.addSource({ name, url: sourceUrl, type, syncIntervalMs: typeof body.syncIntervalMs === "number" ? body.syncIntervalMs : 300000, authToken: typeof body.authToken === "string" ? body.authToken : undefined });
        return json({ source: s }, 201);
      }

      const srcMatch = path.match(/^\/api\/v1\/search\/sources\/([^/]+)$/);
      if (request.method === "DELETE" && srcMatch) {
        return engine.deleteSource(srcMatch[1]!) ? json({ deleted: true }, 200) : json({ error: "not found" }, 404);
      }

      // ── Sync ──
      if (request.method === "POST" && path === "/api/v1/search/sync") {
        const results = await engine.syncAllSources();
        return json({ status: "ok", synced: results }, 200);
      }

      const syncOneMatch = path.match(/^\/api\/v1\/search\/sources\/([^/]+)\/sync$/);
      if (request.method === "POST" && syncOneMatch) {
        const source = engine.getSource(syncOneMatch[1]!);
        if (!source) return json({ error: "source not found" }, 404);
        const result = await engine.syncSource(source);
        return json({ status: "ok", synced: result }, 200);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-search] Listening on port ${server.port}`);
  const stopHeartbeat = startNexusSearchHeartbeat(baseUrl);

  return { server, close: () => {
  stopNexusHeartbeat();
  stopNexusMonitor();
  nexusClient.stop(); stopHeartbeat(); if (syncTimer) clearInterval(syncTimer); engine.close(); phantom.stop(); server.stop(); } };
}
