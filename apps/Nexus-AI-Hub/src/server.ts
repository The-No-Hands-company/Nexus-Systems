import { randomUUID } from "node:crypto";
import { startHeartbeat } from "./cloud";
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";
import { NexusDiscovery } from "../../../packages/nexus-discovery/src/index";
import { AIHubEngine } from "./ai-hub-engine";
import {
  buildKnowledgeResponse,
  searchKnowledge,
  type KnowledgeCatalogApp,
  type KnowledgeResponse,
} from "./ecosystem-knowledge";

function json(p: unknown, s = 200): Response {
  return new Response(JSON.stringify(p), {
    status: s,
    headers: { "content-type": "application/json; charset=utf-8", "x-request-id": randomUUID() },
  });
}

export async function createServer(options: {
  cloudUrl?: string;
  catalogFetch?: (input: string, init?: RequestInit) => Promise<Response>;
} = {}) {
  const port = Number(process.env.PORT || "3114");
  const baseUrl = process.env.NEXUS_NEXUS_AI_HUB_BASE_URL || `http://localhost:${port}`;
  const cloudUrl = (options.cloudUrl || process.env.NEXUS_CLOUD_URL || "http://localhost:8787").replace(/\/$/, "");
  const catalogFetch = options.catalogFetch || fetch;
  const startedAt = Date.now();
  const engine = new AIHubEngine("data/nexus-ai-hub.sqlite")
  const phantom = new PhantomApp("nexus-ai-hub");
  const phantomId = await phantom.start();
  const discovery = new NexusDiscovery({ cloudUrl, apiKey: process.env.NEXUS_CLOUD_API_KEY || undefined, ttlMs: 30000 });
;

  let knowledgeCache: KnowledgeResponse | null = null;
  let knowledgeExpiresAt = 0;
  let knowledgeRefresh: Promise<KnowledgeResponse> | null = null;

  const readKnowledge = async (): Promise<KnowledgeResponse> => {
    if (knowledgeCache && knowledgeExpiresAt > Date.now()) return knowledgeCache;
    if (knowledgeRefresh) return knowledgeRefresh;

    knowledgeRefresh = (async () => {
      try {
        const response = await catalogFetch(`${cloudUrl}/api/v1/catalog`, {
          headers: {
            accept: "application/json",
            ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
          },
          signal: AbortSignal.timeout(1500),
        });
        if (!response.ok) throw new Error(`Cloud returned ${response.status}`);
        const payload = await response.json() as { apps?: unknown };
        const apps = catalogApps(payload.apps);
        const fresh = buildKnowledgeResponse(apps, {
          generatedAt: new Date().toISOString(),
          status: "ready",
          cloudUrl,
        });
        knowledgeCache = fresh;
        knowledgeExpiresAt = Date.now() + 30_000;
        return fresh;
      } catch {
        const degraded = buildKnowledgeResponse([], {
          generatedAt: new Date().toISOString(),
          status: "degraded",
          cloudUrl,
        });
        // Keep a successful catalog while Cloud is restarting. A degraded
        // response is only the fallback when no usable snapshot exists.
        const fallback = knowledgeCache?.status === "ready" ? knowledgeCache : degraded;
        knowledgeCache = fallback;
        knowledgeExpiresAt = Date.now() + 5_000;
        return fallback;
      } finally {
        knowledgeRefresh = null;
      }
    })();
    return knowledgeRefresh;
  };

  const server = Bun.serve({
    port,
    async fetch(req) {
      const url = new URL(req.url);
      const p = url.pathname || "";

      if (req.method === "GET" && p === "/health")
        return json({
          service: "nexus-ai-hub",
          status: "ok",
          version: "v1",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          phantom: phantom.status(),
        });

      if (req.method === "GET" && p === "/api/v1/status")
        return json(
          {
            service: "nexus-ai-hub",
            status: "ready",
            capabilities: ["ai-management", "prompt-engineering", "model-versioning"],
            cloudIntegration: {
              enabled: (process.env["NEXUS_AI_HUB_ENABLE_CLOUD_INTEGRATION"] || "true") !== "false",
              cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
            },
          },
          200
        );

      if (req.method === "GET" && p === "/api/v1/knowledge") {
        return json(await readKnowledge());
      }

      if (req.method === "GET" && p === "/api/v1/knowledge/search") {
        const knowledge = await readKnowledge();
        const query = url.searchParams.get("q") || "";
        return json({ ...knowledge, documents: searchKnowledge(knowledge.documents, query) });
      }

      if (req.method === "GET" && p === "/api/v1/items")
        return json(engine.list());

      if (req.method === "POST" && p === "/api/v1/items") {
        const b = await req.json().catch(() => ({})) as any;
        if (!b.name) return json({ error: "name required" }, 400);
        return json(engine.create(b.name, b.ai_models, b.prompts, b.templates), 201);
      }

      const im = p.match(/^\/api\/v1\/items\/([^/]+)$/);
      if (req.method === "GET" && im) {
        const item = engine.get(im[1]!);
        return item ? json(item) : json({ error: "not found" }, 404);
      }

      return json({ error: "not found" }, 404);
    },
  });

  console.log(`[nexus-ai-hub] Listening on port ${server.port}`);
  const stopHeartbeat = startHeartbeat(baseUrl);
  return { server, close: () => { stopHeartbeat(); phantom.stop(); server.stop(); } };
}

function catalogApps(value: unknown): KnowledgeCatalogApp[] {
  if (!Array.isArray(value)) return [];
  return value.flatMap((item): KnowledgeCatalogApp[] => {
    if (!item || typeof item !== "object") return [];
    const record = item as Record<string, unknown>;
    if (typeof record.id !== "string" || typeof record.name !== "string") return [];
    const strings = (candidate: unknown): string[] =>
      Array.isArray(candidate) && candidate.every((entry) => typeof entry === "string")
        ? [...candidate]
        : [];
    return [{
      id: record.id,
      name: record.name,
      description: typeof record.description === "string" ? record.description : "",
      ...(typeof record.path === "string" ? { path: record.path } : {}),
      ...(typeof record.publicUrl === "string" ? { publicUrl: record.publicUrl } : {}),
      primaryShelf: typeof record.primaryShelf === "string" ? record.primaryShelf : "explore",
      secondaryShelves: strings(record.secondaryShelves),
      audiences: strings(record.audiences),
      capabilities: strings(record.capabilities),
      maturity: typeof record.maturity === "string" ? record.maturity : "experimental",
      visibility: typeof record.visibility === "string" ? record.visibility : "discoverable",
      reviewStatus: typeof record.reviewStatus === "string" ? record.reviewStatus : "pending",
      health: typeof record.health === "string" ? record.health : "offline",
    }];
  });
}
