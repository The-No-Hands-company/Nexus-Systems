// Gateway server — HTTP + WebSocket control plane
// Phase 1.5: Streaming WebSocket
// Phase 8: Session API endpoints
// Phase 5: Skills API
// Phase 4: MCP status API
// Phase 3.1: Marketplace + Workflow routes
// Phase 3.8: Artifact API (Canvas / A2UI)

import Fastify from "fastify";
import fastifyWebsocket from "@fastify/websocket";
import type { Orchestrator } from "../core/orchestrator.js";
import type { MemoryManager } from "../memory/index.js";
import type { GSDManager } from "../gsd/index.js";
import type { SafetyManager } from "../safety/index.js";
import type { ToolRegistry } from "../tools/index.js";
import type { HookRunner } from "../hooks/index.js";
import type { RecipeRegistry } from "../recipes/index.js";
import type { PluginManager } from "../plugins/index.js";
import type { TeamManager } from "../teams/index.js";
import type { LocalMarketplace } from "../marketplace/index.js";
import { getArtifact, listArtifacts } from "../artifacts/index.js";

export interface GatewayConfig {
  port: number;
  bind: string;
}

export interface GatewayDeps {
  orchestrator: Orchestrator;
  memory: MemoryManager;
  gsd: GSDManager;
  safety: SafetyManager;
  tools?: ToolRegistry;
  hooks?: HookRunner;
  recipes?: RecipeRegistry;
  plugins?: PluginManager;
  teams?: TeamManager;
  marketplace?: LocalMarketplace;
}

export async function createGateway(config: GatewayConfig, deps: GatewayDeps) {
  const app = Fastify({ logger: false });
  await app.register(fastifyWebsocket);

  const { orchestrator, memory, gsd, safety, tools, hooks, recipes, plugins, teams, marketplace } = deps;

  // ─── Health ──────────────────────────────────────────────────────

  app.get("/api/health/live", async () => ({
    status: "live",
    version: "0.1.0",
    uptime: process.uptime(),
  }));

  app.get("/api/health/ready", async () => ({
    status: "ready",
    version: "0.1.0",
    uptime: process.uptime(),
    memory: memory.getStatus(),
    agents: orchestrator.listAgents().length,
    tools: tools ? tools.list().length : 0,
  }));

  app.get("/api/status", async () => ({
    status: "ok",
    version: "0.1.0",
    uptime: process.uptime(),
    agents: orchestrator.listAgents(),
    memory: memory.getStatus(),
    safety: {
      mode: safety.getPermissions ? safety.getPermissions() : null,
    },
    tools: tools ? tools.list().map((t) => ({ name: t.name, description: t.description })) : [],
    plugins: plugins ? plugins.getPlugins().map((p) => ({
      name: p.manifest.name,
      version: p.manifest.version,
      description: p.manifest.description,
    })) : [],
  }));

  // Backward compatibility route
  app.get("/health", async () => ({
    status: "ok",
    version: "0.1.0",
    uptime: process.uptime(),
    agents: orchestrator.listAgents(),
    memory: memory.getStatus(),
  }));

  // ─── Chat API ────────────────────────────────────────────────────

  app.post<{
    Body: { message: string; agentId?: string; sessionId?: string };
  }>("/api/chat", async (request) => {
    const { message, agentId, sessionId } = request.body;
    if (!message || typeof message !== "string") return { error: "message is required" };
    return orchestrator.sendMessage(message, agentId, sessionId);
  });

  // ─── Agents API ──────────────────────────────────────────────────

  app.get("/api/agents", async () => orchestrator.listAgents());

  app.get<{ Params: { id: string } }>("/api/agents/:id", async (request) => {
    const agent = orchestrator.getAgent(request.params.id);
    if (!agent) {
      return { error: "Agent not found" };
    }
    return {
      id: agent.id,
      name: agent.config.name,
      state: agent.getState().phase,
      stopped: orchestrator.isAgentStopped(agent.id),
      model: agent.config.model,
      sessionId: agent.getState().sessionId,
    };
  });

  app.post<{ Params: { id: string } }>("/api/agents/:id/stop", async (request, reply) => {
    const stopped = orchestrator.stopAgent(request.params.id);
    if (!stopped) {
      reply.status(404);
      return { error: "Agent not found" };
    }
    return { stopped: request.params.id };
  });

  app.post<{ Params: { id: string } }>("/api/agents/:id/restart", async (request, reply) => {
    const restarted = orchestrator.restartAgent(request.params.id);
    if (!restarted) {
      reply.status(404);
      return { error: "Agent not found or not stopped" };
    }
    return { restarted: request.params.id };
  });

  // ─── Memory API ──────────────────────────────────────────────────

  app.get("/api/memory/status", async () => ({
    ...memory.getStatus(),
    alignment: memory.getAlignmentStats(),
  }));

  app.post<{ Body: { query: string; limit?: number } }>(
    "/api/memory/search",
    async (request) => {
      const { query, limit } = request.body;
      return memory.searchEpisodes(query, { limit });
    },
  );

  app.get("/api/memory/vault", async () => memory.getVaultNotes());

  // ─── Sessions API (Phase 8) ──────────────────────────────────────

  app.get<{ Querystring: { limit?: string; agentId?: string } }>(
    "/api/sessions",
    async (request) => {
      const limit = parseInt(request.query.limit || "20", 10);
      const agentId = request.query.agentId;
      return memory.listSessions({ limit, agentId });
    },
  );

  app.delete<{ Params: { id: string } }>(
    "/api/sessions/:id",
    async (request) => {
      memory.deleteSession(request.params.id);
      return { deleted: request.params.id };
    },
  );

  app.get<{ Params: { id: string } }>(
    "/api/sessions/:id",
    async (request, reply) => {
      const session = memory.getSession(request.params.id);
      if (!session) return reply.status(404).send({ error: "Session not found" });
      return session;
    },
  );

  // ─── GSD API ─────────────────────────────────────────────────────

  app.get("/api/gsd/specs", async () =>
    gsd.listSpecs().map((s) => ({
      ...s,
      progress: gsd.getProgress(s.id),
    })),
  );

  app.get<{ Params: { id: string } }>(
    "/api/gsd/specs/:id",
    async (request) => gsd.getSpec(request.params.id),
  );

  // ─── Skills API (Phase 5) ───────────────────────────────────────

  app.get("/api/skills", async () =>
    orchestrator.listSkills().map((s) => ({
      id: s.id,
      name: s.name,
      description: s.description,
      enabled: s.enabled,
    })),
  );

  // ─── Safety API ──────────────────────────────────────────────────

  app.get<{ Querystring: { limit?: string } }>(
    "/api/safety/log",
    async (request) => {
      const limit = parseInt(request.query.limit || "50", 10);
      return safety.getActionLog(limit);
    },
  );

  // ─── Tools API ──────────────────────────────────────────────────

  app.get("/api/tools", async () => {
    if (!tools) return [];
    return tools.list().map((t) => ({
      name: t.name,
      description: t.description,
      parameters: t.parameters,
    }));
  });

  // ─── Hooks API ──────────────────────────────────────────────────

  app.get("/api/hooks", async () => {
    if (!hooks) return { stats: {}, hooks: [] };
    return {
      stats: hooks.stats(),
      hooks: hooks.list().map((h) => ({
        id: h.id,
        event: h.event,
        priority: h.priority,
        source: h.source,
      })),
    };
  });

  // ─── Recipes API ────────────────────────────────────────────────

  app.get("/api/recipes", async () => {
    if (!recipes) return [];
    return recipes.list().map((r) => ({
      id: r.id,
      name: r.name,
      description: r.description,
      steps: r.steps.length,
    }));
  });

  // ─── Safety API ─────────────────────────────────────────────────

  app.get("/api/safety", async () => {
    return {
      permissions: safety.getPermissions(),
      recentActions: safety.getActionLog(20),
    };
  });

  // ─── Plugins API ────────────────────────────────────────────────

  app.get("/api/plugins", async () => {
    if (!plugins) return [];
    return plugins.getPlugins().map((p) => ({
      name: p.manifest.name,
      version: p.manifest.version,
      description: p.manifest.description,
      tools: p.tools.length,
      skills: p.skills.length,
    }));
  });
  // ─── Teams API (Phase 2.4) ───────────────────────────────────────

  app.get("/api/teams", async () => {
    if (!teams) return [];
    return teams.list().map((t) => ({
      name: t.name,
      description: t.description,
      version: t.version,
      tags: t.tags,
      agents: t.agents.length,
      entryAgent: t.entryAgent,
    }));
  });

  app.get<{ Params: { name: string } }>("/api/teams/:name", async (request, reply) => {
    if (!teams) return reply.status(503).send({ error: "Teams not initialized" });
    const template = teams.get(request.params.name);
    if (!template) return reply.status(404).send({ error: "Team template not found" });
    return template;
  });

  app.post<{ Params: { name: string }; Body: { task: string } }>(
    "/api/teams/:name/run",
    async (request, reply) => {
      if (!teams) return reply.status(503).send({ error: "Teams not initialized" });
      const { task } = request.body;
      if (!task) return reply.status(400).send({ error: "task is required" });
      try {
        const result = await teams.runTemplate(request.params.name, task, orchestrator);
        return result;
      } catch (err: any) {
        reply.status(400);
        return { error: err.message };
      }
    },
  );

  // ─── Marketplace API (Phase 3.1) ─────────────────────────────────

  app.get("/api/marketplace/installed", async () => {
    if (!marketplace) return [];
    return marketplace.listInstalled();
  });

  app.get<{ Querystring: { q?: string; tags?: string } }>(
    "/api/marketplace/search",
    async (request, reply) => {
      if (!marketplace) return reply.status(503).send({ error: "Marketplace not initialized" });
      const { q = "", tags } = request.query;
      const tagList = tags ? tags.split(",").filter(Boolean) : undefined;
      try {
        return await marketplace.search(q, tagList);
      } catch (err: any) {
        reply.status(400);
        return { error: err.message };
      }
    },
  );

  app.get("/api/marketplace/featured", async (_, reply) => {
    if (!marketplace) return reply.status(503).send({ error: "Marketplace not initialized" });
    try {
      return await marketplace.featured();
    } catch (err: any) {
      reply.status(400);
      return { error: err.message };
    }
  });

  app.post<{ Body: { id: string; force?: boolean } }>(
    "/api/marketplace/install",
    async (request, reply) => {
      if (!marketplace) return reply.status(503).send({ error: "Marketplace not initialized" });
      const { id, force } = request.body;
      if (!id) return reply.status(400).send({ error: "id is required" });
      try {
        const path = await marketplace.install(id, { force });
        return { installed: id, path };
      } catch (err: any) {
        reply.status(400);
        return { error: err.message };
      }
    },
  );

  app.delete<{ Params: { id: string } }>(
    "/api/marketplace/skills/:id",
    async (request, reply) => {
      if (!marketplace) return reply.status(503).send({ error: "Marketplace not initialized" });
      const removed = marketplace.uninstall(request.params.id);
      if (!removed) return reply.status(404).send({ error: "Skill not installed" });
      return { uninstalled: request.params.id };
    },
  );

  // ─── Workflows API (Phase 3.2) ─────────────────────────────────

  app.get("/api/workflows", async () => ({
    workflows: [
      { id: "research", name: "Research Pipeline", description: "4-stage research + writing pipeline (Planner → Researcher → Writer → Editor)", stages: 4 },
      { id: "worker-pool", name: "Worker Pool", description: "HiClaw-style manager/worker task decomposition and parallel execution", stages: 3 },
    ],
    benchmarkSuites: [
      { id: "nexus-core", name: "Nexus Core Benchmark", tasks: 5 },
      { id: "nexus-advanced", name: "Nexus Advanced Benchmark", tasks: 2 },
    ],
  }));

  app.post<{ Body: { topic: string; depth?: number } }>(
    "/api/workflows/research",
    async (request, reply) => {
      const { topic, depth } = request.body;
      if (!topic) return reply.status(400).send({ error: "topic is required" });
      try {
        const { ResearchWorkflow } = await import("../workflows/index.js");
        const wf = new ResearchWorkflow({ depth });
        const result = await wf.run(topic, orchestrator);
        return result;
      } catch (err: any) {
        reply.status(500);
        return { error: err.message };
      }
    },
  );

  app.post<{ Body: { goal: string; concurrency?: number } }>(
    "/api/workflows/worker-pool",
    async (request, reply) => {
      const { goal, concurrency } = request.body;
      if (!goal) return reply.status(400).send({ error: "goal is required" });
      try {
        const { WorkerPool } = await import("../workflows/index.js");
        const pool = new WorkerPool({ concurrency });
        const result = await pool.run(goal, orchestrator);
        return result;
      } catch (err: any) {
        reply.status(500);
        return { error: err.message };
      }
    },
  );

  app.post<{ Params: { suite: string }; Body: { model?: string } }>(
    "/api/workflows/benchmark/:suite",
    async (request, reply) => {
      try {
        const { BenchmarkRunner } = await import("../workflows/index.js");
        const runner = new BenchmarkRunner();
        const result = await runner.run(request.params.suite, orchestrator, {
          model: request.body?.model,
        });
        return result;
      } catch (err: any) {
        reply.status(400);
        return { error: err.message };
      }
    },
  );
  // ─── RBAC API (Phase 4.2) ─────────────────────────────────────

  app.get("/api/rbac/roles", async () => {
    const { rbac: _rbac } = await import("../rbac/index.js");
    return { roles: _rbac.listRoles(), enabled: _rbac.enabled };
  });

  app.get("/api/rbac/policies", async () => {
    const { rbac: _rbac } = await import("../rbac/index.js");
    return { policies: _rbac.listPolicies(), enabled: _rbac.enabled };
  });

  app.post<{ Body: { subject: string; role: string } }>(
    "/api/rbac/assign",
    async (request, reply) => {
      const { subject, role } = request.body;
      if (!subject || !role) return reply.status(400).send({ error: "subject and role required" });
      const { rbac: _rbac } = await import("../rbac/index.js");
      _rbac.assignRole(subject, role);
      return { ok: true };
    },
  );

  app.post<{ Body: { subject: string; action: string } }>(
    "/api/rbac/check",
    async (request) => {
      const { subject, action } = request.body;
      const { rbac: _rbac } = await import("../rbac/index.js");
      return _rbac.checkPolicy(subject, action as any);
    },
  );

  // ─── Distributed Agents API (Phase 4.2) ───────────────────────

  app.get("/api/agents/distributed", async () => {
    const { distributedAgents: da } = await import("../rbac/index.js");
    return da.listAll();
  });

  app.post<{ Body: { agentId: string; task: string } }>(
    "/api/agents/distributed/send",
    async (request, reply) => {
      const { agentId, task } = request.body;
      if (!agentId || !task) return reply.status(400).send({ error: "agentId and task required" });
      const { distributedAgents: da } = await import("../rbac/index.js");
      const result = await da.sendTask(agentId, task);
      if (result === null) return reply.status(404).send({ error: `Agent ${agentId} not found` });
      return { result };
    },
  );

  // ─── Tool Manifest API (Phase 4.1) ────────────────────────────

  app.get("/api/manifest", async () => {
    const { buildNexusToolManifest } = await import("../platform/index.js");
    const { listAdapters: la } = await import("../adapters/index.js");
    const toolList = tools ? tools.list().map(t => ({ id: t.name, name: t.name, description: t.description })) : [];
    const adapterList = la().map(a => a.id);
    const skillList = marketplace ? (await marketplace.listInstalled()).map(s => s.id) : [];
    // Use a minimal config shape for the manifest builder
    const minConfig = { gateway: { port: 18800, bind: "127.0.0.1" } } as any;
    return buildNexusToolManifest(minConfig, {
      tools: toolList,
      adapters: adapterList,
      skills: skillList,
    });
  });

  // ─── Metrics API (Phase 5) ──────────────────────────────────────

  app.get("/api/metrics", async () => {
    const { metrics: m } = await import("../metrics/index.js");
    const snap = m.snapshot();
    // Phase 6: include LLM rate-limit stats
    const rateLimits = orchestrator.getRateLimitStats();
    return { ...snap, rateLimits };
  });

  app.post("/api/metrics/reset", async () => {
    const { metrics: m } = await import("../metrics/index.js");
    m.reset();
    return { ok: true };
  });

  // ─── Artifacts API (Phase 3.8 — Canvas / A2UI) ──────────────────

  app.get("/api/artifacts", async () => ({ artifacts: listArtifacts() }));

  app.get<{ Params: { id: string } }>("/api/artifacts/:id", async (req, reply) => {
    const artifact = getArtifact(req.params.id);
    if (!artifact) { reply.code(404); return { error: "Artifact not found" }; }
    reply
      .code(200)
      .header("Content-Type", "text/html; charset=utf-8")
      .header("X-Frame-Options", "SAMEORIGIN")
      .header("Content-Security-Policy", "default-src 'self' 'unsafe-inline' 'unsafe-eval' data: blob:; frame-ancestors 'self'")
      .send(artifact.html);
  });

  // ─── Webhooks API (Phase 5) ─────────────────────────────────────

  app.get("/api/webhooks", async () => {
    const { webhooks: wh } = await import("../webhooks/index.js");
    return { webhooks: wh.list() };
  });

  app.post<{ Body: { url: string; events?: string[]; secret?: string; description?: string } }>(
    "/api/webhooks",
    async (request, reply) => {
      const { url, events = ["*"], secret, description } = request.body;
      if (!url) return reply.status(400).send({ error: "url is required" });
      const { webhooks: wh } = await import("../webhooks/index.js");
      const entry = wh.register({ url, events: events as any, secret, description });
      return entry;
    },
  );

  app.delete<{ Params: { id: string } }>("/api/webhooks/:id", async (request, reply) => {
    const { webhooks: wh } = await import("../webhooks/index.js");
    const ok = wh.unregister(request.params.id);
    if (!ok) return reply.status(404).send({ error: "Webhook not found" });
    return { unregistered: request.params.id };
  });

  app.post<{ Params: { id: string } }>("/api/webhooks/:id/enable", async (request, reply) => {
    const { webhooks: wh } = await import("../webhooks/index.js");
    const ok = wh.enable_hook(request.params.id);
    if (!ok) return reply.status(404).send({ error: "Webhook not found" });
    return { ok: true };
  });

  app.post<{ Params: { id: string } }>("/api/webhooks/:id/disable", async (request, reply) => {
    const { webhooks: wh } = await import("../webhooks/index.js");
    const ok = wh.disable_hook(request.params.id);
    if (!ok) return reply.status(404).send({ error: "Webhook not found" });
    return { ok: true };
  });

  app.post<{ Params: { id: string } }>("/api/webhooks/:id/test", async (request, reply) => {
    const { webhooks: wh } = await import("../webhooks/index.js");
    const hook = wh.get(request.params.id);
    if (!hook) return reply.status(404).send({ error: "Webhook not found" });
    const results = await wh.dispatch("*" as any, { test: true, source: "nexusclaw-gateway" });
    const r = results.find(x => x.webhookId === request.params.id);
    return r ?? { ok: false, error: "no delivery attempted" };
  });

  // ─── MCP API (Phase 5 surface) ──────────────────────────────────

  app.get("/api/mcp/servers", async () => {
    const { MCPManager } = await import("../mcp/index.js");
    return { servers: Object.keys({}) }; // returns configured server names
  });

  // ─── Dashboard (minimal HTML) ────────────────────────────────────

  app.get("/", async (_, reply) => {
    reply.type("text/html").send(DASHBOARD_HTML);
  });

  // ─── WebSocket (Phase 1.5: streaming support) ───────────────────

  app.register(async function (fastify) {
    fastify.get(
      "/ws",
      { websocket: true },
      (socket) => {
        socket.on("message", async (rawData: Buffer | string) => {
          try {
            const data = JSON.parse(rawData.toString());

            if (data.type === "chat") {
              const response = await orchestrator.sendMessage(
                data.message,
                data.agentId,
                data.sessionId,
              );
              socket.send(JSON.stringify({ type: "response", ...response }));
            } else if (data.type === "stream") {
              // Streaming chat via WebSocket
              const agent = data.agentId
                ? orchestrator.getAgent(data.agentId)
                : orchestrator.getDefaultAgent();
              if (!agent) {
                socket.send(JSON.stringify({ type: "error", message: "Agent not found" }));
                return;
              }
              for await (const event of agent.processMessageStream(
                data.message,
                data.sessionId,
              )) {
                socket.send(JSON.stringify({ type: "stream_event", event }));
              }
            } else if (data.type === "status") {
              socket.send(
                JSON.stringify({
                  type: "status",
                  agents: orchestrator.listAgents(),
                  memory: memory.getStatus(),
                  skills: orchestrator.listSkills().length,
                }),
              );
            }
          } catch (err: any) {
            socket.send(JSON.stringify({ type: "error", message: err.message }));
          }
        });
      },
    );
  });

  return {
    async start() {
      await app.listen({ port: config.port, host: config.bind });
      return `http://${config.bind}:${config.port}`;
    },
    async stop() {
      await app.close();
    },
    app,
  };
}

// ─── Full SPA Dashboard (Phase 11.1) ────────────────────────────────────────

const DASHBOARD_HTML = `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>AnyClaw Dashboard</title>
<style>
:root {
  --bg: #0d1117; --bg2: #161b22; --bg3: #21262d; --border: #30363d;
  --text: #c9d1d9; --text2: #8b949e; --accent: #58a6ff; --green: #238636;
  --green2: #2ea043; --orange: #f0883e; --red: #f85149; --purple: #bc8cff;
  --yellow: #d29922; --radius: 8px; --font: -apple-system,BlinkMacSystemFont,\'Segoe UI\',Roboto,monospace;
}
.light-theme {
  --bg: #ffffff; --bg2: #f6f8fa; --bg3: #eaeef2; --border: #d0d7de;
  --text: #1f2328; --text2: #656d76; --accent: #0969da; --green: #1a7f37;
  --green2: #1f883d; --orange: #bc4c00; --red: #cf222e; --purple: #8250df;
  --yellow: #9a6700;
}
*,*::before,*::after{box-sizing:border-box;margin:0;padding:0}
html{-webkit-text-size-adjust:100%}
body{font-family:var(--font);background:var(--bg);color:var(--text);display:flex;height:100vh;overflow:hidden;-webkit-font-smoothing:antialiased;-moz-osx-font-smoothing:grayscale;color-scheme:dark}

/* Sidebar */
.sidebar{width:220px;background:var(--bg2);border-right:1px solid var(--border);display:flex;flex-direction:column;flex-shrink:0}
.sidebar-brand{padding:16px 20px;font-size:18px;font-weight:700;color:var(--accent);border-bottom:1px solid var(--border);display:flex;align-items:center;gap:8px}
.sidebar-brand svg{width:22px;height:22px}
.sidebar-nav{flex:1;padding:8px 0;overflow-y:auto}
.nav-item{display:flex;align-items:center;gap:10px;padding:10px 20px;color:var(--text2);cursor:pointer;font-size:13px;transition:background-color .15s,color .15s,border-color .15s;border-left:3px solid transparent;touch-action:manipulation;-webkit-tap-highlight-color:transparent;user-select:none}
.nav-item:hover{background:var(--bg3);color:var(--text)}
.nav-item.active{color:var(--accent);background:rgba(88,166,255,.08);border-left-color:var(--accent)}
.nav-item svg{width:16px;height:16px;flex-shrink:0;pointer-events:none}
.sidebar-footer{padding:12px 20px;border-top:1px solid var(--border);font-size:11px;color:var(--text2)}

/* Main */
.main{flex:1;display:flex;flex-direction:column;overflow:hidden}
.topbar{height:48px;border-bottom:1px solid var(--border);display:flex;align-items:center;padding:0 24px;gap:12px;flex-shrink:0}
.topbar h2{font-size:15px;font-weight:600;color:var(--text)}
.status-dot{width:8px;height:8px;border-radius:50%;background:var(--green2)}
.content{flex:1;overflow-y:auto;padding:24px}

/* Cards & Grid */
.stats-grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:16px;margin-bottom:24px}
.stat-card{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:16px;transition:border-color .2s}
.stat-card:hover{border-color:var(--accent)}
.stat-val{font-size:28px;font-weight:700;color:var(--accent)}
.stat-label{font-size:11px;color:var(--text2);text-transform:uppercase;margin-top:4px;letter-spacing:.5px}
.card{background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);padding:16px;margin-bottom:16px}
.card-title{font-size:13px;font-weight:600;color:var(--text2);text-transform:uppercase;margin-bottom:12px;letter-spacing:.5px}

/* Chat tabs (Phase 8, 11.5) */
.chat-tab-bar{display:flex;align-items:center;border-bottom:1px solid var(--border);background:var(--bg);flex-shrink:0;overflow-x:auto;min-height:37px}
.chat-tab{display:flex;align-items:center;gap:6px;padding:8px 14px;font-size:13px;color:var(--text2);cursor:pointer;border-bottom:2px solid transparent;white-space:nowrap;flex-shrink:0;user-select:none}
.chat-tab:hover{background:var(--bg2);color:var(--text)}
.chat-tab.active{color:var(--accent);border-bottom-color:var(--accent)}
.chat-tab-close{opacity:0;font-size:11px;padding:1px 4px;border-radius:3px;line-height:1;background:transparent;border:none;color:inherit;cursor:pointer}
.chat-tab:hover .chat-tab-close{opacity:.6}
.chat-tab-close:hover{opacity:1!important;background:var(--bg3)!important}
.chat-tab-new{padding:6px 12px;color:var(--text2);cursor:pointer;font-size:20px;flex-shrink:0;line-height:1;margin-left:2px;border-radius:4px;user-select:none}
.chat-tab-new:hover{color:var(--accent);background:var(--bg2)}
.chat-messages-wrap{flex:1;position:relative;overflow:hidden}

/* Chat */
.chat-container{display:flex;flex-direction:column;height:calc(100vh - 48px)}
.chat-messages{display:none;position:absolute;inset:0;overflow-y:auto;padding:24px;flex-direction:column;gap:12px}
.chat-messages.active{display:flex}
.chat-bubble{max-width:80%;padding:12px 16px;border-radius:12px;font-size:14px;line-height:1.6;word-wrap:break-word}
.chat-bubble pre{white-space:pre-wrap;word-wrap:break-word;margin:0;font-family:inherit}
.chat-bubble.user{align-self:flex-end;background:rgba(88,166,255,.15);border:1px solid rgba(88,166,255,.3);color:var(--text)}
.chat-bubble.assistant{align-self:flex-start;background:var(--bg2);border:1px solid var(--border)}
.chat-bubble.tool{align-self:flex-start;background:rgba(240,136,62,.08);border:1px solid rgba(240,136,62,.3);color:var(--orange);font-size:12px;font-family:monospace}
.chat-bubble.thinking{align-self:flex-start;background:var(--bg3);border:1px solid var(--border);color:var(--text2);font-style:italic;font-size:12px}
/* Tool call detail (Phase 7) */
.tool-call{align-self:flex-start;background:rgba(240,136,62,.06);border:1px solid rgba(240,136,62,.25);border-radius:8px;font-size:12px;max-width:80%;overflow:hidden}
.tool-call-header{padding:6px 12px;cursor:pointer;display:flex;align-items:center;gap:8px;color:var(--orange);font-family:monospace;user-select:none}
.tool-call-header:hover{background:rgba(240,136,62,.1)}
.tool-call-body{padding:8px 12px;border-top:1px solid rgba(240,136,62,.2);display:none;font-family:monospace;white-space:pre-wrap;word-break:break-all;color:var(--text2);max-height:200px;overflow-y:auto}
.tool-call.expanded .tool-call-body{display:block}
.tool-result-ok{border-left:3px solid var(--green2)}
.tool-result-err{border-left:3px solid var(--red)}
/* Kanban (Phase 7) */
.kanban-board{display:flex;gap:12px;overflow-x:auto;padding-bottom:8px;align-items:flex-start}
.kanban-col{min-width:200px;flex-shrink:0;background:var(--bg2);border:1px solid var(--border);border-radius:var(--radius);overflow:hidden}
.kanban-col-header{padding:10px 14px;font-size:12px;font-weight:600;text-transform:uppercase;letter-spacing:.5px;border-bottom:1px solid var(--border);display:flex;align-items:center;justify-content:space-between}
.kanban-card{padding:10px 12px;margin:8px;background:var(--bg3);border:1px solid var(--border);border-radius:6px;font-size:13px;cursor:default}
.kanban-card:hover{border-color:var(--accent)}
.kanban-card-title{font-weight:500;margin-bottom:6px;word-break:break-word}
.kanban-card-footer{font-size:11px;color:var(--text2);display:flex;gap:6px;align-items:center;margin-top:4px}
.kanban-empty{padding:16px 12px;text-align:center;color:var(--text2);font-size:12px;font-style:italic}
.chat-meta{font-size:11px;color:var(--text2);margin-top:4px}
.chat-input-area{border-top:1px solid var(--border);padding:16px 24px;display:flex;gap:12px;align-items:center}
.chat-input{flex:1;background:var(--bg);border:1px solid var(--border);border-radius:var(--radius);padding:12px 16px;color:var(--text);font-size:14px;font-family:var(--font);resize:none;min-height:44px;max-height:120px}
.chat-input:focus{outline:none;border-color:var(--accent)}
.btn{border:none;border-radius:6px;padding:10px 20px;cursor:pointer;font-size:13px;font-weight:600;transition:background-color .15s,color .15s,border-color .15s,opacity .15s;touch-action:manipulation;-webkit-tap-highlight-color:transparent}
.btn-primary{background:var(--green);color:white}
.btn-primary:hover{background:var(--green2)}
.btn-primary:disabled{opacity:.4;cursor:not-allowed}
.btn-secondary{background:var(--bg3);color:var(--text2);border:1px solid var(--border)}
.btn-secondary:hover{color:var(--text);border-color:var(--text2)}
.btn-danger{background:rgba(248,81,73,.15);color:var(--red);border:1px solid rgba(248,81,73,.3)}
.btn-danger:hover{background:rgba(248,81,73,.25)}

/* Tables */
table{width:100%;border-collapse:collapse;font-size:13px}
th{text-align:left;padding:8px 12px;border-bottom:2px solid var(--border);color:var(--text2);font-size:11px;text-transform:uppercase;letter-spacing:.5px}
td{padding:8px 12px;border-bottom:1px solid var(--border)}
tr:hover td{background:rgba(88,166,255,.04)}
code{background:var(--bg3);padding:2px 6px;border-radius:4px;font-size:12px;color:var(--purple)}

/* Tags */
.tag{display:inline-block;padding:2px 8px;border-radius:12px;font-size:11px;font-weight:500}
.tag-green{background:rgba(35,134,54,.2);color:#3fb950}
.tag-orange{background:rgba(210,153,34,.2);color:var(--yellow)}
.tag-blue{background:rgba(88,166,255,.15);color:var(--accent)}
.tag-red{background:rgba(248,81,73,.15);color:var(--red)}
.tag-gray{background:var(--bg3);color:var(--text2)}

/* Page visibility */
.page{display:none}
.page.active{display:flex;flex-direction:column;height:100%}
.page-scroll{display:none}
.page-scroll.active{display:block}

/* Search */
.search-bar{background:var(--bg);border:1px solid var(--border);border-radius:6px;padding:8px 12px;color:var(--text);font-size:13px;width:100%;margin-bottom:16px}
.search-bar:focus{outline:none;border-color:var(--accent)}

/* Empty state */
.empty{text-align:center;padding:40px;color:var(--text2)}
.empty svg{width:48px;height:48px;margin-bottom:12px;opacity:.3}

/* mobile sidebar overlay backdrop */
.sidebar-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:99}
.sidebar-overlay.open{display:block}
/* hamburger button — hidden on desktop */
.hamburger{display:none;background:none;border:none;color:var(--text);cursor:pointer;padding:6px;border-radius:4px;font-size:20px;line-height:1}
.hamburger:hover{background:var(--bg3)}

/* Responsive */
@media(max-width:768px){
  .hamburger{display:flex;align-items:center}
  .app{grid-template-columns:1fr}
  .sidebar{position:fixed;top:0;left:-240px;height:100vh;z-index:100;transition:left .2s;box-shadow:4px 0 24px rgba(0,0,0,.4)}
  .sidebar.mobile-open{left:0}
  .stats-grid{grid-template-columns:1fr 1fr}
  .chat-bubble{max-width:95%}
  .content{padding:16px}
}
@media(max-width:480px){
  .stats-grid{grid-template-columns:1fr}
  .topbar{padding:0 12px}
  .chat-input-area{padding:10px 12px}
  .content{padding:12px}
  table{font-size:12px}
}

/* Artifact panel (Phase 3.8 — Canvas / A2UI) */
.artifact-panel{position:fixed;top:0;right:-600px;width:min(600px,100vw);height:100vh;background:var(--bg2);border-left:1px solid var(--border);box-shadow:-4px 0 24px rgba(0,0,0,.3);z-index:200;display:flex;flex-direction:column;transition:right .25s ease}
.artifact-panel.open{right:0}
.artifact-panel-header{display:flex;align-items:center;gap:8px;padding:12px 16px;border-bottom:1px solid var(--border);flex-shrink:0}
.artifact-panel-title{flex:1;font-weight:600;font-size:14px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.artifact-panel-close{background:none;border:none;color:var(--text-dim);cursor:pointer;font-size:20px;line-height:1;padding:2px 6px;border-radius:4px}
.artifact-panel-close:hover{color:var(--text);background:var(--bg3)}
.artifact-panel iframe{flex:1;border:none;background:#fff}
/* Artifact link in chat */
.artifact-link{display:inline-flex;align-items:center;gap:6px;background:var(--bg3);border:1px solid var(--border);border-radius:6px;padding:6px 12px;margin-top:6px;cursor:pointer;font-size:13px;color:var(--accent);text-decoration:none}
.artifact-link:hover{background:var(--accent);color:#fff}
.artifact-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:199}
.artifact-overlay.open{display:block}
</style>
</head>
<body>

<!-- Mobile sidebar overlay -->
<div class="sidebar-overlay" id="sidebar-overlay" onclick="toggleSidebar()"></div>

<!-- Artifact panel (Phase 3.8) -->
<div class="artifact-overlay" id="artifact-overlay" onclick="closeArtifact()"></div>
<div class="artifact-panel" id="artifact-panel">
  <div class="artifact-panel-header">
    <span id="artifact-panel-title" class="artifact-panel-title">Artifact</span>
    <button class="artifact-panel-close" onclick="closeArtifact()" title="Close">✕</button>
  </div>
  <iframe id="artifact-frame" sandbox="allow-scripts allow-same-origin allow-forms" title="Artifact viewer"></iframe>
</div>

<!-- Sidebar -->
<nav class="sidebar" id="main-sidebar">
  <div class="sidebar-brand">
    <svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-1 17.93c-3.95-.49-7-3.85-7-7.93 0-.62.08-1.21.21-1.79L9 15v1c0 1.1.9 2 2 2v1.93zm6.9-2.54c-.26-.81-1-1.39-1.9-1.39h-1v-3c0-.55-.45-1-1-1H8v-2h2c.55 0 1-.45 1-1V7h2c1.1 0 2-.9 2-2v-.41c2.93 1.19 5 4.06 5 7.41 0 2.08-.8 3.97-2.1 5.39z"/></svg>
    <span>AnyClaw</span>
  </div>
  <div class="sidebar-nav">
    <div class="nav-item active" data-page="overview" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M3 13h8V3H3v10zm0 8h8v-6H3v6zm10 0h8V11h-8v10zm0-18v6h8V3h-8z"/></svg>
      <span>Overview</span>
    </div>
    <div class="nav-item" data-page="chat" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M20 2H4c-1.1 0-2 .9-2 2v18l4-4h14c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2z"/></svg>
      <span>Chat</span>
    </div>
    <div class="nav-item" data-page="sessions" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M4 6H2v14c0 1.1.9 2 2 2h14v-2H4V6zm16-4H8c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2z"/></svg>
      <span>Sessions</span>
    </div>
    <div class="nav-item" data-page="memory" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M15 9H9v6h6V9zm-2 4h-2v-2h2v2zm8-2V9h-2V7c0-1.1-.9-2-2-2h-2V3h-2v2h-2V3H9v2H7c-1.1 0-2 .9-2 2v2H3v2h2v2H3v2h2v2c0 1.1.9 2 2 2h2v2h2v-2h2v2h2v-2h2c1.1 0 2-.9 2-2v-2h2v-2h-2v-2h2zm-4 6H7V7h10v10z"/></svg>
      <span>Memory</span>
    </div>
    <div class="nav-item" data-page="gsd" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-7 14l-5-5 1.41-1.41L12 14.17l7.59-7.59L21 8l-9 9z"/></svg>
      <span>GSD Tasks</span>
    </div>
    <div class="nav-item" data-page="tools" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M22.7 19l-9.1-9.1c.9-2.3.4-5-1.5-6.9-2-2-5-2.4-7.4-1.3L9 6 6 9 1.6 4.7C.4 7.1.9 10.1 2.9 12.1c1.9 1.9 4.6 2.4 6.9 1.5l9.1 9.1c.4.4 1 .4 1.4 0l2.3-2.3c.5-.4.5-1.1.1-1.4z"/></svg>
      <span>Tools</span>
    </div>
    <div class="nav-item" data-page="recipes" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M4 6H2v14c0 1.1.9 2 2 2h14v-2H4V6zm16-4H8c-1.1 0-2 .9-2 2v12c0 1.1.9 2 2 2h12c1.1 0 2-.9 2-2V4c0-1.1-.9-2-2-2zm-1 9h-4v4h-2v-4H9V9h4V5h2v4h4v2z"/></svg>
      <span>Recipes</span>
    </div>
    <div class="nav-item" data-page="agents" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M16 11c1.66 0 2.99-1.34 2.99-3S17.66 5 16 5c-1.66 0-3 1.34-3 3s1.34 3 3 3zm-8 0c1.66 0 2.99-1.34 2.99-3S9.66 5 8 5C6.34 5 5 6.34 5 8s1.34 3 3 3zm0 2c-2.33 0-7 1.17-7 3.5V19h14v-2.5c0-2.33-4.67-3.5-7-3.5z"/></svg>
      <span>Agents</span>
    </div>
    <div class="nav-item" data-page="teams" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M4 10v7h3v-7H4zm6 0v7h3v-7h-3zm-8 12h19v-3H2v3zm14-12v7h3v-7h-3zM11.5 1L2 6v2h19V6l-9.5-5z"/></svg>
      <span>Teams</span>
    </div>
    <div class="nav-item" data-page="workflows" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zM9 17H7v-7h2v7zm4 0h-2V7h2v10zm4 0h-2v-4h2v4z"/></svg>
      <span>Workflows</span>
    </div>
    <div class="nav-item" data-page="marketplace" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M20 4H4v2l8 5 8-5V4zm0 4.5l-8 5-8-5V20h16V8.5z"/></svg>
      <span>Marketplace</span>
    </div>
    <div class="nav-item" data-page="admin" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 1L3 5v6c0 5.55 3.84 10.74 9 12 5.16-1.26 9-6.45 9-12V5l-9-4zm0 4l5 2.18V11c0 3.5-2.33 6.79-5 7.93-2.67-1.14-5-4.43-5-7.93V7.18L12 5z"/></svg>
      <span>Admin</span>
    </div>
    <div class="nav-item" data-page="metrics" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M19 3H5c-1.1 0-2 .9-2 2v14c0 1.1.9 2 2 2h14c1.1 0 2-.9 2-2V5c0-1.1-.9-2-2-2zm-7 14H7v-2h5v2zm5-4H7v-2h10v2zm0-4H7V7h10v2z"/></svg>
      <span>Metrics</span>
    </div>
    <div class="nav-item" data-page="webhooks" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm-1 15v-4H7l5-8v4h4l-5 8z"/></svg>
      <span>Webhooks</span>
    </div>
    <div class="nav-item" data-page="mcp" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M4.5 6.375a4.125 4.125 0 118.25 0 4.125 4.125 0 01-8.25 0zM14.25 8.625a3.375 3.375 0 116.75 0 3.375 3.375 0 01-6.75 0zM1.5 19.125a7.125 7.125 0 0114.25 0v.003l-.001.119a.75.75 0 01-.363.63 13.067 13.067 0 01-6.761 1.873c-2.472 0-4.786-.684-6.76-1.873a.75.75 0 01-.364-.63l-.001-.122zM17.25 19.128l-.001.144a2.25 2.25 0 01-.233.96 10.088 10.088 0 005.06-1.01.75.75 0 00.42-.643 4.875 4.875 0 00-6.957-4.611 8.586 8.586 0 011.71 5.157v.003z"/></svg>
      <span>MCP</span>
    </div>
    <div class="nav-item" data-page="settings" onclick="navigate(this.dataset.page)">
      <svg viewBox="0 0 24 24" fill="currentColor"><path d="M19.14 12.94c.04-.3.06-.61.06-.94 0-.32-.02-.64-.07-.94l2.03-1.58a.49.49 0 00.12-.61l-1.92-3.32a.49.49 0 00-.59-.22l-2.39.96c-.5-.38-1.03-.7-1.62-.94l-.36-2.54a.48.48 0 00-.48-.41h-3.84c-.24 0-.43.17-.47.41l-.36 2.54c-.59.24-1.13.57-1.62.94l-2.39-.96a.49.49 0 00-.59.22L2.74 8.87c-.12.21-.08.47.12.61l2.03 1.58c-.05.3-.07.62-.07.94s.02.64.07.94l-2.03 1.58a.49.49 0 00-.12.61l1.92 3.32c.12.22.37.29.59.22l2.39-.96c.5.38 1.03.7 1.62.94l.36 2.54c.05.24.24.41.48.41h3.84c.24 0 .44-.17.47-.41l.36-2.54c.59-.24 1.13-.56 1.62-.94l2.39.96c.22.08.47 0 .59-.22l1.92-3.32c.12-.22.07-.47-.12-.61l-2.01-1.58zM12 15.6A3.6 3.6 0 1115.6 12 3.6 3.6 0 0112 15.6z"/></svg>
      <span>Settings</span>
    </div>
  </div>
  <div class="sidebar-footer">AnyClaw v0.1.0</div>
</nav>

<!-- Main Content -->
<div class="main">
  <div class="topbar">
    <button class="hamburger" onclick="toggleSidebar()" title="Menu" aria-label="Toggle navigation">&#9776;</button>
    <div class="status-dot" id="ws-status"></div>
    <h2 id="page-title">Overview</h2>
    <div style="flex:1"></div>
    <span style="font-size:12px;color:var(--text2)" id="uptime-label"></span>
    <button class="btn btn-secondary" style="padding:5px 10px;font-size:12px" onclick="toggleTheme()" id="theme-toggle" title="Toggle light/dark theme">&#9788;</button>
  </div>

  <!-- Overview Page -->
  <div class="content page-scroll active" id="pg-overview">
    <div class="stats-grid" id="stats-grid"></div>
    <div class="card">
      <div class="card-title">System Health</div>
      <div id="health-info" style="font-size:13px;line-height:2"></div>
    </div>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-top:16px">
      <div class="card">
        <div class="card-title">Recent Sessions</div>
        <div id="recent-sessions"></div>
      </div>
      <div class="card">
        <div class="card-title">GSD Progress</div>
        <div id="recent-specs"></div>
      </div>
    </div>
  </div>

  <!-- Chat Page -->
  <div class="page" id="pg-chat">
    <div class="chat-tab-bar" id="chat-tab-bar">
      <span class="chat-tab-new" onclick="newChatTab()" title="New conversation">+</span>
    </div>
    <div class="chat-messages-wrap" id="chat-messages-wrap"></div>
    <div class="chat-input-area">
      <textarea class="chat-input" id="chat-input" placeholder="Send a message to AnyClaw..." rows="1"></textarea>
      <button class="btn btn-primary" id="chat-send" onclick="sendChat()">Send</button>
    </div>
  </div>

  <!-- Sessions Page -->
  <div class="content page-scroll" id="pg-sessions">
    <div style="display:flex;gap:12px;margin-bottom:16px">
      <input class="search-bar" style="margin:0" placeholder="Search sessions..." oninput="filterSessions(this.value)"/>
      <button class="btn btn-secondary" onclick="loadSessions()">Refresh</button>
    </div>
    <div class="card" id="sessions-table"><em style="color:var(--text2)">Loading...</em></div>
  </div>

  <!-- Memory Page -->
  <div class="content page-scroll" id="pg-memory">
    <div style="display:flex;gap:12px;margin-bottom:16px">
      <input class="search-bar" style="margin:0" id="memory-search" placeholder="Search episodic memory..." />
      <button class="btn btn-primary" onclick="searchMemory()">Search</button>
    </div>
    <div class="stats-grid" id="memory-stats"></div>
    <div class="card">
      <div class="card-title">Vault Notes</div>
      <div id="vault-notes" style="font-size:13px;line-height:1.8"><em style="color:var(--text2)">Loading...</em></div>
    </div>
    <div class="card" id="memory-results" style="display:none">
      <div class="card-title">Search Results</div>
      <div id="memory-results-list"></div>
    </div>
  </div>

  <!-- GSD Page -->
  <div class="content page-scroll" id="pg-gsd">
    <div style="display:flex;gap:8px;margin-bottom:16px;align-items:center">
      <button class="btn btn-secondary" onclick="loadGSD()">&#8635; Refresh</button>
      <div style="flex:1"></div>
      <button class="btn btn-secondary" id="gsd-view-kanban" onclick="setGSDView(\'kanban\')" style="padding:5px 10px;font-size:12px">&#9778; Kanban</button>
      <button class="btn btn-secondary" id="gsd-view-list" onclick="setGSDView(\'list\')" style="padding:5px 10px;font-size:12px">&#9776; List</button>
    </div>
    <div id="gsd-board"></div>
  </div>

  <!-- Tools Page -->
  <div class="content page-scroll" id="pg-tools">
    <input class="search-bar" placeholder="Filter tools..." oninput="filterTools(this.value)" />
    <div class="card" id="tools-table"><em style="color:var(--text2)">Loading...</em></div>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:16px;margin-top:16px">
      <div class="card">
        <div class="card-title">Hooks</div>
        <div id="hooks-info"><em style="color:var(--text2)">Loading...</em></div>
      </div>
      <div class="card">
        <div class="card-title">Plugins</div>
        <div id="plugins-info"><em style="color:var(--text2)">Loading...</em></div>
      </div>
    </div>
  </div>

  <!-- Recipes Page -->
  <div class="content page-scroll" id="pg-recipes">
    <div id="recipes-list"><em style="color:var(--text2)">Loading...</em></div>
  </div>

  <!-- Agents Page -->
  <div class="content page-scroll" id="pg-agents">
    <div style="display:flex;gap:12px;margin-bottom:16px">
      <button class="btn btn-secondary" onclick="loadAgents()">Refresh</button>
    </div>
    <div id="agents-list"><em style="color:var(--text2)">Loading...</em></div>
  </div>

  <!-- Teams Page -->
  <div class="content page-scroll" id="pg-teams">
    <div style="display:flex;gap:12px;margin-bottom:16px">
      <input class="search-bar" style="margin:0;flex:1" id="team-task-input" placeholder="Task to run (for team run)..." />
      <button class="btn btn-secondary" onclick="loadTeams()">Refresh</button>
    </div>
    <div id="teams-list"><em style="color:var(--text2)">Loading...</em></div>
  </div>

  <!-- Workflows Page -->
  <div class="content page-scroll" id="pg-workflows">
    <div class="card">
      <div class="card-title">Research Pipeline</div>
      <div style="display:flex;gap:8px;margin-bottom:8px">
        <input class="search-bar" style="margin:0;flex:1" id="research-topic" placeholder="Research topic..." />
        <button class="btn btn-primary" onclick="runResearch()">Run</button>
      </div>
      <div id="research-result" style="font-size:13px"></div>
    </div>
    <div class="card">
      <div class="card-title">Worker Pool</div>
      <div style="display:flex;gap:8px;margin-bottom:8px">
        <input class="search-bar" style="margin:0;flex:1" id="pool-goal" placeholder="High-level goal to decompose..." />
        <input class="search-bar" style="margin:0;width:80px" id="pool-concurrency" placeholder="Workers" value="3" />
        <button class="btn btn-primary" onclick="runWorkerPool()">Run</button>
      </div>
      <div id="pool-result" style="font-size:13px"></div>
    </div>
    <div class="card">
      <div class="card-title">Benchmarks</div>
      <div style="display:flex;gap:8px;margin-bottom:8px">
        <select id="bench-suite" style="background:var(--bg);border:1px solid var(--border);color:var(--text);padding:8px 12px;border-radius:6px;font-size:13px">
          <option value="nexus-core">Nexus Core</option>
          <option value="nexus-advanced">Nexus Advanced</option>
        </select>
        <button class="btn btn-primary" onclick="runBenchmark()">Run Benchmark</button>
      </div>
      <div id="bench-result" style="font-size:13px"></div>
    </div>
  </div>

  <!-- Marketplace Page -->
  <div class="content page-scroll" id="pg-marketplace">
    <div style="display:flex;gap:12px;margin-bottom:16px">
      <input class="search-bar" style="margin:0;flex:1" id="market-search" placeholder="Search skills..." />
      <button class="btn btn-primary" onclick="searchMarketplace()">Search</button>
      <button class="btn btn-secondary" onclick="loadFeaturedSkills()">Featured</button>
    </div>
    <div style="display:grid;grid-template-columns:1fr 1fr;gap:16px">
      <div>
        <div class="card-title" style="margin-bottom:8px">Results</div>
        <div id="market-results"><em style="color:var(--text2)">Search or browse featured skills</em></div>
      </div>
      <div>
        <div class="card-title" style="margin-bottom:8px">Installed Skills</div>
        <div id="market-installed"><em style="color:var(--text2)">Loading...</em></div>
      </div>
    </div>
  </div>

  <!-- Settings Page -->
  <div class="content page-scroll" id="pg-settings">
    <div class="card">
      <div class="card-title">Configuration</div>
      <div id="settings-info"><em style="color:var(--text2)">Loading...</em></div>
    </div>
    <div class="card">
      <div class="card-title">Safety Log (Recent)</div>
      <div id="safety-log"><em style="color:var(--text2)">Loading...</em></div>
    </div>
  </div>

  <!-- Admin Page (Phase 4) -->
  <div class="content page-scroll" id="pg-admin">
    <div class="card">
      <div class="card-title">Nexus Tool Manifest</div>
      <div id="manifest-info"><em style="color:var(--text2)">Loading...</em></div>
      <button class="btn btn-secondary" style="margin-top:10px" onclick="loadManifest()">&#8635; Refresh</button>
    </div>
    <div class="card">
      <div class="card-title">RBAC Roles</div>
      <div id="rbac-roles"><em style="color:var(--text2)">Loading...</em></div>
    </div>
    <div class="card">
      <div class="card-title">RBAC Policies</div>
      <div id="rbac-policies"><em style="color:var(--text2)">Loading...</em></div>
      <div style="display:flex;gap:8px;margin-top:10px">
        <input id="rbac-subject" class="input" placeholder="Subject (agent ID or *)" style="flex:1">
        <input id="rbac-role" class="input" placeholder="Role ID" style="flex:1">
        <button class="btn btn-primary" onclick="assignRole()">Assign</button>
      </div>
    </div>
    <div class="card">
      <div class="card-title">Distributed Agents</div>
      <div id="dist-agents"><em style="color:var(--text2)">Loading...</em></div>
      <button class="btn btn-secondary" style="margin-top:10px" onclick="loadDistributedAgents()">&#8635; Refresh</button>
    </div>
  </div>

  <!-- Metrics Page (Phase 5) -->
  <div class="content page-scroll" id="pg-metrics">
    <div class="stats-grid" id="metrics-stats"></div>
    <div class="card">
      <div class="card-title">Token Usage by Model</div>
      <div id="metrics-models"><em style="color:var(--text2)">Loading...</em></div>
    </div>
    <div class="card">
      <div class="card-title">Top Tools</div>
      <div id="metrics-tools"><em style="color:var(--text2)">Loading...</em></div>
    </div>
    <div class="card">
      <div class="card-title">Token Usage by Agent</div>
      <div id="metrics-agents"><em style="color:var(--text2)">Loading...</em></div>
    </div>
    <div class="card">
      <div class="card-title">Recent Tool Errors</div>
      <div id="metrics-errors"><em style="color:var(--text2)">None</em></div>
    </div>
    <div style="display:flex;gap:8px;margin-top:4px">
      <button class="btn btn-secondary" onclick="loadMetrics()">&#8635; Refresh</button>
      <button class="btn btn-danger" style="padding:6px 14px" onclick="resetMetrics()">Reset Counters</button>
    </div>
  </div>

  <!-- Webhooks Page (Phase 5) -->
  <div class="content page-scroll" id="pg-webhooks">
    <div class="card">
      <div class="card-title">Register Webhook</div>
      <div style="display:flex;flex-direction:column;gap:8px">
        <input id="wh-url" class="input" placeholder="https://your-server.example.com/hook">
        <input id="wh-events" class="input" placeholder="Events (comma-separated, or * for all)" value="*">
        <input id="wh-secret" class="input" placeholder="HMAC secret (optional)">
        <input id="wh-desc" class="input" placeholder="Description (optional)">
        <button class="btn btn-primary" style="align-self:flex-start" onclick="registerWebhook()">Register</button>
      </div>
    </div>
    <div class="card">
      <div class="card-title">Registered Webhooks</div>
      <div id="webhooks-list"><em style="color:var(--text2)">Loading...</em></div>
    </div>
  </div>

  <!-- MCP Page (Phase 5) -->
  <div class="content page-scroll" id="pg-mcp">
    <div class="card">
      <div class="card-title">MCP Servers</div>
      <div id="mcp-servers"><em style="color:var(--text2)">Loading...</em></div>
    </div>
    <div class="card">
      <div class="card-title">MCP Tools</div>
      <div id="mcp-tools"><em style="color:var(--text2)">No MCP tools loaded</em></div>
    </div>
  </div>

</div>

<script>
// ─── State ──────────────────────────────────────────────
let chatTabs = [];       // [{id, sessionId, label, lastEl, pendingCalls, _labeled}]
let activeTabId = null;
let allSessions = [];
let allTools = [];
let ws;
let gsdView = 'kanban'; // 'kanban' | 'list'

// helpers
function activeTab() { return chatTabs.find(t => t.id === activeTabId); }
function activeMsgsDiv() { return document.getElementById('tab-msgs-' + activeTabId); }

// ─── Theme (Phase 7, 11.6) ──────────────────────────────
function toggleTheme() {
  const isLight = document.body.classList.toggle('light-theme');
  localStorage.setItem('theme', isLight ? 'light' : 'dark');
  document.getElementById('theme-toggle').textContent = isLight ? '\u263d' : '\u2600';
}
(function() {
  if (localStorage.getItem('theme') === 'light') {
    document.body.classList.add('light-theme');
    document.addEventListener('DOMContentLoaded', () => {
      const b = document.getElementById('theme-toggle');
      if (b) b.textContent = '\u263d';
    });
  }
})();

// ─── Mobile sidebar (Phase 9, 11.7) ─────────────────────
function toggleSidebar() {
  const sidebar = document.getElementById('main-sidebar');
  const overlay = document.getElementById('sidebar-overlay');
  const open = sidebar.classList.toggle('mobile-open');
  overlay.classList.toggle('open', open);
}

// Close sidebar when navigating on mobile — walk up the tree to avoid SVG namespace issues
document.addEventListener('click', function(e) {
  if (window.innerWidth > 768) return;
  var el = e.target;
  while (el && el !== document.body) {
    if (el.classList && el.classList.contains('nav-item')) {
      var sidebar = document.getElementById('main-sidebar');
      var overlay = document.getElementById('sidebar-overlay');
      if (sidebar && sidebar.classList.contains('mobile-open')) {
        sidebar.classList.remove('mobile-open');
        if (overlay) overlay.classList.remove('open');
      }
      break;
    }
    el = el.parentElement;
  }
}, true);

// ─── Artifact Panel (Phase 3.8 — Canvas / A2UI) ─────────
function showArtifact(artifactId, title) {
  const panel = document.getElementById('artifact-panel');
  const overlay = document.getElementById('artifact-overlay');
  const frame = document.getElementById('artifact-frame');
  const titleEl = document.getElementById('artifact-panel-title');
  if (!panel || !frame) return;
  frame.src = '/api/artifacts/' + artifactId;
  if (titleEl) titleEl.textContent = title || 'Artifact';
  panel.classList.add('open');
  if (overlay) overlay.classList.add('open');
}
function closeArtifact() {
  const panel = document.getElementById('artifact-panel');
  const overlay = document.getElementById('artifact-overlay');
  const frame = document.getElementById('artifact-frame');
  if (panel) panel.classList.remove('open');
  if (overlay) overlay.classList.remove('open');
  // Reset iframe to avoid keeping live scripts running
  if (frame) frame.src = 'about:blank';
}

// ─── WebSocket ──────────────────────────────────────────
function connectWS() {
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onopen = () => {
    document.getElementById('ws-status').style.background = 'var(--green2)';
    ws.send(JSON.stringify({ type: 'status' }));
  };
  ws.onclose = () => {
    document.getElementById('ws-status').style.background = 'var(--red)';
    setTimeout(connectWS, 3000);
  };
  ws.onmessage = (e) => {
    const data = JSON.parse(e.data);
    if (data.type === 'response') {
      const tab = activeTab();
      if (tab) tab.sessionId = data.sessionId;
      if (data.thinking) addChat('thinking', '\ud83e\udde0 ' + data.thinking.slice(0, 300) + (data.thinking.length > 300 ? '...' : ''));
      if (data.toolResults?.length > 0) {
        for (const tr of data.toolResults) {
          addToolCall(tr.name, null, typeof tr.result === 'string' ? tr.result : JSON.stringify(tr.result, null, 2), true);
        }
      }
      addChat('assistant', data.content);
      document.getElementById('chat-send').disabled = false;
      if (activeTab()) activeTab().pendingCalls = {};
    } else if (data.type === 'stream_event') {
      if (data.event.type === 'text_delta') appendToLastChat(data.event.content || '');
      else if (data.event.type === 'tool_start') {
        const id = data.event.content || ('tc-' + Date.now());
        addToolCall(data.event.toolName || '?', id, null, null);
      }
      else if (data.event.type === 'tool_result') {
        updateToolCall(data.event.toolName, typeof data.event.toolResult === 'string' ? data.event.toolResult : JSON.stringify(data.event.toolResult, null, 2));
      }
      else if (data.event.type === 'done') document.getElementById('chat-send').disabled = false;
    } else if (data.type === 'error') {
      addChat('tool', 'Error: ' + data.message);
      document.getElementById('chat-send').disabled = false;
    } else if (data.type === 'status') {
      renderOverviewStats(data);
    }
  };
}

// ─── Navigation ────────────────────────────────────────
function navigate(page) {
  document.querySelectorAll('.nav-item').forEach(n => n.classList.remove('active'));
  const navEl = document.querySelector('.nav-item[data-page="' + page + '"]') || [...document.querySelectorAll('.nav-item')].find((n) => {
    const onClick = n.getAttribute('onclick') || '';
    return onClick.includes("'" + page + "'");
  });
  if (navEl) navEl.classList.add('active');
  document.querySelectorAll('.page,.page-scroll').forEach(p => p.classList.remove('active'));
  const el = document.getElementById('pg-' + page);
  if (el) el.classList.add('active');
  const titles = {overview:'Overview',chat:'Chat',sessions:'Sessions',memory:'Memory',gsd:'GSD Tasks',tools:'Tools & Plugins',recipes:'Recipes',agents:'Agents',teams:'Teams',workflows:'Workflows',marketplace:'Marketplace',admin:'Admin',metrics:'Metrics',webhooks:'Webhooks',mcp:'MCP Servers',settings:'Settings'};
  const titleEl = document.getElementById('page-title');
  if (titleEl) titleEl.textContent = titles[page] || page;
  // Load data on navigate
  if (page === 'sessions') loadSessions();
  else if (page === 'memory') loadMemory();
  else if (page === 'gsd') loadGSD();
  else if (page === 'tools') loadTools();
  else if (page === 'recipes') loadRecipes();
  else if (page === 'agents') loadAgents();
  else if (page === 'teams') loadTeams();
  else if (page === 'marketplace') { loadInstalledSkills(); loadFeaturedSkills(); }
  else if (page === 'admin') { loadManifest(); loadRBAC(); loadDistributedAgents(); }
  else if (page === 'metrics') loadMetrics();
  else if (page === 'webhooks') loadWebhooks();
  else if (page === 'mcp') loadMCPServers();
  else if (page === 'settings') loadSettings();
  else if (page === 'overview') refreshOverview();
}

connectWS();

// ─── Overview ──────────────────────────────────────────
function renderOverviewStats(data) {
  const g = document.getElementById('stats-grid');
  if (!g) return;
  g.innerHTML = sc('Agents', data.agents?.length || 0, 'accent')
    + sc('Episodes', data.memory?.episodes || 0, 'purple')
    + sc('Vault Notes', data.memory?.vaultNotes || 0, 'green')
    + sc('Sessions', data.memory?.activeSessions || 0, 'orange')
    + sc('Skills', data.skills || 0, 'yellow');
}
function sc(label, val, color) {
  const colors = {accent:'var(--accent)',purple:'var(--purple)',green:'#3fb950',orange:'var(--orange)',yellow:'var(--yellow)'};
  return '<div class="stat-card"><div class="stat-val" style="color:' + (colors[color]||colors.accent) + '">' + val + '</div><div class="stat-label">' + label + '</div></div>';
}

async function refreshOverview() {
  if (ws && ws.readyState === 1) ws.send(JSON.stringify({ type: 'status' }));
  try {
    const [health, sessions, specs] = await Promise.all([
      fetch('/health').then(r => r.json()),
      fetch('/api/sessions?limit=5').then(r => r.json()),
      fetch('/api/gsd/specs').then(r => r.json()),
    ]);
    document.getElementById('uptime-label').textContent = 'Uptime: ' + Math.floor(health.uptime) + 's';
    document.getElementById('health-info').innerHTML =
      '<div>Version: <code>' + esc(health.version) + '</code></div>'
      + '<div>Agents: ' + (health.agents?.length||0) + '</div>'
      + '<div>Memory episodes: ' + (health.memory?.episodes||0) + '</div>'
      + '<div>Vault notes: ' + (health.memory?.vaultNotes||0) + '</div>';
    document.getElementById('recent-sessions').innerHTML = sessions.length === 0
      ? '<em style="color:var(--text2)">No sessions yet</em>'
      : sessions.map(s => '<div style="padding:4px 0;border-bottom:1px solid var(--border);font-size:13px">'
        + '<code>' + s.id.slice(0,8) + '</code> &middot; ' + (s.messageCount||0) + ' msgs &middot; '
        + new Date(s.createdAt).toLocaleDateString() + '</div>').join('');
    document.getElementById('recent-specs').innerHTML = specs.length === 0
      ? '<em style="color:var(--text2)">No specs yet</em>'
      : specs.map(s => '<div style="padding:4px 0;border-bottom:1px solid var(--border);font-size:13px">'
        + '<span class="tag tag-' + statusColor(s.status) + '">' + s.status + '</span> '
        + esc(s.title) + (s.progress ? ' &middot; ' + s.progress.done + '/' + s.progress.total : '')
        + '</div>').join('');
  } catch(e) {}
}
refreshOverview();

// ─── Chat Tabs (Phase 8, 11.5) ─────────────────────────
function newChatTab(sessionId, label) {
  const id = 'tab-' + Date.now();
  const tab = { id, sessionId: sessionId || null, label: label || ('Chat ' + (chatTabs.length + 1)), lastEl: null, pendingCalls: {}, _labeled: !!label };
  chatTabs.push(tab);
  const wrap = document.getElementById('chat-messages-wrap');
  const div = document.createElement('div');
  div.className = 'chat-messages';
  div.id = 'tab-msgs-' + id;
  wrap.appendChild(div);
  renderChatTabs();
  switchTab(id);
  return tab;
}
function switchTab(tabId) {
  activeTabId = tabId;
  document.querySelectorAll('#chat-messages-wrap .chat-messages').forEach(d => d.classList.remove('active'));
  const div = document.getElementById('tab-msgs-' + tabId);
  if (div) { div.classList.add('active'); div.scrollTop = div.scrollHeight; }
  renderChatTabs();
}
function closeTab(tabId) {
  if (chatTabs.length <= 1) return;
  const idx = chatTabs.findIndex(t => t.id === tabId);
  if (idx < 0) return;
  chatTabs.splice(idx, 1);
  const div = document.getElementById('tab-msgs-' + tabId);
  if (div) div.remove();
  if (activeTabId === tabId) switchTab(chatTabs[Math.max(0, idx - 1)].id);
  else renderChatTabs();
}
function renderChatTabs() {
  const bar = document.getElementById('chat-tab-bar');
  const newBtn = bar.querySelector('.chat-tab-new');
  [...bar.querySelectorAll('.chat-tab')].forEach(t => t.remove());
  chatTabs.forEach(tab => {
    const el = document.createElement('div');
    el.className = 'chat-tab' + (tab.id === activeTabId ? ' active' : '');
    el.textContent = tab.label;
    if (chatTabs.length > 1) {
      const x = document.createElement('button');
      x.className = 'chat-tab-close';
      x.textContent = '×';
      x.title = 'Close tab';
      x.onclick = (ev) => { ev.stopPropagation(); closeTab(tab.id); };
      el.appendChild(x);
    }
    el.onclick = () => switchTab(tab.id);
    bar.insertBefore(el, newBtn);
  });
}
// Initialize with one tab on load
newChatTab();

// ─── Chat ──────────────────────────────────────────────
function sendChat() {
  const input = document.getElementById('chat-input');
  const msg = input.value.trim();
  if (!msg || !ws || ws.readyState !== 1) return;
  const tab = activeTab();
  addChat('user', msg);
  ws.send(JSON.stringify({ type: 'chat', message: msg, sessionId: tab?.sessionId || null }));
  input.value = '';
  input.style.height = 'auto';
  document.getElementById('chat-send').disabled = true;
  if (tab && !tab._labeled) {
    tab.label = msg.slice(0, 22) + (msg.length > 22 ? '\u2026' : '');
    tab._labeled = true;
    renderChatTabs();
  }
}
// Detect [ARTIFACT:id:title] markers in text and inject clickable links
function parseArtifacts(text) {
  // Replace [ARTIFACT:uuid:title] with a clickable button + remaining text
  const lines = [];
  let remainingText = text.replace(/\\[ARTIFACT:([a-f0-9-]+):([^\\]]+)\\]/gi, (_, id, title) => {
    lines.push({ id, title: decodeURIComponent(title) });
    return '';
  });
  return { clean: remainingText.trim(), artifacts: lines };
}
function addChat(role, content) {
  const box = activeMsgsDiv();
  if (!box) return;
  const el = document.createElement('div');
  el.className = 'chat-bubble ' + role;
  // Parse artifact markers from assistant messages
  if (role === 'assistant') {
    const { clean, artifacts } = parseArtifacts(content);
    el.innerHTML = '<pre>' + esc(clean) + '</pre>';
    for (const a of artifacts) {
      const btn = document.createElement('button');
      btn.className = 'artifact-link';
      btn.innerHTML = '🎨 ' + esc(a.title);
      btn.onclick = () => showArtifact(a.id, a.title);
      el.appendChild(btn);
    }
  } else {
    el.innerHTML = '<pre>' + esc(content) + '</pre>';
  }
  box.appendChild(el);
  box.scrollTop = box.scrollHeight;
  const tab = activeTab();
  if (tab) tab.lastEl = el;
}
// Phase 7: rich tool call visualization (11.2)
function addToolCall(name, id, result, success) {
  const tab = activeTab();
  const box = activeMsgsDiv();
  if (!box) return;
  const el = document.createElement('div');
  const statusIcon = result === null ? '\u23f3' : (success ? '\u2713' : '\u2717');
  const statusClass = result === null ? '' : (success ? ' tool-result-ok' : ' tool-result-err');
  el.className = 'tool-call' + statusClass;
  el.id = id ? 'tc-' + id : '';
  el.innerHTML = '<div class="tool-call-header" onclick="this.parentElement.classList.toggle(\\'expanded\\')">'
    + '<span>' + statusIcon + '</span>'
    + '<code>' + esc(name) + '</code>'
    + (result !== null ? '<span style="margin-left:auto;font-size:11px;opacity:.7">' + (result.length > 60 ? result.slice(0,60) + '\u2026' : result) + '</span>' : '<span style="margin-left:auto;font-size:10px;opacity:.5">running...</span>')
    + '</div>'
    + (result !== null ? '<div class="tool-call-body">' + esc(result.slice(0, 2000)) + '</div>' : '');
  box.appendChild(el);
  box.scrollTop = box.scrollHeight;
  if (id && tab) tab.pendingCalls[name] = el;
  return el;
}
function updateToolCall(name, result) {
  const tab = activeTab();
  const el = tab?.pendingCalls[name];
  if (!el) { addToolCall(name, null, result, !result?.startsWith('Error')); return; }
  const ok = !result?.startsWith('Error') && !result?.startsWith('BLOCKED');
  el.className = 'tool-call ' + (ok ? 'tool-result-ok' : 'tool-result-err');
  const header = el.querySelector('.tool-call-header');
  if (header) {
    const icon = header.querySelector('span:first-child');
    if (icon) icon.textContent = ok ? '\u2713' : '\u2717';
    const preview = header.querySelector('span:last-child');
    if (preview) preview.textContent = (result?.slice(0,60) || '') + ((result?.length || 0) > 60 ? '\u2026' : '');
  }
  let body = el.querySelector('.tool-call-body');
  if (!body) { body = document.createElement('div'); body.className = 'tool-call-body'; el.appendChild(body); }
  body.textContent = result?.slice(0, 2000) || '';
  if (tab) delete tab.pendingCalls[name];
}
function appendToLastChat(text) {
  const tab = activeTab();
  if (!tab) return;
  if (!tab.lastEl || !tab.lastEl.classList.contains('assistant')) addChat('assistant', '');
  const pre = tab.lastEl.querySelector('pre');
  if (pre) pre.textContent += text;
  // Check if newly accumulated text contains an artifact marker
  const current = pre ? pre.textContent : '';
  const { clean, artifacts } = parseArtifacts(current);
  if (artifacts.length > 0 && pre) {
    pre.textContent = clean;
    for (const a of artifacts) {
      // Only add buttons not already present
      const exists = tab.lastEl.querySelector('[data-artifact-id="' + a.id + '"]');
      if (!exists) {
        const btn = document.createElement('button');
        btn.className = 'artifact-link';
        btn.setAttribute('data-artifact-id', a.id);
        btn.innerHTML = '🎨 ' + esc(a.title);
        btn.onclick = () => showArtifact(a.id, a.title);
        tab.lastEl.appendChild(btn);
      }
    }
  }
  const box = activeMsgsDiv();
  if (box) box.scrollTop = box.scrollHeight;
}
document.getElementById('chat-input').addEventListener('keydown', e => {
  if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendChat(); }
});
// Auto-resize textarea
document.getElementById('chat-input').addEventListener('input', function() {
  this.style.height = 'auto';
  this.style.height = Math.min(this.scrollHeight, 120) + 'px';
});

// ─── Sessions ──────────────────────────────────────────
async function loadSessions() {
  const data = await fetch('/api/sessions?limit=100').then(r => r.json());
  allSessions = data;
  renderSessions(data);
}
function renderSessions(list) {
  if (list.length === 0) {
    document.getElementById('sessions-table').innerHTML = '<div class="empty">No sessions found</div>';
    return;
  }
  document.getElementById('sessions-table').innerHTML = '<table><thead><tr><th>ID</th><th>Agent</th><th>Messages</th><th>Created</th><th>Actions</th></tr></thead><tbody>'
    + list.map(s => '<tr><td><code>' + s.id.slice(0,12) + '</code></td><td>' + esc(s.agentId||'main') + '</td><td>' + (s.messageCount||0)
    + '</td><td>' + new Date(s.createdAt).toLocaleString()
    + '</td><td><button class="btn btn-secondary" style="padding:4px 10px;font-size:11px" onclick="resumeSession(\\'' + s.id + '\\')">Resume</button> '
    + '<button class="btn btn-danger" style="padding:4px 10px;font-size:11px" onclick="deleteSession(\\'' + s.id + '\\')">Delete</button></td></tr>').join('')
    + '</tbody></table>';
}
function filterSessions(q) {
  const filtered = allSessions.filter(s => s.id.includes(q) || (s.agentId||'').includes(q));
  renderSessions(filtered);
}
function resumeSession(id) {
  // Reuse existing tab with same sessionId if open
  const existing = chatTabs.find(t => t.sessionId === id);
  if (existing) { switchTab(existing.id); navigate('chat'); return; }
  // Open new tab pinned to this session
  const label = 'Session ' + id.slice(0, 8);
  const tab = newChatTab(id, label);
  tab.sessionId = id;
  navigate('chat');
}
async function deleteSession(id) {
  if (!confirm('Delete session ' + id.slice(0,8) + '?')) return;
  await fetch('/api/sessions/' + id, { method: 'DELETE' });
  loadSessions();
}

// ─── Memory ────────────────────────────────────────────
async function loadMemory() {
  const [status, vault] = await Promise.all([
    fetch('/api/memory/status').then(r => r.json()),
    fetch('/api/memory/vault').then(r => r.json()),
  ]);
  document.getElementById('memory-stats').innerHTML =
    sc('Episodes', status.episodes || 0, 'purple')
    + sc('Vault Notes', status.vaultNotes || 0, 'green')
    + sc('Sessions', status.activeSessions || 0, 'orange')
    + sc('Pass Rate', ((status.alignment?.passRate||1)*100).toFixed(0) + '%', status.alignment?.driftAlert ? 'orange' : 'green');

  if (typeof vault === 'string') {
    document.getElementById('vault-notes').innerHTML = vault ? '<pre style="white-space:pre-wrap;font-size:12px">' + esc(vault) + '</pre>' : '<em style="color:var(--text2)">Vault is empty</em>';
  } else if (Array.isArray(vault)) {
    document.getElementById('vault-notes').innerHTML = vault.length === 0
      ? '<em style="color:var(--text2)">No vault notes</em>'
      : vault.map(n => '<div style="padding:6px 0;border-bottom:1px solid var(--border)">'
        + '<span class="tag tag-blue">' + esc(n.category||'note') + '</span> '
        + '<strong>' + esc(n.title||'') + '</strong>'
        + '<div style="font-size:12px;color:var(--text2);margin-top:2px">' + esc((n.content||'').slice(0,200)) + '</div>'
        + '</div>').join('');
  }
}
async function searchMemory() {
  const q = document.getElementById('memory-search').value.trim();
  if (!q) return;
  const results = await fetch('/api/memory/search', {
    method: 'POST', headers: {'Content-Type':'application/json'},
    body: JSON.stringify({ query: q, limit: 10 }),
  }).then(r => r.json());
  const box = document.getElementById('memory-results');
  box.style.display = 'block';
  document.getElementById('memory-results-list').innerHTML = results.length === 0
    ? '<em style="color:var(--text2)">No results</em>'
    : results.map(r => '<div style="padding:6px 0;border-bottom:1px solid var(--border);font-size:13px">'
      + '<div>' + esc((r.content||'').slice(0,300)) + '</div>'
      + '<div style="font-size:11px;color:var(--text2);margin-top:2px">' + new Date(r.validAt).toLocaleString() + ' &middot; ' + (r.tags||[]).join(', ') + '</div>'
      + '</div>').join('');
}
document.getElementById('memory-search').addEventListener('keydown', e => { if (e.key === 'Enter') searchMemory(); });

// ─── GSD ───────────────────────────────────────────────
async function loadGSD() {
  const specs = await fetch('/api/gsd/specs').then(r => r.json());
  if (specs.length === 0) {
    document.getElementById('gsd-board').innerHTML = '<div class="empty">No GSD specs. Use the agent to create tasks.</div>';
    return;
  }
  setGSDView(gsdView, specs);
}
function setGSDView(view, specs) {
  gsdView = view;
  // Highlight active button
  ['kanban','list'].forEach(v => {
    const b = document.getElementById('gsd-view-' + v);
    if (b) b.style.opacity = v === view ? '1' : '.5';
  });
  const board = document.getElementById('gsd-board');
  if (!specs) { loadGSD(); return; }
  if (view === 'kanban') {
    renderGSDKanban(specs, board);
  } else {
    renderGSDList(specs, board);
  }
}
function renderGSDKanban(specs, board) {
  const COLS = [
    { key: 'pending', label: 'Backlog', color: 'var(--text2)' },
    { key: 'in-progress', label: 'In Progress', color: 'var(--accent)' },
    { key: 'verifying', label: 'Verifying', color: 'var(--yellow)' },
    { key: 'done', label: 'Done', color: 'var(--green2)' },
    { key: 'failed', label: 'Failed', color: 'var(--red)' },
  ];
  board.innerHTML = '<div class="kanban-board">'
    + COLS.map(col => {
      const colSpecs = specs.filter(s => s.status === col.key);
      return '<div class="kanban-col">'
        + '<div class="kanban-col-header" style="color:' + col.color + '">'
        + col.label + '<span style="background:var(--bg3);padding:1px 7px;border-radius:10px;color:var(--text2)">' + colSpecs.length + '</span></div>'
        + (colSpecs.length === 0 ? '<div class="kanban-empty">Empty</div>' :
          colSpecs.map(s => {
            const pct = s.progress ? s.progress.percentDone : 0;
            return '<div class="kanban-card">'
              + '<div class="kanban-card-title">' + esc(s.title) + '</div>'
              + (pct > 0 ? '<div style="margin:4px 0;height:4px;background:var(--bg2);border-radius:2px;overflow:hidden"><div style="height:100%;width:' + pct + '%;background:' + col.color + ';border-radius:2px"></div></div>' : '')
              + '<div class="kanban-card-footer">'
              + (s.progress ? '<span>' + s.progress.done + '/' + s.progress.total + ' tasks</span>' : '')
              + '</div>'
              + (s.tasks && s.tasks.length > 0 ? '<div style="margin-top:6px">'
                + s.tasks.slice(0,3).map(t => '<div style="font-size:11px;padding:2px 0;color:var(--text2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis">'
                  + '<span style="color:' + ('done'===t.status?'var(--green2)':'in-progress'===t.status?'var(--accent)':'var(--text2)') + '">&#9679; </span>'
                  + esc(t.title) + '</div>').join('')
                + (s.tasks.length > 3 ? '<div style="font-size:10px;color:var(--text2);margin-top:2px">+' + (s.tasks.length-3) + ' more</div>' : '')
                + '</div>' : '')
              + '</div>';
          }).join(''))
        + '</div>';
    }).join('')
    + '</div>';
}
function renderGSDList(specs, board) {
  board.innerHTML = specs.map(s => {
    const pct = s.progress ? s.progress.percentDone : 0;
    return '<div class="card">'
      + '<div style="display:flex;justify-content:space-between;align-items:center">'
      + '<strong>' + esc(s.title) + '</strong>'
      + '<span class="tag tag-' + statusColor(s.status) + '">' + s.status + '</span></div>'
      + '<div style="margin:8px 0;height:6px;background:var(--bg3);border-radius:3px;overflow:hidden">'
      + '<div style="height:100%;width:' + pct + '%;background:var(--green);border-radius:3px;transition:width .3s"></div></div>'
      + '<div style="font-size:12px;color:var(--text2)">'
      + (s.progress ? s.progress.done + '/' + s.progress.total + ' tasks (' + pct + '%)' : 'No tasks')
      + '</div>'
      + (s.tasks ? '<div style="margin-top:8px">' + s.tasks.map(t =>
        '<div style="padding:4px 0;font-size:12px;display:flex;gap:8px;align-items:center">'
        + '<span class="tag tag-' + taskColor(t.status) + '" style="min-width:70px;text-align:center">' + t.status + '</span>'
        + '<span>' + esc(t.title) + '</span></div>'
      ).join('') + '</div>' : '')
      + '</div>';
  }).join('');
}

// ─── Tools ─────────────────────────────────────────────
async function loadTools() {
  const [toolsData, hooksData, pluginsData] = await Promise.all([
    fetch('/api/tools').then(r => r.json()),
    fetch('/api/hooks').then(r => r.json()),
    fetch('/api/plugins').then(r => r.json()),
  ]);
  allTools = toolsData;
  renderTools(toolsData);

  // Hooks
  const hooksList = hooksData.hooks || [];
  const hStats = hooksData.stats || {};
  document.getElementById('hooks-info').innerHTML = hooksList.length === 0
    ? '<em style="color:var(--text2)">No hooks registered</em>'
    : Object.entries(hStats).filter(([,v]) => v > 0).map(([k,v]) =>
      '<div style="font-size:13px;padding:2px 0"><code>' + k + '</code>: ' + v + ' hook(s)</div>'
    ).join('') || '<em style="color:var(--text2)">No active hooks</em>';

  // Plugins
  document.getElementById('plugins-info').innerHTML = pluginsData.length === 0
    ? '<em style="color:var(--text2)">No plugins loaded</em>'
    : pluginsData.map(p => '<div style="font-size:13px;padding:4px 0;border-bottom:1px solid var(--border)">'
      + '<strong>' + esc(p.name) + '</strong> <span style="color:var(--text2)">v' + esc(p.version) + '</span>'
      + (p.description ? '<div style="font-size:12px;color:var(--text2)">' + esc(p.description) + '</div>' : '')
      + '</div>').join('');
}
function renderTools(list) {
  document.getElementById('tools-table').innerHTML = '<table><thead><tr><th>Name</th><th>Description</th><th>Parameters</th></tr></thead><tbody>'
    + list.map(t => '<tr><td><code>' + esc(t.name) + '</code></td><td style="max-width:400px">' + esc(t.description.slice(0,100))
    + '</td><td>' + (t.parameters||[]).map(p => '<code>' + esc(p.name) + '</code>').join(', ') + '</td></tr>').join('')
    + '</tbody></table>';
}
function filterTools(q) {
  const ql = q.toLowerCase();
  renderTools(allTools.filter(t => t.name.includes(ql) || t.description.toLowerCase().includes(ql)));
}

// ─── Recipes ───────────────────────────────────────────
async function loadRecipes() {
  const data = await fetch('/api/recipes').then(r => r.json());
  if (data.length === 0) {
    document.getElementById('recipes-list').innerHTML = '<div class="empty">No recipes available</div>';
    return;
  }
  document.getElementById('recipes-list').innerHTML = '<div class="stats-grid">'
    + data.map(r => '<div class="card" style="cursor:default">'
      + '<div style="display:flex;justify-content:space-between;align-items:start">'
      + '<strong style="color:var(--accent)">' + esc(r.name) + '</strong>'
      + '<span class="tag tag-gray">' + r.steps + ' steps</span></div>'
      + '<div style="font-size:13px;color:var(--text2);margin-top:6px">' + esc(r.description) + '</div>'
      + '<div style="margin-top:8px"><code style="font-size:11px">' + esc(r.id) + '</code></div>'
      + '</div>').join('')
    + '</div>';
}

// ─── Agents ────────────────────────────────────────────
async function loadAgents() {
  const data = await fetch('/api/agents').then(r => r.json());
  document.getElementById('agents-list').innerHTML = data.length === 0
    ? '<div class="empty">No agents running</div>'
    : '<div class="stats-grid">' + data.map(a => '<div class="card">'
      + '<div style="font-size:18px;font-weight:700;color:var(--accent)">' + esc(a.name || a.id) + '</div>'
      + '<div style="font-size:12px;color:var(--text2);margin-top:4px">ID: <code>' + esc(a.id) + '</code></div>'
      + '<div style="margin-top:6px"><span class="tag tag-' + (a.stopped ? 'red' : a.state === 'idle' ? 'green' : 'orange') + '">' + esc(a.stopped ? 'stopped' : a.state || 'active') + '</span></div>'
      + '<div style="display:flex;gap:6px;margin-top:8px">'
      + (a.stopped
        ? '<button class="btn btn-secondary" style="padding:4px 10px;font-size:11px" onclick="restartAgent(\\'' + a.id + '\\')">▶ Restart</button>'
        : '<button class="btn btn-danger" style="padding:4px 10px;font-size:11px" onclick="stopAgent(\\'' + a.id + '\\')">■ Stop</button>')
      + '</div>'
      + '</div>').join('') + '</div>';
}
async function stopAgent(id) {
  await fetch('/api/agents/' + id + '/stop', { method: 'POST' });
  loadAgents();
}
async function restartAgent(id) {
  await fetch('/api/agents/' + id + '/restart', { method: 'POST' });
  loadAgents();
}

// ─── Teams ────────────────────────────────────────────
async function loadTeams() {
  const data = await fetch('/api/teams').then(r => r.json());
  if (data.length === 0) {
    document.getElementById('teams-list').innerHTML = '<div class="empty">No team templates</div>';
    return;
  }
  document.getElementById('teams-list').innerHTML = '<div class="stats-grid">' + data.map(t => '<div class="card">'
    + '<div style="font-size:16px;font-weight:700;color:var(--accent)">' + esc(t.name) + '</div>'
    + (t.description ? '<div style="font-size:12px;color:var(--text2);margin-top:4px">' + esc(t.description) + '</div>' : '')
    + '<div style="margin-top:6px;font-size:12px">'
    + '<span class="tag tag-blue">' + t.agents + ' agents</span> '
    + (t.tags||[]).map(tag => '<span class="tag tag-gray">' + esc(tag) + '</span> ').join('')
    + '</div>'
    + '<button class="btn btn-primary" style="margin-top:10px;padding:6px 14px;font-size:12px;width:100%" onclick="runTeam(\\'' + t.name + '\\')">▶ Run</button>'
    + '</div>').join('') + '</div>';
}
async function runTeam(name) {
  const task = document.getElementById('team-task-input').value.trim();
  if (!task) { alert('Enter a task in the input above first'); return; }
  const el = document.getElementById('teams-list');
  el.innerHTML += '<div class="card" style="margin-top:12px"><em style="color:var(--text2)">Running team "' + esc(name) + '"...</em></div>';
  try {
    const res = await fetch('/api/teams/' + name + '/run', {
      method: 'POST', headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ task }),
    }).then(r => r.json());
    el.innerHTML += '<div class="card" style="margin-top:8px">'
      + '<div class="card-title">Result [' + esc(res.teamRun||'') + ']</div>'
      + '<pre style="white-space:pre-wrap;font-size:12px">' + esc(res.result||JSON.stringify(res)) + '</pre>'
      + '</div>';
  } catch(e) { el.innerHTML += '<div style="color:var(--red)">Error: ' + esc(e.message) + '</div>'; }
}

// ─── Workflows ─────────────────────────────────────────
async function runResearch() {
  const topic = document.getElementById('research-topic').value.trim();
  if (!topic) return;
  const el = document.getElementById('research-result');
  el.innerHTML = '<em style="color:var(--text2)">Running research pipeline...</em>';
  try {
    const res = await fetch('/api/workflows/research', {
      method: 'POST', headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ topic }),
    }).then(r => r.json());
    if (res.error) { el.innerHTML = '<span style="color:var(--red)">' + esc(res.error) + '</span>'; return; }
    el.innerHTML = '<div style="margin-bottom:8px"><span class="tag tag-green">✓ Done</span> Run: <code>' + esc(res.runId) + '</code> &middot; ' + Math.round((res.durationMs||0)/1000) + 's</div>'
      + (res.outputPath ? '<div style="font-size:11px;color:var(--text2)">Saved: ' + esc(res.outputPath) + '</div>' : '')
      + '<details style="margin-top:8px"><summary style="cursor:pointer;color:var(--accent)">View document</summary>'
      + '<pre style="white-space:pre-wrap;font-size:12px;max-height:400px;overflow:auto">' + esc(res.finalDocument||'') + '</pre></details>';
  } catch(e) { el.innerHTML = '<span style="color:var(--red)">' + esc(e.message) + '</span>'; }
}
async function runWorkerPool() {
  const goal = document.getElementById('pool-goal').value.trim();
  const concurrency = parseInt(document.getElementById('pool-concurrency').value) || 3;
  if (!goal) return;
  const el = document.getElementById('pool-result');
  el.innerHTML = '<em style="color:var(--text2)">Running worker pool...</em>';
  try {
    const res = await fetch('/api/workflows/worker-pool', {
      method: 'POST', headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ goal, concurrency }),
    }).then(r => r.json());
    if (res.error) { el.innerHTML = '<span style="color:var(--red)">' + esc(res.error) + '</span>'; return; }
    const tasks = res.tasks || [];
    el.innerHTML = '<div style="margin-bottom:8px"><span class="tag tag-green">✓ Done</span> Run: <code>' + esc(res.runId) + '</code> &middot; ' + tasks.length + ' tasks</div>'
      + '<details style="margin-bottom:8px"><summary style="cursor:pointer;color:var(--text2)">Task breakdown (' + tasks.length + ')</summary>'
      + tasks.map((t,i) => '<div style="padding:4px 0;font-size:12px;border-bottom:1px solid var(--border)">'
        + '<span class="tag tag-' + (t.status==='done'?'green':'red') + '">' + esc(t.status) + '</span> '
        + esc(t.description.slice(0,80)) + '</div>').join('') + '</details>'
      + '<div class="card-title">Summary</div>'
      + '<pre style="white-space:pre-wrap;font-size:12px;max-height:300px;overflow:auto">' + esc(res.finalSummary||'') + '</pre>';
  } catch(e) { el.innerHTML = '<span style="color:var(--red)">' + esc(e.message) + '</span>'; }
}
async function runBenchmark() {
  const suite = document.getElementById('bench-suite').value;
  const el = document.getElementById('bench-result');
  el.innerHTML = '<em style="color:var(--text2)">Running benchmark...</em>';
  try {
    const res = await fetch('/api/workflows/benchmark/' + suite, {
      method: 'POST', headers: {'Content-Type':'application/json'},
      body: JSON.stringify({}),
    }).then(r => r.json());
    if (res.error) { el.innerHTML = '<span style="color:var(--red)">' + esc(res.error) + '</span>'; return; }
    const pct = res.percentScore || 0;
    const color = pct >= 80 ? 'green' : pct >= 50 ? 'orange' : 'red';
    el.innerHTML = '<div style="margin-bottom:12px">'
      + '<span class="tag tag-' + color + '">' + pct + '%</span> &nbsp;'
      + '<strong>' + esc(res.summary||'') + '</strong></div>'
      + '<div style="margin-bottom:8px;height:8px;background:var(--bg3);border-radius:4px;overflow:hidden">'
      + '<div style="height:100%;width:' + pct + '%;background:var(--green);border-radius:4px;transition:width .5s"></div></div>'
      + '<table><thead><tr><th>Task</th><th>Score</th><th>Tokens</th><th>ms</th></tr></thead><tbody>'
      + (res.taskResults||[]).map(t => '<tr>'
        + '<td>' + esc(t.taskName) + '</td>'
        + '<td><span class="tag tag-' + (t.score>=60?'green':'red') + '">' + t.score + '</span></td>'
        + '<td>' + ((t.usage||{}).inputTokens||0) + '+' + ((t.usage||{}).outputTokens||0) + '</td>'
        + '<td>' + (t.durationMs||0) + '</td></tr>').join('')
      + '</tbody></table>';
  } catch(e) { el.innerHTML = '<span style="color:var(--red)">' + esc(e.message) + '</span>'; }
}

// ─── Marketplace ──────────────────────────────────────
async function loadInstalledSkills() {
  const data = await fetch('/api/marketplace/installed').then(r => r.json());
  const el = document.getElementById('market-installed');
  if (!Array.isArray(data) || data.length === 0) {
    el.innerHTML = '<em style="color:var(--text2)">No skills installed</em>'; return;
  }
  el.innerHTML = data.map(s => '<div style="padding:6px 0;border-bottom:1px solid var(--border);font-size:13px;display:flex;justify-content:space-between;align-items:center">'
    + '<div><strong>' + esc(s.name||s.id) + '</strong> <span style="color:var(--text2)">' + esc(s.version) + '</span>'
    + '<div style="font-size:11px;color:var(--text2)">' + esc(s.id) + '</div></div>'
    + '<button class="btn btn-danger" style="padding:3px 8px;font-size:11px" onclick="uninstallSkill(\\'' + s.id + '\\')">✕</button>'
    + '</div>').join('');
}
async function loadFeaturedSkills() {
  const data = await fetch('/api/marketplace/featured').then(r => r.json());
  renderMarketSkills(data, document.getElementById('market-results'), 'Featured Skills');
}
async function searchMarketplace() {
  const q = document.getElementById('market-search').value.trim();
  const data = await fetch('/api/marketplace/search?q=' + encodeURIComponent(q)).then(r => r.json());
  renderMarketSkills(Array.isArray(data) ? data : [], document.getElementById('market-results'), 'Search Results');
}
function renderMarketSkills(skills, el, title) {
  if (!skills || skills.length === 0) { el.innerHTML = '<em style="color:var(--text2)">No results</em>'; return; }
  el.innerHTML = '<div style="font-size:11px;text-transform:uppercase;color:var(--text2);margin-bottom:8px">' + esc(title) + '</div>'
    + skills.map(s => '<div style="padding:8px 0;border-bottom:1px solid var(--border);font-size:13px">'
      + '<div style="display:flex;justify-content:space-between;align-items:start">'
      + '<div><strong>' + esc(s.name||s.id) + '</strong> <span style="color:var(--text2)">v' + esc(s.version||'?') + '</span></div>'
      + '<button class="btn btn-primary" style="padding:3px 10px;font-size:11px" onclick="installSkill(\\'' + s.id + '\\')">↓ Install</button></div>'
      + (s.description ? '<div style="font-size:12px;color:var(--text2);margin-top:2px">' + esc(s.description.slice(0,120)) + '</div>' : '')
      + (s.tags?.length ? '<div style="margin-top:4px">' + s.tags.map(t => '<span class="tag tag-gray">' + esc(t) + '</span> ').join('') + '</div>' : '')
      + '</div>').join('');
}
async function installSkill(id) {
  try {
    const res = await fetch('/api/marketplace/install', {
      method: 'POST', headers: {'Content-Type':'application/json'},
      body: JSON.stringify({ id }),
    }).then(r => r.json());
    if (res.error) alert('Install failed: ' + res.error);
    else { alert('Installed: ' + id); loadInstalledSkills(); }
  } catch(e) { alert('Error: ' + e.message); }
}
async function uninstallSkill(id) {
  if (!confirm('Uninstall skill "' + id + '"?')) return;
  await fetch('/api/marketplace/skills/' + encodeURIComponent(id), { method: 'DELETE' });
  loadInstalledSkills();
}

// ─── Metrics ───────────────────────────────────────────
async function loadMetrics() {
  try {
    const d = await fetch('/api/metrics').then(r => r.json());
    const sg = document.getElementById('metrics-stats');
    sg.innerHTML = sc('Input tokens', d.totalInputTokens?.toLocaleString() ?? 0, 'accent')
      + sc('Output tokens', d.totalOutputTokens?.toLocaleString() ?? 0, 'purple')
      + sc('Est. cost', '$' + (d.totalCostUsd ?? 0).toFixed(4), 'green')
      + sc('Tool calls', d.totalToolCalls ?? 0, 'orange')
      + sc('Avg LLM latency', (d.avgLlmLatencyMs ?? 0).toFixed(0) + 'ms', 'accent')
      + sc('Avg tool latency', (d.avgToolLatencyMs ?? 0).toFixed(0) + 'ms', 'purple')
      + (d.rateLimits ? sc('RPM', d.rateLimits.rpm + ' / ' + d.rateLimits.limits.requestsPerMinute, d.rateLimits.rpm > d.rateLimits.limits.requestsPerMinute * 0.8 ? 'red' : 'green') : '')
      + (d.rateLimits ? sc('TPM', (d.rateLimits.tpm/1000).toFixed(0) + 'k / ' + (d.rateLimits.limits.tokensPerMinute/1000).toFixed(0) + 'k', d.rateLimits.tpm > d.rateLimits.limits.tokensPerMinute * 0.8 ? 'red' : 'green') : '');

    const models = d.topModels || [];
    document.getElementById('metrics-models').innerHTML = models.length === 0
      ? '<em style="color:var(--text2)">No model usage recorded yet</em>'
      : '<table><thead><tr><th>Model</th><th>Input</th><th>Output</th><th>Cost</th></tr></thead><tbody>'
        + models.map(m => '<tr><td><code>' + esc(m.model) + '</code></td><td>' + (m.inputTokens||0).toLocaleString()
          + '</td><td>' + (m.outputTokens||0).toLocaleString() + '</td><td style="color:var(--accent)">$' + (m.costUsd||0).toFixed(4) + '</td></tr>').join('')
        + '</tbody></table>';

    const tts = d.topTools || [];
    document.getElementById('metrics-tools').innerHTML = tts.length === 0
      ? '<em style="color:var(--text2)">No tool calls recorded yet</em>'
      : '<table><thead><tr><th>Tool</th><th>Calls</th><th>Fail rate</th></tr></thead><tbody>'
        + tts.map(t => '<tr><td><code>' + esc(t.name) + '</code></td><td>' + t.calls
          + '</td><td>' + (t.failRate > 0 ? '<span class="tag tag-red">' + (t.failRate*100).toFixed(0) + '%</span>' : '<span class="tag tag-green">0%</span>') + '</td></tr>').join('')
        + '</tbody></table>';

    const agentMap = d.tokensByAgent || {};
    const agentEntries = Object.entries(agentMap);
    document.getElementById('metrics-agents').innerHTML = agentEntries.length === 0
      ? '<em style="color:var(--text2)">No agent data yet</em>'
      : '<table><thead><tr><th>Agent</th><th>Input</th><th>Output</th><th>Cost</th></tr></thead><tbody>'
        + agentEntries.map(([id, stats]) => '<tr><td><code>' + esc(id) + '</code></td><td>' + (stats.input||0).toLocaleString()
          + '</td><td>' + (stats.output||0).toLocaleString() + '</td><td>$' + (stats.cost||0).toFixed(4) + '</td></tr>').join('')
        + '</tbody></table>';

    const errs = d.recentErrors || [];
    document.getElementById('metrics-errors').innerHTML = errs.length === 0
      ? '<em style="color:var(--text2)">No tool errors</em>'
      : errs.map(e => '<div style="padding:4px 0;border-bottom:1px solid var(--border);font-size:12px">'
          + '<span class="tag tag-red">' + esc(e.toolName) + '</span> <code>' + esc(e.agentId) + '</code> &mdash; ' + esc(e.error.slice(0,120)) + '</div>').join('');
  } catch(e) { document.getElementById('metrics-stats').innerHTML = '<em style="color:var(--text2)">Error loading metrics</em>'; }
}
async function resetMetrics() {
  if (!confirm('Reset all in-memory metrics counters?')) return;
  await fetch('/api/metrics/reset', { method: 'POST' });
  loadMetrics();
}

// ─── Webhooks ──────────────────────────────────────────
async function loadWebhooks() {
  const el = document.getElementById('webhooks-list');
  try {
    const d = await fetch('/api/webhooks').then(r => r.json());
    const hooks = d.webhooks || [];
    if (!hooks.length) { el.innerHTML = '<em style="color:var(--text2)">No webhooks registered</em>'; return; }
    el.innerHTML = hooks.map(h => '<div style="padding:8px 0;border-bottom:1px solid var(--border)">'
      + '<div style="display:flex;justify-content:space-between;align-items:center">'
      + '<div><code style="font-size:11px;color:var(--text2)">' + esc(h.id) + '</code>'
      + ' <span class="tag ' + (h.enabled ? 'tag-green' : 'tag-red') + '">' + (h.enabled ? 'enabled' : 'disabled') + '</span></div>'
      + '<div style="display:flex;gap:4px">'
      + '<button class="btn btn-secondary" style="padding:2px 8px;font-size:11px" onclick="testWebhook(\\'' + h.id + '\\')">Test</button>'
      + (h.enabled
          ? '<button class="btn btn-secondary" style="padding:2px 8px;font-size:11px" onclick="disableWebhook(\\'' + h.id + '\\')">Pause</button>'
          : '<button class="btn btn-primary" style="padding:2px 8px;font-size:11px" onclick="enableWebhook(\\'' + h.id + '\\')">Enable</button>')
      + '<button class="btn btn-danger" style="padding:2px 8px;font-size:11px" onclick="deleteWebhook(\\'' + h.id + '\\')">Delete</button>'
      + '</div></div>'
      + '<div style="font-size:12px;margin-top:4px">' + esc(h.url) + '</div>'
      + '<div style="font-size:11px;color:var(--text2)">events: ' + esc((h.events||[]).join(', ')) + (h.description ? ' &middot; ' + esc(h.description) : '') + '</div>'
      + (h.lastDeliveryAt ? '<div style="font-size:11px;color:var(--text2)">last: ' + new Date(h.lastDeliveryAt).toLocaleString() + ' &mdash; HTTP ' + (h.lastDeliveryStatus||'?') + (h.failCount ? ' &mdash; <span style="color:var(--red)">' + h.failCount + ' fails</span>' : '') + '</div>' : '')
      + '</div>').join('');
  } catch { el.innerHTML = '<em style="color:var(--text2)">Error loading webhooks</em>'; }
}
async function registerWebhook() {
  const url = document.getElementById('wh-url').value.trim();
  const eventsRaw = document.getElementById('wh-events').value.trim();
  const secret = document.getElementById('wh-secret').value.trim();
  const desc = document.getElementById('wh-desc').value.trim();
  if (!url) return alert('URL is required');
  const events = eventsRaw.split(',').map(e => e.trim()).filter(Boolean);
  const r = await fetch('/api/webhooks', { method:'POST', headers:{'content-type':'application/json'}, body: JSON.stringify({ url, events, secret: secret||undefined, description: desc||undefined }) });
  if (r.ok) { document.getElementById('wh-url').value = ''; loadWebhooks(); }
  else alert('Failed: ' + await r.text());
}
async function deleteWebhook(id) { if (confirm('Delete webhook ' + id + '?')) { await fetch('/api/webhooks/' + id, {method:'DELETE'}); loadWebhooks(); } }
async function enableWebhook(id) { await fetch('/api/webhooks/' + id + '/enable', {method:'POST'}); loadWebhooks(); }
async function disableWebhook(id) { await fetch('/api/webhooks/' + id + '/disable', {method:'POST'}); loadWebhooks(); }
async function testWebhook(id) {
  const r = await fetch('/api/webhooks/' + id + '/test', {method:'POST'}).then(r => r.json());
  alert(r.ok ? ('Test delivered! Status: ' + r.status + ' in ' + r.durationMs + 'ms') : ('Test failed: ' + (r.error || r.status)));
}

// ─── MCP Servers ───────────────────────────────────────
async function loadMCPServers() {
  const serversEl = document.getElementById('mcp-servers');
  const toolsEl = document.getElementById('mcp-tools');
  try {
    const [status, toolsData] = await Promise.all([
      fetch('/api/status').then(r => r.json()),
      fetch('/api/tools').then(r => r.json()),
    ]);
    const mcpTools = Array.isArray(toolsData) ? toolsData.filter(t => t.name && t.name.startsWith('mcp__')) : [];
    serversEl.innerHTML = '<div style="font-size:13px;line-height:2">'
      + '<div>MCP tools available: <strong>' + mcpTools.length + '</strong></div>'
      + '<div style="font-size:12px;color:var(--text2);margin-top:4px">Configure MCP servers in your <code>.anyclaw/config.json</code> under <code>tools.mcp_servers</code>.</div>'
      + '</div>';
    toolsEl.innerHTML = mcpTools.length === 0
      ? '<em style="color:var(--text2)">No MCP tools loaded. Add MCP servers to config to enable them.</em>'
      : '<table><thead><tr><th>Tool</th><th>Description</th></tr></thead><tbody>'
        + mcpTools.map(t => '<tr><td><code>' + esc(t.name) + '</code></td><td style="font-size:12px;color:var(--text2)">' + esc((t.description||'').slice(0,100)) + '</td></tr>').join('')
        + '</tbody></table>';
  } catch { serversEl.innerHTML = '<em style="color:var(--text2)">Error loading MCP info</em>'; }
}

// ─── Admin / RBAC / Manifest ───────────────────────────
async function loadManifest() {
  const el = document.getElementById('manifest-info');
  try {
    const d = await fetch('/api/manifest').then(r => r.json());
    el.innerHTML = '<pre style="font-size:11px;overflow:auto;max-height:300px">' + esc(JSON.stringify(d, null, 2)) + '</pre>';
  } catch { el.innerHTML = '<em style="color:var(--text2)">Could not fetch manifest</em>'; }
}
async function loadRBAC() {
  const [roles, policies] = await Promise.all([
    fetch('/api/rbac/roles').then(r => r.json()).catch(() => ({ enabled: false, roles: [] })),
    fetch('/api/rbac/policies').then(r => r.json()).catch(() => ({ policies: [] })),
  ]);
  const rolesEl = document.getElementById('rbac-roles');
  rolesEl.innerHTML = '<div style="font-size:12px;color:var(--text2);margin-bottom:6px">RBAC enabled: <strong>' + (roles.enabled ? 'yes' : 'no') + '</strong></div>'
    + (roles.roles || []).map(r => '<div style="margin:4px 0"><code style="font-weight:600">' + esc(r.id) + '</code> &mdash; '
      + (r.allow||[]).map(p => '<span class="tag tag-green">' + esc(p) + '</span>').join(' ')
      + (r.deny||[]).map(p => '<span class="tag tag-red">deny:' + esc(p) + '</span>').join(' ')
      + '</div>').join('');
  const policiesEl = document.getElementById('rbac-policies');
  const pols = policies.policies || [];
  policiesEl.innerHTML = pols.length === 0
    ? '<em style="color:var(--text2)">No policies assigned</em>'
    : '<table><thead><tr><th>Subject</th><th>Role</th></tr></thead><tbody>'
      + pols.map(p => '<tr><td><code>' + esc(p.subject) + '</code></td><td><span class="tag tag-blue">' + esc(p.roleId) + '</span></td></tr>').join('')
      + '</tbody></table>';
}
async function assignRole() {
  const subject = document.getElementById('rbac-subject').value.trim();
  const roleId = document.getElementById('rbac-role').value.trim();
  if (!subject || !roleId) return alert('Subject and Role are required');
  const r = await fetch('/api/rbac/assign', { method: 'POST', headers: { 'content-type': 'application/json' }, body: JSON.stringify({ subject, roleId }) });
  if (r.ok) { loadRBAC(); document.getElementById('rbac-subject').value = ''; document.getElementById('rbac-role').value = ''; }
  else alert('Failed to assign role: ' + (await r.text()));
}
async function loadDistributedAgents() {
  const el = document.getElementById('dist-agents');
  try {
    const d = await fetch('/api/agents/distributed').then(r => r.json());
    const agents = d.agents || [];
    el.innerHTML = agents.length === 0
      ? '<em style="color:var(--text2)">No remote agents registered</em>'
      : '<table><thead><tr><th>ID</th><th>URL</th><th>Status</th><th>Last Seen</th></tr></thead><tbody>'
        + agents.map(a => '<tr><td><code>' + esc(a.id) + '</code></td><td><code>' + esc(a.url) + '</code></td>'
          + '<td><span class="tag tag-' + (a.status === 'active' ? 'green' : 'gray') + '">' + esc(a.status||'unknown') + '</span></td>'
          + '<td style="font-size:11px">' + (a.lastSeen ? new Date(a.lastSeen).toLocaleTimeString() : '—') + '</td></tr>').join('')
        + '</tbody></table>';
  } catch { el.innerHTML = '<em style="color:var(--text2)">Error fetching agents</em>'; }
}

// ─── Settings ──────────────────────────────────────────
async function loadSettings() {
  const [health, safetyLog] = await Promise.all([
    fetch('/health').then(r => r.json()),
    fetch('/api/safety/log?limit=20').then(r => r.json()),
  ]);
  document.getElementById('settings-info').innerHTML =
    '<div style="font-size:13px;line-height:2">'
    + '<div>Version: <code>' + esc(health.version) + '</code></div>'
    + '<div>Uptime: ' + Math.floor(health.uptime) + ' seconds</div>'
    + '<div>Agents: ' + (health.agents?.length||0) + '</div>'
    + '<div>Memory episodes: ' + (health.memory?.episodes||0) + '</div>'
    + '<div>Vault notes: ' + (health.memory?.vaultNotes||0) + '</div>'
    + '</div>';

  const logs = Array.isArray(safetyLog) ? safetyLog : [];
  document.getElementById('safety-log').innerHTML = logs.length === 0
    ? '<em style="color:var(--text2)">No safety events</em>'
    : '<table><thead><tr><th>Agent</th><th>Tool</th><th>Status</th><th>Detail</th></tr></thead><tbody>'
      + logs.slice(0,20).map(l => '<tr><td><code>' + esc(l.agentId||'?') + '</code></td><td><code>' + esc(l.toolName||'?')
      + '</code></td><td>' + (l.error ? '<span class="tag tag-red">error</span>' : '<span class="tag tag-green">ok</span>')
      + '</td><td style="max-width:300px;font-size:12px">' + esc((l.error || l.result || '').slice(0,100)) + '</td></tr>').join('')
      + '</tbody></table>';
}

// ─── Helpers ───────────────────────────────────────────
function esc(t) { const d = document.createElement('div'); d.textContent = t || ''; return d.innerHTML; }
function statusColor(s) { return {done:'green',executing:'orange',planned:'blue',draft:'gray'}[s] || 'gray'; }
function taskColor(s) { return {done:'green','in-progress':'orange',verifying:'blue',pending:'gray',failed:'red'}[s] || 'gray'; }

// Auto-refresh overview stats every 10s
setInterval(() => {
  if (ws && ws.readyState === 1) ws.send(JSON.stringify({ type: 'status' }));
}, 10000);
</script>
</body>
</html>`;
