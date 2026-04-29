#!/usr/bin/env node
// AnyClaw — CLI entry point
// Phase 6: 18 slash commands
// Phase 14.1: Ultra-lightweight mode
// Phase 14.3: Zero-config quickstart
// Phase 4: MCP wiring
// Phase 5: Skills loading

import { Command } from "commander";
import chalk from "chalk";
import ora from "ora";
import { createInterface } from "node:readline";
import { config as loadDotEnv } from "dotenv";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { homedir } from "node:os";
import {
  loadConfig,
  ensureDataDirs,
  ensureDefaultConfig,
  getDataDir,
  AnyClawConfigSchema,
  type AnyClawConfig,
} from "./config/index.js";
import { createRouter, type LLMRouter } from "./llm/index.js";
import { MemoryManager } from "./memory/index.js";
import { createToolRegistry, setSSRFPolicy, getFileChanges, undoLastChange, clearFileChanges, type ToolRegistry } from "./tools/index.js";
import { GSDManager, createGSDTools } from "./gsd/index.js";
import { SafetyManager, getSandboxProfile, type ConfirmationRequest } from "./safety/index.js";
import { Orchestrator } from "./core/orchestrator.js";
import { MCPManager } from "./mcp/index.js";
import { createGateway } from "./gateway/index.js";
import { PluginManager } from "./plugins/index.js";
import { HookRunner } from "./hooks/index.js";
import { RecipeRegistry, createRecipeTools } from "./recipes/index.js";
import { buildNexusCloudManifest, registerWithNexusCloud, validateNexusCloudManifest, deregisterFromNexusCloud, startNexusCloudHeartbeat } from "./cloud/index.js";
import { TeamManager, BUILTIN_TEMPLATES } from "./teams/index.js";
import { createMarketplace } from "./marketplace/index.js";
import { rbac, auditLogger, distributedAgents } from "./rbac/index.js";
import {
  buildNexusToolManifest,
  writeNexusToolManifest,
  buildHostingDescriptor,
  writeHostingDescriptor,
  phantomHooks,
} from "./platform/index.js";
import { metrics, defaultMetricsFile } from "./metrics/index.js";
import { webhooks, defaultWebhooksFile, type WebhookEvent } from "./webhooks/index.js";
import { configureLogger, getLogger, defaultLogFile } from "./logger/index.js";
import { setSecret, getSecret, deleteSecret, listSecrets, secretsFile } from "./secrets/index.js";
import { createArtifact, getArtifact, deleteArtifact, listArtifacts } from "./artifacts/index.js";
import type { Permissions, Skill } from "./core/types.js";

// ─── Bootstrap ──────────────────────────────────────────────────────────────

function bootstrap(opts: { config?: string; lightweight?: boolean } = {}) {
  loadDotEnv();
  ensureDefaultConfig();
  const config = loadConfig(opts.config);
  ensureDataDirs(config);

  const isLightweight = opts.lightweight || config.lightweight.enabled;

  // ── Nexus AI engine: inject config into env vars for createRouter() ──────
  if (config.nexus_ai.enabled && config.nexus_ai.url && !process.env.NEXUS_AI_URL) {
    process.env.NEXUS_AI_URL = config.nexus_ai.url;
  }
  if (config.nexus_ai.api_key && !process.env.NEXUS_AI_API_KEY) {
    process.env.NEXUS_AI_API_KEY = config.nexus_ai.api_key;
  }

  // LLM Router
  const llm = createRouter({
    requestsPerMinute: config.rate_limit.requests_per_minute,
    tokensPerMinute: config.rate_limit.tokens_per_minute,
    maxRetries: config.rate_limit.max_retries,
    backoffMs: config.rate_limit.backoff_ms,
  });

  // Memory (skip in lightweight)
  const memory = isLightweight && config.lightweight.disable_memory
    ? new MemoryManager(":memory:", config.memory.vault_dir)
    : new MemoryManager(config.memory.episodic_db, config.memory.vault_dir);

  // GSD (skip in lightweight)
  const gsd = isLightweight && config.lightweight.disable_gsd
    ? new GSDManager(":memory:", {
        autoCommit: config.gsd.auto_commit,
        verifyBeforeDone: config.gsd.verify_before_done,
      })
    : new GSDManager(config.gsd.db, {
        autoCommit: config.gsd.auto_commit,
        verifyBeforeDone: config.gsd.verify_before_done,
      });

  // Safety (with sandbox profile — Phase 9.6)
  const profile = getSandboxProfile(config.safety.sandbox_profile as "strict" | "standard" | "permissive");
  const permissions: Permissions = {
    allowedTools: [],
    deniedTools: profile.deniedTools,
    allowedPaths: config.safety.allowed_paths,
    blockedCommands: [...new Set([...config.safety.blocked_commands, ...profile.blockedCommands])],
    confirmationRequired: [...new Set([...config.safety.confirmation_required, ...profile.confirmationRequired])],
    sandboxMode: false,
    elevated: config.safety.elevated,
  };
  const safety = new SafetyManager(permissions, { rateLimit: profile.rateLimit });

  // SSRF policy (Phase 9.5)
  setSSRFPolicy({
    blockedHosts: config.safety.ssrf.blocked_hosts,
    allowPrivate: config.safety.ssrf.allow_private,
    allowedHosts: config.safety.ssrf.allowed_hosts,
    blockedUrlPatterns: config.safety.ssrf.blocked_url_patterns,
  });

  // Tools
  const tools = createToolRegistry(config.tools.builtin, memory, llm);

  // Register GSD tools
  for (const tool of createGSDTools(gsd)) {
    tools.register(tool);
  }

  // MCP integration (Phase 4) — connect servers and register discovered tools
  let mcpManager: MCPManager | undefined;
  const mcpServers = config.tools.mcp_servers;
  const hasMcpServers = Object.values(mcpServers).some((s) => s.enabled);

  // Embedding provider for vector search (Phase 2.1)
  // Use LLM router's built-in embedding support (OpenAI, Ollama)
  const embedFn = llm.createEmbeddingFn();
  if (embedFn) {
    memory.setEmbeddingFn(embedFn);
  }

  // Orchestrator
  const hooks = new HookRunner();
  const orchestrator = new Orchestrator({ llm, tools, memory, gsd, safety, hooks });

  // Recipes (Phase 13.6)
  const recipes = new RecipeRegistry();
  const recipeTools = createRecipeTools(recipes);
  for (const t of recipeTools) {
    // Wire recipe_run tool to use the real ToolRegistry
    if (t.name === "recipe_run") {
      const origExecute = t.execute;
      t.execute = async (args, context) => {
        const recipeId = args.recipe_id as string;
        const inputs = (args.inputs as Record<string, string>) || {};
        return recipes.execute(
          recipeId,
          inputs,
          (toolName, toolArgs, ctx) => tools.execute(toolName, toolArgs, ctx),
          context,
        );
      };
    }
    tools.register(t);
  }

  // Load skills (Phase 5)
  const skills = loadAllSkills(config);
  for (const skill of skills) {
    orchestrator.registerSkill(skill);
  }

  // Create default agent
  orchestrator.createAgent({
    id: "main",
    name: "AnyClaw",
    model: config.agent.model,
    fallbackModel: config.agent.fallback_model,
    thinking: config.agent.thinking,
    maxTokens: config.agent.max_tokens,
    temperature: config.agent.temperature,
    soul: config.agent.soul,
    persona: config.agent.persona,
    contextFiles: config.agent.context_files,
    maxToolIterations: config.agent.max_tool_iterations,
    allowedTools: config.agent.allowed_tools,
    deniedTools: config.agent.denied_tools,
  });

  // agents_list tool (Phase 3.12) — introspect active agents
  tools.register({
    name: "agents_list",
    description: "List all active agents with their id, name, and current state",
    parameters: [],
    async execute() {
      return orchestrator.listAgents();
    },
  });

  // ── Gateway self-management tools (Phase 9, 3.13) ─────────────────────────
  tools.register({
    name: "gateway_info",
    description: "Get gateway status: version, uptime, model, active agents, and feature flags",
    parameters: [],
    async execute() {
      const agents = orchestrator.listAgents();
      return {
        version: "0.1.0",
        uptimeSeconds: Math.floor(process.uptime()),
        agents: agents.map(a => {
          const fullAgent = orchestrator.getAgent(a.id);
          return { id: a.id, name: a.name, state: a.state, model: (fullAgent?.config as any)?.model };
        }),
        features: { memory: true, gsd: true, webhooks: true, metrics: true, rbac: rbac.enabled },
      };
    },
  });

  tools.register({
    name: "agent_config_get",
    description: "Get the current agent configuration (API keys are masked)",
    parameters: [
      { name: "agent_id", type: "string", description: "Agent ID (default: main)", required: false },
    ],
    async execute(args) {
      const agentId = (args.agent_id as string) || "main";
      const agent = orchestrator.getAgent(agentId);
      if (!agent) throw new Error(`Agent '${agentId}' not found`);
      const cfg = { ...agent.config } as any;
      // Mask any key-like fields
      for (const k of Object.keys(cfg)) {
        if (/key|token|secret|password/i.test(k)) cfg[k] = "***";
      }
      return cfg;
    },
  });

  tools.register({
    name: "agent_config_set",
    description: "Update a runtime agent configuration field (model, persona, maxToolIterations, etc.). Does not persist across restarts.",
    parameters: [
      { name: "field", type: "string", description: "Config field name (e.g. model, persona, maxToolIterations)", required: true },
      { name: "value", type: "string", description: "New value (will be type-coerced appropriately)", required: true },
      { name: "agent_id", type: "string", description: "Agent ID (default: main)", required: false },
    ],
    async execute(args) {
      const agentId = (args.agent_id as string) || "main";
      const agent = orchestrator.getAgent(agentId);
      if (!agent) throw new Error(`Agent '${agentId}' not found`);
      const field = args.field as string;
      const FORBIDDEN = ["id", "name"];
      if (FORBIDDEN.includes(field)) throw new Error(`Field '${field}' is read-only`);
      let value: unknown = args.value as string;
      // Coerce numbers
      if (!isNaN(Number(value))) value = Number(value);
      // Coerce booleans
      if (value === "true") value = true;
      if (value === "false") value = false;
      (agent.config as any)[field] = value;
      return { updated: field, value, agentId };
    },
  });

  tools.register({
    name: "agent_restart",
    description: "Restart an agent (stop then start it again, preserving config)",
    parameters: [
      { name: "agent_id", type: "string", description: "Agent ID to restart (default: main)", required: false },
    ],
    async execute(args) {
      const agentId = (args.agent_id as string) || "main";
      await orchestrator.restartAgent(agentId);
      return { restarted: agentId };
    },
  });

  // ─── Agent Routing Rules (Phase 7.3) ─────────────────────────────
  tools.register({
    name: "agent_route_add",
    description: "Add a routing rule: messages matching 'pattern' are sent to 'agent_id'. Pattern can be a substring, glob (with *), or a regex wrapped in /slashes/.",
    parameters: [
      { name: "pattern", type: "string", description: "Match pattern — substring, glob (*), or /regex/", required: true },
      { name: "agent_id", type: "string", description: "Target agent ID", required: true },
      { name: "priority", type: "number", description: "Higher = checked first (default: 0)", required: false },
      { name: "description", type: "string", description: "Human-readable description of this rule", required: false },
    ],
    async execute(args) {
      const ruleId = orchestrator.addRoutingRule({
        pattern: args.pattern as string,
        agentId: args.agent_id as string,
        priority: Number(args.priority ?? 0),
        description: args.description as string | undefined,
      });
      return { ruleId, message: `Routing rule added: "${args.pattern}" → ${args.agent_id}` };
    },
  });

  tools.register({
    name: "agent_route_list",
    description: "List all active agent routing rules",
    parameters: [],
    async execute() {
      return { rules: orchestrator.listRoutingRules() };
    },
  });

  tools.register({
    name: "agent_route_remove",
    description: "Remove a routing rule by its ID",
    parameters: [
      { name: "rule_id", type: "string", description: "Rule ID returned by agent_route_add", required: true },
    ],
    async execute(args) {
      const removed = orchestrator.removeRoutingRule(args.rule_id as string);
      return { removed, ruleId: args.rule_id };
    },
  });

  // ─── Agent Delegation (Phase 7.4) ────────────────────────────────
  tools.register({
    name: "delegate_to_agent",
    description: "Delegate a task to an existing agent (by ID) or spawn a temporary sub-agent for a focused task. Returns the agent's response.",
    parameters: [
      { name: "task", type: "string", description: "Task or message to send to the agent", required: true },
      { name: "agent_id", type: "string", description: "Existing agent ID to delegate to (if omitted, spawns a sub-agent)", required: false },
      { name: "model", type: "string", description: "Model for sub-agent (if spawning; ignored if agent_id provided)", required: false },
      { name: "system_prompt", type: "string", description: "System prompt for sub-agent (if spawning)", required: false },
      { name: "agent_name", type: "string", description: "Name for sub-agent (default: delegate)", required: false },
    ],
    async execute(args, context) {
      if (args.agent_id) {
        // Send to existing agent
        const response = await orchestrator.sendToAgent(
          args.agent_id as string,
          args.task as string,
          context?.agentId,
        );
        return { agentId: args.agent_id, response: response.content, sessionId: response.sessionId };
      } else {
        // Spawn ephemeral sub-agent
        const response = await orchestrator.delegate(
          context?.agentId ?? "main",
          args.task as string,
          {
            name: (args.agent_name as string) || "delegate",
            model: (args.model as string) || undefined,
            soul: (args.system_prompt as string) || undefined,
          },
        );
        return { response: response.content, sessionId: response.sessionId };
      }
    },
  });

  // ─── Agent-to-Agent Messaging (Phase 7.6) ────────────────────────
  tools.register({
    name: "send_to_agent",
    description: "Send a direct message to another running agent and get its response. Useful for multi-agent coordination.",
    parameters: [
      { name: "agent_id", type: "string", description: "Target agent ID", required: true },
      { name: "message", type: "string", description: "Message to send", required: true },
      { name: "session_id", type: "string", description: "Existing session ID to continue (optional)", required: false },
    ],
    async execute(args, context) {
      const response = await orchestrator.sendToAgent(
        args.agent_id as string,
        args.message as string,
        context?.agentId,
        args.session_id as string | undefined,
      );
      return {
        from: context?.agentId ?? "external",
        to: args.agent_id,
        response: response.content,
        sessionId: response.sessionId,
      };
    },
  });

  // ─── Canvas / Artifact tools (Phase 3.8 — A2UI) ──────────────────
  tools.register({
    name: "artifact_create",
    description: "Create an interactive HTML artifact (canvas) that will be displayed as a clickable panel in the dashboard. Use this to build visualizations, dashboards, interactive forms, charts, or any web UI. Return the artifact marker in your reply to make it clickable.",
    parameters: [
      { name: "title", type: "string", description: "Artifact title (shown in panel header)", required: true },
      { name: "html", type: "string", description: "Full HTML document or HTML snippet (may include inline CSS/JS)", required: true },
      { name: "description", type: "string", description: "Short description of what this artifact does", required: false },
    ],
    async execute(args, context) {
      const artifact = createArtifact({
        title: args.title as string,
        html: args.html as string,
        description: args.description as string | undefined,
        agentId: context?.agentId,
        sessionId: context?.sessionId,
      });
      const marker = `[ARTIFACT:${artifact.id}:${encodeURIComponent(artifact.title)}]`;
      return {
        artifactId: artifact.id,
        url: `/api/artifacts/${artifact.id}`,
        marker,
        instruction: `Include this marker in your response to display the artifact: ${marker}`,
      };
    },
  });

  tools.register({
    name: "artifact_list",
    description: "List all created artifacts for this session",
    parameters: [],
    async execute() {
      return { artifacts: listArtifacts() };
    },
  });

  tools.register({
    name: "artifact_delete",
    description: "Delete an artifact by ID",
    parameters: [
      { name: "artifact_id", type: "string", description: "Artifact ID to delete", required: true },
    ],
    async execute(args) {
      const deleted = deleteArtifact(args.artifact_id as string);
      return { deleted, artifactId: args.artifact_id };
    },
  });

  // Plugins (Phase 10.1) — load from configured dirs/packages
  const pluginDirs = config.plugins.dirs.map((d: string) =>
    d.startsWith("~") ? join(homedir(), d.slice(2)) : d,
  );
  const pluginManager = new PluginManager(pluginDirs);

  // Team templates (Phase 2.4): load built-ins + user-defined templates
  const teamsDir = join(getDataDir(), "data", "teams");
  const teamManager = new TeamManager(teamsDir);
  for (const t of BUILTIN_TEMPLATES) teamManager.register(t);
  for (const t of teamManager.loadAll()) teamManager.register(t);

  // Marketplace (Phase 3.1): skill registry + installer
  const skillsDir = join(getDataDir(), "workspace", "skills");
  const marketplace = createMarketplace({
    skillsDir,
    registryUrl: (config as any).marketplace?.registry_url,
    apiKey: (config as any).marketplace?.api_key,
  });

  // RBAC + audit (Phase 4.2): configure from config if present
  const rbacConfig = (config as any).rbac;
  if (rbacConfig?.enabled) {
    rbac.enable();
    if (rbacConfig.policies) {
      for (const p of rbacConfig.policies) {
        for (const role of p.roles ?? []) rbac.assignRole(p.subject, role);
      }
    }
  }
  const auditConfig = (config as any).audit;
  if (auditConfig?.enabled) {
    auditLogger.enable();
    if (auditConfig.file) auditLogger.setFile(auditConfig.file);
  }

  // Metrics (Phase 5): enabled by default, file sink in data dir
  const metricsConfig = (config as any).metrics;
  if (metricsConfig?.enabled !== false) {
    metrics.enable();
    const mFile = metricsConfig?.file ?? defaultMetricsFile();
    metrics.setFile(mFile);
  }

  // Webhooks (Phase 5): load registered webhooks from file
  const whConfig = (config as any).webhooks;
  if (whConfig?.enabled !== false) {
    const whFile = whConfig?.file ?? defaultWebhooksFile();
    webhooks.setFile(whFile);
  }

  // Logger (Phase 7, 10.4): configure structured logging
  const logConfig = (config as any).logging;
  configureLogger({
    level: logConfig?.level ?? "info",
    file: logConfig?.file ?? (logConfig?.enabled !== false ? defaultLogFile() : undefined),
    json: logConfig?.json ?? false,
  });
  const log = getLogger("bootstrap");
  log.info("AnyClaw started", { lightweight: opts.lightweight ?? false, model: (config as any).agent?.model ?? "unknown" });

  return { config, llm, memory, tools, gsd, safety, orchestrator, mcpManager, hasMcpServers, mcpServers, hooks, pluginManager, recipes, teamManager, marketplace, rbac, auditLogger, distributedAgents, phantomHooks, metrics, webhooks };
}

// ─── Skills Loading (Phase 5) ───────────────────────────────────────────

function loadAllSkills(config: AnyClawConfig): Skill[] {
  const skills: Skill[] = [];

  // From config
  for (const [id, skillConf] of Object.entries(config.skills)) {
    if (!skillConf.enabled) continue;
    const skillPath = skillConf.path;
    if (existsSync(skillPath)) {
      try {
        const content = readFileSync(skillPath, "utf-8");
        skills.push({
          id,
          name: id,
          description: `Skill loaded from ${skillPath}`,
          instructions: content,
          enabled: true,
          source: skillPath,
          agents: skillConf.agents && skillConf.agents.length > 0 ? skillConf.agents : undefined,
        });
      } catch { /* skip broken skills */ }
    }
  }

  // Auto-discover from workspace skills dir
  const skillsDir = join(getDataDir(), "workspace", "skills");
  if (existsSync(skillsDir)) {
    for (const entry of readdirSync(skillsDir)) {
      const skillFile = join(skillsDir, entry, "SKILL.md");
      if (existsSync(skillFile)) {
        const content = readFileSync(skillFile, "utf-8");
        skills.push({
          id: entry,
          name: entry,
          description: `Auto-discovered skill: ${entry}`,
          instructions: content,
          enabled: true,
          source: skillFile,
        });
      }
    }
  }

  return skills;
}

// ─── Slash Command Handler (Phase 6) ────────────────────────────────────

async function handleSlashCommand(
  input: string,
  ctx: {
    orchestrator: Orchestrator;
    memory: MemoryManager;
    gsd: GSDManager;
    safety: SafetyManager;
    tools: ToolRegistry;
    config: AnyClawConfig;
    sessionId?: string;
    rl: ReturnType<typeof createInterface>;
    hooks?: HookRunner;
    recipes?: RecipeRegistry;
  },
): Promise<{ handled: boolean; sessionId?: string }> {
  const parts = input.trim().split(/\s+/);
  const cmd = parts[0];
  const rest = parts.slice(1).join(" ");

  switch (cmd) {
    // 6.1: /compact
    case "/compact": {
      if (ctx.sessionId) {
        const spinner = ora("Compacting session context...").start();
        await ctx.orchestrator.getDefaultAgent().compactSession(ctx.sessionId);
        spinner.succeed("Session context compacted");
      } else {
        console.log(chalk.gray("No active session to compact"));
      }
      return { handled: true };
    }

    // 6.2: /status
    case "/status": {
      const status = ctx.memory.getStatus();
      const alignment = ctx.memory.getAlignmentStats();
      console.log(chalk.cyan("\nMemory Status:"));
      console.log(chalk.gray(`  Episodes: ${status.episodes}`));
      console.log(chalk.gray(`  Vault notes: ${status.vaultNotes}`));
      console.log(chalk.gray(`  Sessions: ${status.activeSessions}`));
      console.log(
        chalk.gray(
          `  Alignment: ${(alignment.passRate * 100).toFixed(0)}% pass (${alignment.total} events)`,
        ),
      );
      if (alignment.driftAlert) console.log(chalk.red("  Drift alert!"));
      console.log();
      return { handled: true };
    }

    // 6.3: /clear
    case "/clear": {
      console.clear();
      console.log(chalk.cyan.bold("AnyClaw — Interactive Chat\n"));
      return { handled: true };
    }

    // 6.4: /model
    case "/model": {
      if (rest) {
        const agent = ctx.orchestrator.getDefaultAgent();
        (agent.config as any).model = rest;
        console.log(chalk.green(`Model set to: ${rest}`));
      } else {
        const agent = ctx.orchestrator.getDefaultAgent();
        console.log(chalk.gray(`Current model: ${agent.config.model}`));
      }
      return { handled: true };
    }

    // 6.5: /tools
    case "/tools": {
      const toolList = ctx.tools.list();
      console.log(chalk.cyan(`\nAvailable Tools (${toolList.length}):`));
      for (const t of toolList) {
        console.log(chalk.gray(`  ${t.name.padEnd(24)} ${t.description.slice(0, 60)}`));
      }
      console.log();
      return { handled: true };
    }

    // 6.6: /memory
    case "/memory": {
      if (rest) {
        const results = ctx.memory.searchEpisodes(rest, { limit: 5 });
        if (results.length === 0) {
          console.log(chalk.gray("No memories found"));
        } else {
          console.log(chalk.cyan(`\nMemory search: "${rest}"`));
          for (const r of results) {
            console.log(chalk.gray(`  [${new Date(r.validAt).toLocaleDateString()}] ${r.content.slice(0, 100)}`));
          }
        }
      } else {
        const status = ctx.memory.getStatus();
        console.log(chalk.cyan(`\nMemory: ${status.episodes} episodes, ${status.vaultNotes} vault notes`));
      }
      console.log();
      return { handled: true };
    }

    // 6.7: /vault
    case "/vault": {
      const vault = ctx.memory.getVaultContext();
      console.log(chalk.cyan("\nVault Context:"));
      console.log(chalk.gray(vault || "(empty)"));
      console.log();
      return { handled: true };
    }

    // 6.8: /gsd
    case "/gsd": {
      const specs = ctx.gsd.listSpecs();
      if (specs.length === 0) {
        console.log(chalk.gray("\nNo GSD specs"));
      } else {
        console.log(chalk.cyan("\nGSD Specs:"));
        for (const s of specs) {
          const p = ctx.gsd.getProgress(s.id);
          console.log(
            chalk.gray(
              `  [${s.status}] ${s.title} — ${p?.done || 0}/${p?.total || 0} done (${p?.percentDone || 0}%)`,
            ),
          );
        }
      }
      console.log();
      return { handled: true };
    }

    // 6.9: /sessions
    case "/sessions": {
      const sessions = ctx.memory.listSessions({ limit: 10 });
      if (sessions.length === 0) {
        console.log(chalk.gray("\nNo saved sessions"));
      } else {
        console.log(chalk.cyan("\nRecent Sessions:"));
        for (const s of sessions) {
          const mark = s.id === ctx.sessionId ? chalk.green(" (current)") : "";
          console.log(
            chalk.gray(
              `  ${s.id.slice(0, 8)}... | ${s.agentId} | ${s.messageCount || 0} msgs | ${new Date(s.createdAt).toLocaleString()}${mark}`,
            ),
          );
        }
      }
      console.log();
      return { handled: true };
    }

    // 6.10: /resume
    case "/resume": {
      if (!rest) {
        console.log(chalk.gray("Usage: /resume <session-id>"));
        return { handled: true };
      }
      // Find matching session
      const sessions = ctx.memory.listSessions({ limit: 100 });
      const match = sessions.find((s) => s.id.startsWith(rest));
      if (match) {
        console.log(chalk.green(`Resumed session ${match.id.slice(0, 8)}`));
        return { handled: true, sessionId: match.id };
      }
      console.log(chalk.red(`Session not found: ${rest}`));
      return { handled: true };
    }

    // 6.11: /new
    case "/new": {
      return { handled: true, sessionId: undefined };
    }

    // 6.12: /elevated
    case "/elevated": {
      const current = ctx.safety.isElevated();
      if (rest === "on") {
        ctx.safety.setElevated(true);
        console.log(chalk.yellow("Elevated mode ON — all safety checks bypassed"));
      } else if (rest === "off") {
        ctx.safety.setElevated(false);
        console.log(chalk.green("Elevated mode OFF — normal safety checks active"));
      } else {
        console.log(chalk.gray(`Elevated mode: ${current ? "ON" : "OFF"}`));
      }
      return { handled: true };
    }

    // 6.13: /thinking
    case "/thinking": {
      const levels = ["off", "minimal", "low", "medium", "high"];
      if (rest && levels.includes(rest)) {
        (ctx.orchestrator.getDefaultAgent().config as any).thinking = rest;
        console.log(chalk.green(`Thinking level: ${rest}`));
      } else {
        const current = ctx.orchestrator.getDefaultAgent().config.thinking || "medium";
        console.log(chalk.gray(`Thinking: ${current} (options: ${levels.join(", ")})`));
      }
      return { handled: true };
    }

    // 6.14: /export
    case "/export": {
      if (ctx.sessionId) {
        const messages = ctx.memory.getSessionMessages(ctx.sessionId);
        const exportPath = rest || `session-${ctx.sessionId.slice(0, 8)}.json`;
        const { writeFileSync } = await import("node:fs");
        writeFileSync(exportPath, JSON.stringify(messages, null, 2), "utf-8");
        console.log(chalk.green(`Session exported to ${exportPath}`));
      } else {
        console.log(chalk.gray("No active session to export"));
      }
      return { handled: true };
    }

    // 6.15: /agents
    case "/agents": {
      const agents = ctx.orchestrator.listAgents();
      console.log(chalk.cyan("\nAgents:"));
      for (const a of agents) {
        console.log(chalk.gray(`  ${a.id} (${a.name}) — ${a.state}`));
      }
      console.log();
      return { handled: true };
    }

    // 6.16: /skills
    case "/skills": {
      const skills = ctx.orchestrator.listSkills();
      if (skills.length === 0) {
        console.log(chalk.gray("\nNo skills loaded"));
      } else {
        console.log(chalk.cyan("\nSkills:"));
        for (const s of skills) {
          console.log(chalk.gray(`  [${s.enabled ? "ON" : "OFF"}] ${s.name}: ${s.description}`));
        }
      }
      console.log();
      return { handled: true };
    }

    // 6.17: /config — view or update config
    case "/config": {
      if (rest) {
        // Show a specific config key
        const keys = rest.split(".");
        let val: any = ctx.config;
        for (const k of keys) val = val?.[k];
        if (val !== undefined) {
          console.log(chalk.gray(`  ${rest} = ${JSON.stringify(val)}`));
        } else {
          console.log(chalk.red(`  Config key not found: ${rest}`));
        }
      } else {
        console.log(chalk.cyan("\nConfiguration:"));
        console.log(chalk.gray(`  Model: ${ctx.config.agent.model}`));
        console.log(chalk.gray(`  Thinking: ${ctx.config.agent.thinking}`));
        console.log(chalk.gray(`  Max tokens: ${ctx.config.agent.max_tokens}`));
        console.log(chalk.gray(`  Gateway: ${ctx.config.gateway.bind}:${ctx.config.gateway.port}`));
        console.log(chalk.gray(`  Memory DB: ${ctx.config.memory.episodic_db}`));
        console.log(chalk.gray(`  MCP servers: ${Object.keys(ctx.config.tools.mcp_servers).length}`));
        console.log(chalk.gray(`  Skills: ${Object.keys(ctx.config.skills).length}`));
        console.log();
      }
      return { handled: true };
    }

    // 6.18: /exec — execute a shell command directly
    case "/exec": {
      if (!rest) {
        console.log(chalk.gray("Usage: /exec <command>"));
        return { handled: true };
      }
      const verdict = ctx.safety.checkAction({ agentId: "cli", toolName: "shell", args: { command: rest } });
      if (!verdict.allowed) {
        console.log(chalk.red(`Blocked: ${verdict.reason}`));
        return { handled: true };
      }
      try {
        const { execSync } = await import("node:child_process");
        const output = execSync(rest, { encoding: "utf-8", timeout: 30_000, cwd: process.cwd() });
        console.log(output);
      } catch (err: any) {
        console.log(chalk.red(err.stderr || err.message));
      }
      return { handled: true };
    }

    // 6.19: /debug — show debug info for current session/state
    case "/debug": {
      const agent = ctx.orchestrator.getDefaultAgent();
      const state = agent.getState();
      console.log(chalk.cyan("\nDebug Info:"));
      console.log(chalk.gray(`  Agent ID: ${agent.id}`));
      console.log(chalk.gray(`  Phase: ${state.phase}`));
      console.log(chalk.gray(`  Turn count: ${state.turnCount}`));
      console.log(chalk.gray(`  Session: ${ctx.sessionId || "(none)"}`));
      console.log(chalk.gray(`  Pending tool calls: ${state.pendingToolCalls.length}`));
      console.log(chalk.gray(`  Providers: ${ctx.config.agent.model}`));
      if (ctx.sessionId) {
        const msgs = ctx.memory.getSessionMessages(ctx.sessionId);
        console.log(chalk.gray(`  Messages in session: ${msgs.length}`));
        const totalChars = msgs.reduce((s, m) => s + m.content.length, 0);
        console.log(chalk.gray(`  Total chars: ${totalChars}`));
      }
      console.log();
      return { handled: true };
    }

    // 6.20: /kill — terminate a sub-agent or background process
    case "/kill": {
      if (!rest) {
        console.log(chalk.gray("Usage: /kill <agent-id>"));
        return { handled: true };
      }
      const removed = ctx.orchestrator.removeAgent(rest);
      if (removed) {
        console.log(chalk.green(`Agent ${rest} terminated`));
      } else {
        console.log(chalk.red(`Agent not found: ${rest}`));
      }
      return { handled: true };
    }

    // 6.21: /steer — inject a system-level instruction into the current session
    case "/steer": {
      if (!rest) {
        console.log(chalk.gray("Usage: /steer <instruction>"));
        return { handled: true };
      }
      if (ctx.sessionId) {
        const { randomUUID } = await import("node:crypto");
        ctx.memory.pushMessage(ctx.sessionId, {
          id: randomUUID(),
          role: "system",
          content: `[User steering] ${rest}`,
          timestamp: Date.now(),
        });
        console.log(chalk.green("Steering instruction injected into session context"));
      } else {
        console.log(chalk.gray("No active session to steer"));
      }
      return { handled: true };
    }

    // 6.22: /import — import a session from a JSON file
    case "/import": {
      if (!rest) {
        console.log(chalk.gray("Usage: /import <path-to-session.json>"));
        return { handled: true };
      }
      try {
        const { readFileSync } = await import("node:fs");
        const data = JSON.parse(readFileSync(rest, "utf-8"));
        if (!Array.isArray(data)) {
          console.log(chalk.red("Invalid session file: expected JSON array of messages"));
          return { handled: true };
        }
        const session = ctx.memory.createSession("main", "import");
        for (const msg of data) {
          if (msg.role && msg.content) {
            const { randomUUID } = await import("node:crypto");
            ctx.memory.pushMessage(session.id, {
              id: msg.id || randomUUID(),
              role: msg.role,
              content: msg.content,
              timestamp: msg.timestamp || Date.now(),
            });
          }
        }
        console.log(chalk.green(`Imported ${data.length} messages as session ${session.id.slice(0, 8)}`));
        return { handled: true, sessionId: session.id };
      } catch (err: any) {
        console.log(chalk.red(`Import failed: ${err.message}`));
      }
      return { handled: true };
    }

    // 6.23: /help
    case "/help": {
      console.log(chalk.cyan("\nSlash Commands:"));
      const cmds = [
        ["/compact", "Compress session context"],
        ["/status", "Memory & alignment stats"],
        ["/clear", "Clear screen"],
        ["/model [name]", "Get/set model"],
        ["/tools", "List available tools"],
        ["/memory [query]", "Search memory or show stats"],
        ["/vault", "Show vault context"],
        ["/gsd", "List GSD specs & progress"],
        ["/sessions", "List recent sessions"],
        ["/resume <id>", "Resume a previous session"],
        ["/new", "Start a new session"],
        ["/elevated [on|off]", "Toggle elevated mode"],
        ["/thinking [level]", "Get/set thinking depth"],
        ["/export [path]", "Export session to JSON"],
        ["/import <path>", "Import session from JSON"],
        ["/config [key]", "View configuration"],
        ["/exec <cmd>", "Execute a shell command"],
        ["/debug", "Show debug info"],
        ["/kill <agent-id>", "Terminate a sub-agent"],
        ["/steer <text>", "Inject steering instruction"],
        ["/agents", "List active agents"],
        ["/skills", "List loaded skills"],
        ["/recipe [id]", "List or run recipes"],
        ["/hooks", "List registered hooks"],
        ["/plugins", "Plugin system info"],
        ["/undo", "Undo last file change"],
        ["/diff", "Show file changes this session"],
        ["/help", "This help message"],
        ["/version", "Show version"],
        ["exit", "Quit"],
      ];
      for (const [c, d] of cmds) {
        console.log(chalk.gray(`  ${c.padEnd(22)} ${d}`));
      }
      console.log();
      return { handled: true };
    }

    // /recipe — list or run recipes
    case "/recipe": {
      if (ctx.recipes) {
        if (!rest) {
          const list = ctx.recipes.list();
          console.log(chalk.cyan(`\nRecipes (${list.length}):`));
          for (const r of list) {
            console.log(chalk.gray(`  ${r.id.padEnd(20)} ${r.description.slice(0, 60)}`));
          }
          console.log(chalk.gray('\n  Usage: /recipe <id> [inputs as JSON]'));
        } else {
          const parts2 = rest.split(/\s+/);
          const recipeId = parts2[0];
          const inputsStr = parts2.slice(1).join(" ");
          const inputs = inputsStr ? JSON.parse(inputsStr) : {};
          try {
            const result = await ctx.recipes.execute(
              recipeId,
              inputs,
              (toolName, toolArgs, tctx) => ctx.tools.execute(toolName, toolArgs, tctx),
              {
                agentId: "cli",
                sessionId: ctx.sessionId || "",
                workingDir: process.cwd(),
                permissions: ctx.safety.getPermissions(),
              },
            );
            console.log(chalk.cyan(`\nRecipe "${recipeId}" — ${result.success ? chalk.green("SUCCESS") : chalk.red("FAILED")}`));
            for (const s of result.stepResults) {
              const icon = s.success ? chalk.green("✓") : chalk.red("✗");
              console.log(chalk.gray(`  ${icon} ${s.step}`));
              if (s.error) console.log(chalk.red(`    ${s.error}`));
            }
          } catch (err: any) {
            console.log(chalk.red(`Recipe error: ${err.message}`));
          }
        }
      } else {
        console.log(chalk.gray("Recipe system not available"));
      }
      console.log();
      return { handled: true };
    }

    // /hooks — list registered hooks
    case "/hooks": {
      if (ctx.hooks) {
        const stats = ctx.hooks.stats();
        const all = ctx.hooks.list();
        console.log(chalk.cyan(`\nHooks (${all.length} total):`));
        for (const [event, count] of Object.entries(stats)) {
          if (count > 0) console.log(chalk.gray(`  ${event}: ${count}`));
        }
        if (all.length === 0) console.log(chalk.gray("  (none registered)"));
      } else {
        console.log(chalk.gray("Hooks system not available"));
      }
      console.log();
      return { handled: true };
    }

    // /plugins — list loaded plugins
    case "/plugins": {
      // Dynamic: check if pluginManager is accessible (not in ctx, but via closure)
      console.log(chalk.gray("Use the plugin system via configuration (plugins.dirs in anyclaw.yaml)"));
      console.log();
      return { handled: true };
    }

    // 6.18: /version
    case "/version": {
      console.log(chalk.gray("AnyClaw v0.1.0"));
      return { handled: true };
    }

    // 6.16: /undo — undo last file change (Phase 6.16)
    case "/undo": {
      const result = undoLastChange();
      if (result) {
        console.log(chalk.green(`  Undone: ${result.path}`));
        console.log(chalk.gray(`    ${result.action}`));
      } else {
        console.log(chalk.gray("  No file changes to undo"));
      }
      console.log();
      return { handled: true };
    }

    // 6.17: /diff — show file changes this session (Phase 6.17)
    case "/diff": {
      const changes = getFileChanges();
      if (changes.length === 0) {
        console.log(chalk.gray("  No file changes this session"));
      } else {
        console.log(chalk.cyan(`\n  File Changes (${changes.length}):`));
        for (const c of changes) {
          const icon = c.type === "create" ? chalk.green("+") : chalk.yellow("~");
          const ago = Math.round((Date.now() - c.timestamp) / 1000);
          console.log(chalk.gray(`  ${icon} ${c.path}  (${c.type}, ${ago}s ago)`));
        }
      }
      console.log();
      return { handled: true };
    }

    default:
      return { handled: false };
  }
}

// ─── CLI ────────────────────────────────────────────────────────────────────

const program = new Command();

program
  .name("anyclaw")
  .description("The everything claw — comprehensive local-first AI agent framework")
  .version("0.1.0");

// ─── Chat command (interactive REPL) ─────────────────────────────────────

program
  .command("chat")
  .description("Start an interactive chat session with AnyClaw")
  .option("-c, --config <path>", "Path to config file")
  .option("-m, --model <model>", "Override model")
  .option("-l, --lightweight", "Lightweight mode (no memory/GSD)")
  .option("-s, --session <id>", "Resume a session")
  .option("--last", "Auto-resume the most recent session")
  .option("--dev", "Dev mode: use mock LLM (no API calls, fast responses)")
  .action(async (opts) => {
    const { config, orchestrator, memory, tools, gsd, safety, hasMcpServers, mcpServers, hooks, pluginManager, recipes } = bootstrap({
      config: opts.config,
      lightweight: opts.lightweight,
    });

    if (opts.dev) {
      (orchestrator.getDefaultAgent().config as any).model = "mock/dev";
      console.log(chalk.yellow("  [DEV MODE] Using mock LLM — no API calls will be made"));
    } else if (opts.model) {
      (orchestrator.getDefaultAgent().config as any).model = opts.model;
    }

    // MCP: connect servers async after bootstrap (non-blocking)
    if (hasMcpServers) {
      const mcpMgr = new MCPManager();
      mcpMgr.connectAll(mcpServers).then(({ tools: mcpTools, errors }) => {
        for (const t of mcpTools) tools.register(t);
        if (mcpTools.length > 0) console.log(chalk.gray(`  MCP: ${mcpTools.length} tools loaded`));
        for (const e of errors) console.log(chalk.yellow(`  MCP error (${e.id}): ${e.error}`));
      }).catch(() => {});
    }

    // Plugins: discover and load async (non-blocking)
    pluginManager.discoverPlugins().then(({ loaded, errors }) => {
      // Register plugin tools, providers, skills, hooks
      for (const t of pluginManager.getAllTools()) tools.register(t);
      for (const s of pluginManager.getAllSkills()) orchestrator.registerSkill(s);
      for (const h of pluginManager.getPlugins().flatMap((p) => p.hooks)) {
        hooks.register(h.event, h.handler, { source: "plugin" });
      }
      if (loaded.length > 0) console.log(chalk.gray(`  Plugins: ${loaded.join(", ")}`));
      for (const e of errors) console.log(chalk.yellow(`  Plugin error (${e.path}): ${e.error}`));
    }).catch(() => {});

    console.log(chalk.cyan.bold("\nAnyClaw — Interactive Chat"));
    console.log(chalk.gray(`Model: ${opts.model || config.agent.model}`));
    console.log(chalk.gray(`Memory: ${memory.getStatus().episodes} episodes`));
    console.log(chalk.gray('Type "/help" for commands, "exit" to quit\n'));

    const rl = createInterface({ input: process.stdin, output: process.stdout });

    // Phase 8.1: Session persistence — auto-resume or show recent sessions
    let sessionId: string | undefined = opts.session;
    if (!sessionId && opts.last) {
      const recent = memory.listSessions({ limit: 1 });
      if (recent.length > 0) {
        sessionId = recent[0].id;
        console.log(chalk.gray(`  Resumed last session: ${sessionId.slice(0, 8)}... (${recent[0].messageCount} messages)`));
      }
    }
    if (!sessionId) {
      const recent = memory.listSessions({ limit: 5 });
      if (recent.length > 0) {
        console.log(chalk.gray("  Recent sessions:"));
        for (const s of recent) {
          const age = Math.round((Date.now() - s.updatedAt) / 60000);
          const ageStr = age < 60 ? `${age}m ago` : age < 1440 ? `${Math.round(age / 60)}h ago` : `${Math.round(age / 1440)}d ago`;
          console.log(chalk.gray(`    ${s.id.slice(0, 8)} — ${s.messageCount} msgs, ${ageStr}${s.tags.length > 0 ? ` [${s.tags.join(", ")}]` : ""}`));
        }
        console.log(chalk.gray('  Use "/resume <id>" or "--last" to continue one\n'));
      }
    }

    // Interactive confirmation handler (Phase 1.4)
    safety.setConfirmationHandler(async (req: ConfirmationRequest) => {
      return new Promise((resolve) => {
        rl.question(
          chalk.yellow(`\n  Confirm: ${req.action}\n  ${req.details}\n  [y/N] `),
          (answer) => resolve(answer.trim().toLowerCase() === "y"),
        );
      });
    });

    const promptUser = () => {
      rl.question(chalk.green("you > "), async (input) => {
        const trimmed = input.trim();
        if (!trimmed) { promptUser(); return; }

        if (trimmed === "exit" || trimmed === "quit") {
          if (sessionId) {
            const count = orchestrator.getDefaultAgent().ingestSession(sessionId);
            if (count > 0) console.log(chalk.gray(`\nSaved ${count} episodes to memory`));
          }
          console.log(chalk.gray("\nGoodbye!\n"));
          memory.close();
          gsd.close();
          rl.close();
          process.exit(0);
        }

        // Handle slash commands
        if (trimmed.startsWith("/")) {
          const result = await handleSlashCommand(trimmed, {
            orchestrator,
            memory,
            gsd,
            safety,
            tools,
            config,
            sessionId,
            rl,
            hooks,
            recipes,
          });
          if (result.handled) {
            if (result.sessionId !== undefined) sessionId = result.sessionId;
            promptUser();
            return;
          }
        }

        // Send message
        const spinner = ora({ text: "Thinking...", color: "cyan" }).start();

        try {
          const response = await orchestrator.sendMessage(trimmed, undefined, sessionId);
          sessionId = response.sessionId;
          spinner.stop();

          if (response.thinking) {
            console.log(chalk.gray(`  [thinking] ${response.thinking.slice(0, 200)}...`));
          }

          if (response.toolResults.length > 0) {
            console.log(
              chalk.yellow(
                `  [Used ${response.toolResults.length} tool(s): ${response.toolResults.map((t) => t.name).join(", ")}]`,
              ),
            );
          }

          console.log(chalk.white(`\n${response.content}\n`));
          console.log(chalk.gray(`  tokens: ${response.usage.inputTokens}→${response.usage.outputTokens}`));
          console.log();
        } catch (err: any) {
          spinner.fail(chalk.red(`Error: ${err.message}`));
          console.log();
        }

        promptUser();
      });
    };

    promptUser();
  });

// ─── Gateway command ─────────────────────────────────────────────────────

program
  .command("gateway")
  .description("Start the AnyClaw gateway server (HTTP + WebSocket)")
  .option("-c, --config <path>", "Path to config file")
  .option("-p, --port <port>", "Port number", "18800")
  .option("--dev", "Dev mode: use mock LLM (no API calls)")
  .action(async (opts) => {
    const { config, orchestrator, memory, tools, gsd, safety, hasMcpServers, mcpServers, hooks, pluginManager, recipes, teamManager, marketplace } = bootstrap({ config: opts.config });

    if (opts.dev) {
      (orchestrator.getDefaultAgent().config as any).model = "mock/dev";
      console.log(chalk.yellow("  [DEV MODE] Using mock LLM — no API calls will be made"));
    }

    // Wire MCP for gateway too
    if (hasMcpServers) {
      const mcpMgr = new MCPManager();
      const { tools: mcpTools, errors } = await mcpMgr.connectAll(mcpServers);
      for (const t of mcpTools) tools.register(t);
      if (errors.length > 0) {
        for (const e of errors) console.log(chalk.yellow(`  MCP error (${e.id}): ${e.error}`));
      }
    }

    const port = parseInt(opts.port, 10) || config.gateway.port;
    const bind = config.gateway.bind;

    console.log(chalk.cyan.bold("\nAnyClaw Gateway"));

    const gateway = await createGateway({ port, bind }, { orchestrator, memory, gsd, safety, tools, hooks, recipes, plugins: pluginManager, teams: teamManager, marketplace });
    const address = await gateway.start();

    console.log(chalk.green(`  Gateway: ${address}`));
    console.log(chalk.green(`  Dashboard: ${address}`));
    console.log(chalk.green(`  WebSocket: ws://${bind}:${port}/ws`));
    console.log(chalk.green(`  API: ${address}/api/chat`));

    let stopHeartbeat: (() => void) | undefined;
    if (config.cloud.enabled && config.cloud.auto_register) {
      try {
        const registration = await registerWithNexusCloud(config, { gatewayUrl: address });
        console.log(chalk.green(`  Nexus Cloud: registered ${registration?.id ?? "success"}`));
        stopHeartbeat = startNexusCloudHeartbeat(config, { gatewayUrl: address }, undefined, (err) => {
          console.log(chalk.yellow(`  Nexus Cloud heartbeat failed: ${err.message}`));
        });
        console.log(chalk.gray(`  Nexus Cloud heartbeat: every ${config.cloud.heartbeat_interval_seconds}s`));
      } catch (err: any) {
        console.log(chalk.yellow(`  Nexus Cloud registration failed: ${err.message}`));
      }
    }

    console.log(chalk.gray("\n  Press Ctrl+C to stop\n"));

    for (const signal of ["SIGINT", "SIGTERM"] as const) {
      process.on(signal, async () => {
        console.log(chalk.gray("\n\nShutting down..."));
        stopHeartbeat?.();
        await gateway.stop();
        memory.close();
        gsd.close();
        process.exit(0);
      });
    }
  });

// ─── Message command (one-shot) ──────────────────────────────────────────

program
  .command("message")
  .description("Send a single message and get a response")
  .argument("<text>", "Message to send")
  .option("-c, --config <path>", "Path to config file")
  .option("-m, --model <model>", "Override model")
  .option("-l, --lightweight", "Lightweight mode")
  .action(async (text, opts) => {
    const { orchestrator, memory, gsd } = bootstrap({
      config: opts.config,
      lightweight: opts.lightweight,
    });

    const spinner = ora("Processing...").start();

    try {
      const response = await orchestrator.sendMessage(text);
      spinner.stop();
      console.log(response.content);
      orchestrator.getDefaultAgent().ingestSession(response.sessionId);
    } catch (err: any) {
      spinner.fail(`Error: ${err.message}`);
      process.exit(1);
    } finally {
      memory.close();
      gsd.close();
    }
  });

// ─── Pipe command (Phase 12.6) ───────────────────────────────────────────

program
  .command("pipe")
  .description("Read from stdin and output raw response (for scripting)")
  .option("-c, --config <path>", "Path to config file")
  .option("-m, --model <model>", "Override model")
  .option("-l, --lightweight", "Lightweight mode")
  .option("--json", "Output response as JSON")
  .action(async (opts) => {
    const chunks: Buffer[] = [];
    for await (const chunk of process.stdin) {
      chunks.push(chunk as Buffer);
    }
    const input = Buffer.concat(chunks).toString("utf-8").trim();

    if (!input) {
      process.stderr.write("Error: no input received on stdin\n");
      process.exit(1);
    }

    const { orchestrator, memory, gsd } = bootstrap({
      config: opts.config,
      lightweight: opts.lightweight,
    });

    try {
      const response = await orchestrator.sendMessage(input);
      orchestrator.getDefaultAgent().ingestSession(response.sessionId);

      if (opts.json) {
        process.stdout.write(
          JSON.stringify({
            content: response.content,
            sessionId: response.sessionId,
            usage: response.usage,
          }) + "\n"
        );
      } else {
        process.stdout.write(response.content + "\n");
      }
    } catch (err: any) {
      process.stderr.write(`Error: ${err.message}\n`);
      process.exit(1);
    } finally {
      memory.close();
      gsd.close();
    }
  });

// ─── Status command ──────────────────────────────────────────────────────

program
  .command("status")
  .description("Show AnyClaw system status")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    const { config, memory, gsd } = bootstrap({ config: opts.config });

    const status = memory.getStatus();
    const alignment = memory.getAlignmentStats();
    const specs = gsd.listSpecs();

    console.log(chalk.cyan.bold("\nAnyClaw Status\n"));

    console.log(chalk.white("  Configuration:"));
    console.log(chalk.gray(`    Model: ${config.agent.model}`));
    console.log(chalk.gray(`    Thinking: ${config.agent.thinking}`));
    console.log(chalk.gray(`    Gateway: ${config.gateway.bind}:${config.gateway.port}`));
    console.log();

    console.log(chalk.white("  Memory:"));
    console.log(chalk.gray(`    Episodes: ${status.episodes}`));
    console.log(chalk.gray(`    Vault notes: ${status.vaultNotes}`));
    console.log(chalk.gray(`    Sessions: ${status.activeSessions}`));
    console.log();

    console.log(chalk.white("  Alignment:"));
    console.log(chalk.gray(`    Pass rate: ${(alignment.passRate * 100).toFixed(1)}%`));
    console.log(chalk.gray(`    Avg score: ${alignment.avgScore.toFixed(2)}`));
    if (alignment.driftAlert) console.log(chalk.red("    DRIFT ALERT"));
    else console.log(chalk.green("    No drift detected"));
    console.log();

    console.log(chalk.white("  GSD:"));
    console.log(chalk.gray(`    Specs: ${specs.length}`));
    for (const s of specs.slice(0, 5)) {
      const p = gsd.getProgress(s.id);
      console.log(chalk.gray(`      [${s.status}] ${s.title} — ${p?.percentDone || 0}%`));
    }
    console.log();

    memory.close();
    gsd.close();
  });

// ─── Health command ─────────────────────────────────────────────────────

program
  .command("health")
  .description("Show AnyClaw runtime health status")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    const { config, orchestrator, memory, gsd, safety } = bootstrap({ config: opts.config });
    const status = memory.getStatus();
    const alignment = memory.getAlignmentStats();

    console.log(chalk.cyan.bold("\nAnyClaw Health\n"));
    console.log(chalk.white("  Runtime:"));
    console.log(chalk.gray(`    Uptime: ${Math.round(process.uptime())}s`));
    console.log(chalk.gray(`    Model: ${config.agent.model}`));
    console.log();

    console.log(chalk.white("  Agents:"));
    const agents = orchestrator.listAgents();
    console.log(chalk.gray(`    Active agents: ${agents.length}`));
    console.log();

    console.log(chalk.white("  Memory:"));
    console.log(chalk.gray(`    Episodes: ${status.episodes}`));
    console.log(chalk.gray(`    Vault notes: ${status.vaultNotes}`));
    console.log(chalk.gray(`    Sessions: ${status.activeSessions}`));
    console.log();

    console.log(chalk.white("  Alignment:"));
    console.log(chalk.gray(`    Pass rate: ${(alignment.passRate * 100).toFixed(1)}%`));
    console.log(chalk.gray(`    Avg score: ${alignment.avgScore.toFixed(2)}`));
    if (alignment.driftAlert) console.log(chalk.red("    DRIFT ALERT"));
    else console.log(chalk.green("    No drift detected"));
    console.log();

    console.log(chalk.white("  Safety:"));
    const permissions = safety.getPermissions();
    console.log(chalk.gray(`    Allowed tools: ${permissions.allowedTools.length}`));
    console.log(chalk.gray(`    Denied tools: ${permissions.deniedTools.length}`));
    console.log(chalk.gray(`    Confirmation required: ${permissions.confirmationRequired.length}`));
    console.log();

    console.log(chalk.white("  Gateway:"));
    console.log(chalk.gray(`    Bind: ${config.gateway.bind}`));
    console.log(chalk.gray(`    Port: ${config.gateway.port}`));
    console.log();

    memory.close();
    gsd.close();
  });

// ─── Agents commands ────────────────────────────────────────────────────

const agentsCommand = program
  .command("agents")
  .description("Manage runtime agents");

agentsCommand
  .command("list")
  .description("List all active agents")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    const { orchestrator, memory, gsd } = bootstrap({ config: opts.config });
    const agents = orchestrator.listAgents();
    console.log(chalk.cyan.bold("\nActive Agents\n"));
    if (agents.length === 0) {
      console.log(chalk.gray("  No agents registered (run 'anyclaw gateway' to start the runtime)"));
    } else {
      for (const a of agents) {
        const stoppedTag = a.stopped ? chalk.red(" [stopped]") : "";
        console.log(`  ${chalk.green(a.id)}  ${chalk.white(a.name)}  ${chalk.gray(a.state)}${stoppedTag}`);
      }
    }
    console.log();
    memory.close();
    gsd.close();
  });

agentsCommand
  .command("create")
  .description("Create a new agent")
  .requiredOption("--name <name>", "Agent name")
  .option("--model <model>", "LLM model to use")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    const { orchestrator, memory, gsd, config } = bootstrap({ config: opts.config });
    const agent = orchestrator.createAgent({
      id: `${opts.name.toLowerCase().replace(/\s+/g, "-")}-${Date.now()}`,
      name: opts.name,
      model: opts.model || config.agent.model,
    });
    console.log(chalk.green(`\nAgent created: ${agent.id} (${opts.name})\n`));
    memory.close();
    gsd.close();
  });

agentsCommand
  .command("remove")
  .description("Remove an agent by ID")
  .argument("<id>", "Agent ID")
  .option("-c, --config <path>", "Path to config file")
  .action(async (id, opts) => {
    const { orchestrator, memory, gsd } = bootstrap({ config: opts.config });
    const removed = orchestrator.removeAgent(id);
    if (removed) {
      console.log(chalk.green(`\nAgent removed: ${id}\n`));
    } else {
      console.log(chalk.yellow(`\nAgent not found: ${id}\n`));
      process.exit(1);
    }
    memory.close();
    gsd.close();
  });

agentsCommand
  .command("status")
  .description("Show status of a specific agent")
  .argument("<id>", "Agent ID")
  .option("-c, --config <path>", "Path to config file")
  .action(async (id, opts) => {
    const { orchestrator, memory, gsd } = bootstrap({ config: opts.config });
    const agent = orchestrator.getAgent(id);
    if (!agent) {
      console.log(chalk.red(`\nAgent not found: ${id}\n`));
      process.exit(1);
    }
    const state = agent.getState();
    console.log(chalk.cyan.bold(`\nAgent: ${id}\n`));
    console.log(chalk.gray(`  Name:    ${agent.config.name}`));
    console.log(chalk.gray(`  Model:   ${agent.config.model}`));
    console.log(chalk.gray(`  Phase:   ${state.phase}`));
    if (state.sessionId) console.log(chalk.gray(`  Session: ${state.sessionId}`));
    console.log();
    memory.close();
    gsd.close();
  });

agentsCommand
  .command("stop")
  .description("Soft-stop an agent (preserves state, blocks new messages)")
  .argument("<id>", "Agent ID")
  .option("-c, --config <path>", "Path to config file")
  .action(async (id, opts) => {
    const { orchestrator, memory, gsd } = bootstrap({ config: opts.config });
    const stopped = orchestrator.stopAgent(id);
    if (stopped) {
      console.log(chalk.yellow(`\nAgent stopped: ${id}  (use 'agents restart' to resume)\n`));
    } else {
      console.log(chalk.red(`\nAgent not found: ${id}\n`));
      process.exit(1);
    }
    memory.close();
    gsd.close();
  });

agentsCommand
  .command("restart")
  .description("Resume a previously stopped agent")
  .argument("<id>", "Agent ID")
  .option("-c, --config <path>", "Path to config file")
  .action(async (id, opts) => {
    const { orchestrator, memory, gsd } = bootstrap({ config: opts.config });
    const restarted = orchestrator.restartAgent(id);
    if (restarted) {
      console.log(chalk.green(`\nAgent resumed: ${id}\n`));
    } else {
      console.log(chalk.yellow(`\nAgent "${id}" was not stopped (or does not exist).\n`));
      process.exit(1);
    }
    memory.close();
    gsd.close();
  });

// ─── Sessions commands ───────────────────────────────────────────────────

const sessionsCommand = program
  .command("sessions")
  .description("View and manage saved sessions");

sessionsCommand
  .command("list")
  .description("List saved sessions")
  .option("-c, --config <path>", "Path to config file")
  .option("--limit <n>", "Maximum number of sessions to show", "20")
  .action(async (opts) => {
    const { memory, gsd } = bootstrap({ config: opts.config });
    const sessions = memory.listSessions({ limit: parseInt(opts.limit, 10) || 20 });
    console.log(chalk.cyan.bold("\nSaved Sessions\n"));
    if (sessions.length === 0) {
      console.log(chalk.gray("  No sessions found."));
    } else {
      for (const s of sessions) {
        const date = new Date(s.updatedAt).toISOString().replace("T", " ").slice(0, 19);
        console.log(`  ${chalk.green(s.id.slice(0, 12))}  ${chalk.gray(date)}  ${chalk.white(s.messageCount + " msgs")}  ${s.tags.length ? chalk.yellow(s.tags.join(", ")) : ""}`);
      }
    }
    console.log();
    memory.close();
    gsd.close();
  });

sessionsCommand
  .command("show")
  .description("Show messages of a specific session")
  .argument("<id>", "Session ID")
  .option("-c, --config <path>", "Path to config file")
  .option("--limit <n>", "Max messages to show", "20")
  .action(async (id, opts) => {
    const { memory, gsd } = bootstrap({ config: opts.config });
    const messages = memory.getSessionMessages(id);
    if (messages.length === 0) {
      console.log(chalk.yellow(`\nSession not found or empty: ${id}\n`));
      memory.close();
      gsd.close();
      return;
    }
    const limit = parseInt(opts.limit, 10) || 20;
    const shown = messages.slice(-limit);
    console.log(chalk.cyan.bold(`\nSession: ${id}  (${messages.length} messages total)\n`));
    for (const m of shown) {
      const role = m.role === "user" ? chalk.blue("user") : m.role === "assistant" ? chalk.green("assistant") : chalk.gray(m.role);
      const ts = new Date(m.timestamp).toISOString().replace("T", " ").slice(0, 19);
      console.log(`  [${ts}] ${role}: ${m.content.slice(0, 200)}${m.content.length > 200 ? chalk.gray("…") : ""}`);
    }
    console.log();
    memory.close();
    gsd.close();
  });

// ─── Tools commands ──────────────────────────────────────────────────────

program
  .command("tools")
  .description("List registered tools")
  .option("-c, --config <path>", "Path to config file")
  .option("--json", "Output as JSON")
  .action(async (opts) => {
    const { tools: registry, memory, gsd } = bootstrap({ config: opts.config });
    const toolList = registry.list();
    if (opts.json) {
      console.log(JSON.stringify(toolList.map((t) => ({ name: t.name, description: t.description })), null, 2));
    } else {
      console.log(chalk.cyan.bold(`\nRegistered Tools (${toolList.length})\n`));
      for (const t of toolList) {
        const params = t.parameters.map((p) => p.name).join(", ");
        console.log(`  ${chalk.green(t.name.padEnd(28))} ${chalk.gray(t.description.slice(0, 60))}`);
        if (params) console.log(`  ${"".padEnd(28)} ${chalk.dim("params: " + params)}`);
      }
      console.log();
    }
    memory.close();
    gsd.close();
  });

// ─── Plugins commands ────────────────────────────────────────────────────

const pluginsCommand = program
  .command("plugins")
  .description("Discover and load plugins");

pluginsCommand
  .command("list")
  .description("List loaded plugins")
  .option("-c, --config <path>", "Path to config file")
  .option("--json", "Output as JSON")
  .action(async (opts) => {
    const { pluginManager, memory, gsd } = bootstrap({ config: opts.config });
    await pluginManager.discoverPlugins();
    const loaded = pluginManager.getPlugins();
    if (opts.json) {
      console.log(JSON.stringify(loaded.map((p) => ({
        name: p.manifest.name,
        version: p.manifest.version,
        description: p.manifest.description,
        tools: p.tools.length,
        skills: p.skills.length,
        source: p.source,
      })), null, 2));
    } else {
      console.log(chalk.cyan.bold(`\nLoaded Plugins (${loaded.length})\n`));
      if (loaded.length === 0) {
        console.log(chalk.gray("  No plugins loaded. Add plugin directories to config.plugins.dirs"));
      } else {
        for (const p of loaded) {
          console.log(`  ${chalk.green(p.manifest.name)}  v${p.manifest.version}  ${chalk.gray(p.manifest.description || "")}`);
          if (p.tools.length) console.log(chalk.dim(`    tools: ${p.tools.map((t) => t.name).join(", ")}`));
          if (p.skills.length) console.log(chalk.dim(`    skills: ${p.skills.map((s) => s.name).join(", ")}`));
        }
      }
      console.log();
    }
    memory.close();
    gsd.close();
  });

pluginsCommand
  .command("load")
  .description("Load a plugin from a directory or file")
  .argument("<path>", "Path to plugin directory or .js file")
  .option("-c, --config <path>", "Path to config file")
  .action(async (pluginPath, opts) => {
    const { pluginManager, memory, gsd } = bootstrap({ config: opts.config });
    try {
      const plugin = pluginPath.endsWith(".js") || pluginPath.endsWith(".ts")
        ? await pluginManager.loadFromFile(pluginPath)
        : await pluginManager.loadFromDir(pluginPath);
      console.log(chalk.green(`\nPlugin loaded: ${plugin.manifest.name} v${plugin.manifest.version}`));
      if (plugin.tools.length) console.log(chalk.gray(`  Tools: ${plugin.tools.map((t) => t.name).join(", ")}`));
      if (plugin.skills.length) console.log(chalk.gray(`  Skills: ${plugin.skills.map((s) => s.name).join(", ")}`));
      console.log();
    } catch (err: any) {
      console.log(chalk.red(`\nFailed to load plugin: ${err.message}\n`));
      process.exit(1);
    }
    memory.close();
    gsd.close();
  });

// ─── Safety commands ─────────────────────────────────────────────────────

program
  .command("safety")
  .description("Show current safety and permission settings")
  .option("-c, --config <path>", "Path to config file")
  .option("--json", "Output as JSON")
  .action(async (opts) => {
    const { config, safety, memory, gsd } = bootstrap({ config: opts.config });
    const permissions = safety.getPermissions();
    const log = safety.getActionLog(20);
    if (opts.json) {
      console.log(JSON.stringify({ permissions, recentActions: log.length }, null, 2));
    } else {
      console.log(chalk.cyan.bold("\nSafety Status\n"));
      console.log(chalk.white("  Profile:"));
      console.log(chalk.gray(`    Sandbox profile: ${config.safety.sandbox_profile}`));
      console.log(chalk.gray(`    Elevated mode:   ${permissions.elevated ? chalk.yellow("YES") : "no"}`));
      console.log();
      console.log(chalk.white("  Tools:"));
      console.log(chalk.gray(`    Allowed tools: ${permissions.allowedTools.length === 0 ? "all" : permissions.allowedTools.join(", ")}`));
      console.log(chalk.gray(`    Denied tools:  ${permissions.deniedTools.length > 0 ? chalk.red(permissions.deniedTools.join(", ")) : "none"}`));
      console.log();
      console.log(chalk.white("  Commands:"));
      console.log(chalk.gray(`    Blocked patterns:       ${permissions.blockedCommands.length}`));
      console.log(chalk.gray(`    Confirmation required:  ${permissions.confirmationRequired.length}`));
      if (permissions.blockedCommands.length > 0) {
        for (const b of permissions.blockedCommands.slice(0, 5)) {
          console.log(chalk.dim(`      - ${b}`));
        }
        if (permissions.blockedCommands.length > 5) console.log(chalk.dim(`      … and ${permissions.blockedCommands.length - 5} more`));
      }
      console.log();
      console.log(chalk.white("  Paths:"));
      console.log(chalk.gray(`    Allowed paths: ${permissions.allowedPaths.length === 0 ? "all" : permissions.allowedPaths.join(", ")}`));
      console.log();
      console.log(chalk.white("  Action log:"));
      console.log(chalk.gray(`    Recent actions: ${log.length}`));
      console.log();
    }
    memory.close();
    gsd.close();
  });

// ─── GSD command ─────────────────────────────────────────────────────────────

const gsdCommand = program
  .command("gsd")
  .description("Manage GSD (Get Stuff Done) specs and tasks");

gsdCommand
  .command("list")
  .description("List all GSD specs with progress")
  .option("-c, --config <path>", "Path to config file")
  .option("--json", "Output as JSON")
  .action(async (opts) => {
    const { gsd, memory } = bootstrap({ config: opts.config, lightweight: true });
    const specs = gsd.listSpecs();
    if (opts.json) {
      console.log(JSON.stringify(specs.map((s) => ({ ...s, progress: gsd.getProgress(s.id) })), null, 2));
    } else if (specs.length === 0) {
      console.log(chalk.gray("No GSD specs found. Use 'anyclaw gsd create <title>' to create one."));
    } else {
      console.log(chalk.cyan.bold(`\nGSD Specs (${specs.length})\n`));
      for (const s of specs) {
        const p = gsd.getProgress(s.id) ?? { total: 0, done: 0, inProgress: 0 };
        const bar = `[${"█".repeat(Math.round((p.done / Math.max(p.total, 1)) * 10))}${"░".repeat(10 - Math.round((p.done / Math.max(p.total, 1)) * 10))}]`;
        const statusColor = s.status === "done" ? chalk.green : s.status === "executing" ? chalk.yellow : chalk.gray;
        console.log(`  ${chalk.white.bold(s.id.slice(0, 8))}  ${statusColor(s.status.padEnd(12))}  ${bar}  ${p.done}/${p.total}  ${chalk.white(s.title)}`);
      }
      console.log();
    }
    memory.close();
    gsd.close();
  });

gsdCommand
  .command("show <id>")
  .description("Show spec detail with all tasks")
  .option("-c, --config <path>", "Path to config file")
  .action(async (id, opts) => {
    const { gsd, memory } = bootstrap({ config: opts.config, lightweight: true });
    const spec = gsd.getSpec(id) ?? gsd.listSpecs().find((s) => s.id.startsWith(id));
    if (!spec) {
      console.log(chalk.red(`Spec not found: ${id}`));
      memory.close(); gsd.close(); process.exit(1);
    }
    const p = gsd.getProgress(spec.id) ?? { total: 0, done: 0, inProgress: 0 };
    console.log(chalk.cyan.bold(`\n${spec.title}`));
    console.log(chalk.gray(`  ID: ${spec.id}`));
    console.log(chalk.gray(`  Status: ${spec.status}   Tasks: ${p.done}/${p.total} done (${p.inProgress} in-progress)\n`));
    if (spec.description) console.log(`  ${spec.description}\n`);
    if (spec.tasks.length === 0) {
      console.log(chalk.gray("  No tasks yet. Use 'anyclaw gsd task-add <id> <title>'"));
    } else {
      for (const t of spec.tasks) {
        const icon = t.status === "done" ? chalk.green("✔") : t.status === "in-progress" ? chalk.yellow("●") : chalk.gray("○");
        console.log(`  ${icon}  ${t.status === "done" ? chalk.gray(t.title) : chalk.white(t.title)}`);
        if (t.description) console.log(chalk.gray(`       ${t.description}`));
      }
    }
    console.log();
    memory.close();
    gsd.close();
  });

gsdCommand
  .command("create <title>")
  .description("Create a new GSD spec")
  .option("-c, --config <path>", "Path to config file")
  .option("-d, --description <desc>", "Spec description")
  .action(async (title, opts) => {
    const { gsd, memory } = bootstrap({ config: opts.config, lightweight: true });
    const spec = gsd.createSpec(title, opts.description ?? "");
    console.log(chalk.green(`\nCreated spec: ${spec.id}`));
    console.log(chalk.white(`  Title: ${spec.title}`));
    console.log(chalk.gray(`  Add tasks with: anyclaw gsd task-add ${spec.id.slice(0, 8)} <title>\n`));
    memory.close();
    gsd.close();
  });

gsdCommand
  .command("task-add <spec-id> <title>")
  .description("Add a task to a GSD spec")
  .option("-c, --config <path>", "Path to config file")
  .option("-d, --description <desc>", "Task description")
  .option("--criteria <criteria>", "Comma-separated done criteria")
  .action(async (specId, title, opts) => {
    const { gsd, memory } = bootstrap({ config: opts.config, lightweight: true });
    const spec = gsd.getSpec(specId) ?? gsd.listSpecs().find((s) => s.id.startsWith(specId));
    if (!spec) {
      console.log(chalk.red(`Spec not found: ${specId}`));
      memory.close(); gsd.close(); process.exit(1);
    }
    const doneCriteria: string[] = opts.criteria ? opts.criteria.split(",").map((s: string) => s.trim()) : [];
    gsd.addTask(spec.id, title, opts.description ?? "", doneCriteria);
    const p = gsd.getProgress(spec.id) ?? { total: 0, done: 0 };
    console.log(chalk.green(`\nTask added to '${spec.title}'`));
    console.log(chalk.gray(`  Progress: ${p.done}/${p.total} done\n`));
    memory.close();
    gsd.close();
  });

// ─── Board command ────────────────────────────────────────────────────────────

program
  .command("board")
  .description("Show active agents and GSD progress overview (Phase 2.4 monitor)")
  .option("-c, --config <path>", "Path to config file")
  .option("--json", "Output as JSON")
  .action(async (opts) => {
    const { orchestrator, gsd, memory, tools, safety } = bootstrap({ config: opts.config, lightweight: true });
    const agents = orchestrator.listAgents();
    const specs = gsd.listSpecs();
    const toolNames = tools.names();

    if (opts.json) {
      console.log(JSON.stringify({
        agents: agents.map((a) => ({ id: a.id, name: a.name, state: a.state })),
        specs: specs.map((s) => ({ ...s, progress: gsd.getProgress(s.id) })),
        tools: toolNames.length,
      }, null, 2));
    } else {
      const uptime = process.uptime();
      const uptimeStr = uptime < 60 ? `${Math.round(uptime)}s` : uptime < 3600 ? `${Math.round(uptime / 60)}m` : `${Math.round(uptime / 3600)}h`;
      console.log(chalk.cyan.bold("\n AnyClaw Board\n"));
      console.log(`  ${chalk.white("Agents:")}`)
      if (agents.length === 0) {
        console.log(chalk.gray("    No active agents"));
      } else {
        for (const a of agents) {
          const color = a.state === "idle" ? chalk.green : a.state === "running" ? chalk.yellow : chalk.gray;
          console.log(`    ${color("●")}  ${chalk.white(a.id.slice(0, 12))}  ${color(a.state)}`);
        }
      }
      console.log();
      console.log(`  ${chalk.white("GSD Specs:")}`);
      if (specs.length === 0) {
        console.log(chalk.gray("    No specs"));
      } else {
        for (const s of specs.slice(0, 8)) {
          const p = gsd.getProgress(s.id) ?? { total: 0, done: 0 };
          const pct = p.total > 0 ? Math.round((p.done / p.total) * 100) : 0;
          const bar = `${"█".repeat(Math.round(pct / 10))}${"░".repeat(10 - Math.round(pct / 10))}`;
          const color = s.status === "done" ? chalk.green : s.status === "executing" ? chalk.yellow : chalk.gray;
          console.log(`    ${color(bar)}  ${pct.toString().padStart(3)}%  ${chalk.white(s.title.slice(0, 40))}`);
        }
        if (specs.length > 8) console.log(chalk.gray(`    … and ${specs.length - 8} more`));
      }
      console.log();
      console.log(chalk.gray(`  Tools: ${toolNames.length}   Safety: ${safety.getPermissions().sandboxMode ? "sandbox" : "standard"}   Uptime: ${uptimeStr}`));
      console.log();
    }
    memory.close();
    gsd.close();
  });

// ─── Recipes command ──────────────────────────────────────────────────────────

program
  .command("recipes")
  .description("List available built-in recipes (pre-built workflows)")
  .option("-c, --config <path>", "Path to config file")
  .option("--json", "Output as JSON")
  .action(async (opts) => {
    const { memory, gsd } = bootstrap({ config: opts.config, lightweight: true });
    const { RecipeRegistry } = await import("./recipes/index.js");
    const registry = new RecipeRegistry();
    const list = registry.list();
    if (opts.json) {
      console.log(JSON.stringify(list.map((r) => ({ id: r.id, name: r.name, description: r.description, steps: r.steps.length, inputs: r.inputs.length })), null, 2));
    } else if (list.length === 0) {
      console.log(chalk.gray("No recipes available."));
    } else {
      console.log(chalk.cyan.bold(`\nRecipes (${list.length})\n`));
      for (const r of list) {
        console.log(`  ${chalk.white.bold(r.name.padEnd(28))} ${chalk.gray(r.description)}`);
        console.log(chalk.gray(`    id: ${r.id}   steps: ${r.steps.length}   inputs: ${r.inputs.map((i) => i.name).join(", ") || "none"}`));
        console.log();
      }
    }
    memory.close();
    gsd.close();
  });

// ─── MCP command ─────────────────────────────────────────────────────────────

const mcpCommand = program
  .command("mcp")
  .description("Inspect MCP (Model Context Protocol) server configuration");

mcpCommand
  .command("list")
  .description("List all configured MCP servers and their enabled state")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    const { config, memory, gsd } = bootstrap({ config: opts.config });
    const servers = config.tools.mcp_servers as Record<string, import("./core/types.js").MCPServerConfig>;
    const entries = Object.entries(servers);
    console.log(chalk.cyan.bold("\nMCP Servers\n"));
    if (entries.length === 0) {
      console.log(chalk.gray("  No MCP servers configured. Add them under tools.mcp_servers in your config file."));
    } else {
      for (const [name, srv] of entries) {
        const status = srv.enabled ? chalk.green("enabled") : chalk.gray("disabled");
        const transport = chalk.blue(srv.transport);
        const endpoint = srv.transport === "stdio"
          ? chalk.white(`${srv.command ?? ""} ${(srv.args ?? []).join(" ")}`.trim())
          : chalk.white(srv.url ?? "");
        console.log(`  ${chalk.bold(name)}  [${transport}]  ${status}`);
        if (endpoint) console.log(chalk.gray(`    ${endpoint}`));
      }
    }
    console.log();
    memory.close();
    gsd.close();
  });

mcpCommand
  .command("status")
  .description("Connect to enabled MCP servers and report tool counts and errors")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    const { config, memory, gsd, mcpServers } = bootstrap({ config: opts.config });
    const enabledServers = Object.entries(mcpServers as Record<string, import("./core/types.js").MCPServerConfig>)
      .filter(([, s]) => s.enabled);
    console.log(chalk.cyan.bold("\nMCP Server Status\n"));
    if (enabledServers.length === 0) {
      console.log(chalk.gray("  No enabled MCP servers configured."));
    } else {
      const { MCPManager } = await import("./mcp/index.js");
      const mgr = new MCPManager();
      const { tools: mcpTools, errors } = await mgr.connectAll(mcpServers as Record<string, import("./core/types.js").MCPServerConfig>);
      const errMap = new Map(errors.map((e) => [e.id, e.error]));
      for (const [name] of enabledServers) {
        const err = errMap.get(name);
        if (err) {
          console.log(`  ${chalk.red("✗")} ${chalk.bold(name)}  ${chalk.red(err)}`);
        } else {
          console.log(`  ${chalk.green("✓")} ${chalk.bold(name)}  ${chalk.gray(`${mcpTools.length} tools registered`)}`);
        }
      }
      await mgr.closeAll();
    }
    console.log();
    memory.close();
    gsd.close();
  });

// ─── Cloud command ────────────────────────────────────────────────────────────

const cloudCommand = program
  .command("cloud")
  .description("Manage Nexus Cloud registration and discovery");

cloudCommand
  .command("status")
  .description("Show the local Nexus Cloud registration manifest")
  .option("-c, --config <path>", "Path to config file")
  .option("--endpoint <url>", "Override cloud.endpoint from config")
  .option("--api-key <key>", "Override cloud.api_key from config")
  .action(async (opts) => {
    const { config, memory, gsd } = bootstrap({ config: opts.config });

    const effectiveConfig = { ...config };
    if (opts.endpoint) effectiveConfig.cloud.endpoint = opts.endpoint;
    if (opts.apiKey) effectiveConfig.cloud.api_key = opts.apiKey;

    const gatewayUrl = `http://${effectiveConfig.gateway.bind}:${effectiveConfig.gateway.port}`;
    const manifest = buildNexusCloudManifest(effectiveConfig, { gatewayUrl });
    const validationErrors = validateNexusCloudManifest(manifest);

    if (validationErrors.length > 0) {
      console.log(chalk.red("Nexus Cloud manifest validation failed:"));
      for (const err of validationErrors) {
        console.log(chalk.red(`  - ${err}`));
      }
      memory.close();
      gsd.close();
      process.exit(1);
    }

    console.log(chalk.cyan.bold("\nNexus Cloud Manifest:\n"));
    console.log(JSON.stringify(manifest, null, 2));

    memory.close();
    gsd.close();
  });

cloudCommand
  .command("validate")
  .description("Validate the local Nexus Cloud registration manifest")
  .option("-c, --config <path>", "Path to config file")
  .option("--endpoint <url>", "Override cloud.endpoint from config")
  .option("--api-key <key>", "Override cloud.api_key from config")
  .action(async (opts) => {
    const { config, memory, gsd } = bootstrap({ config: opts.config });

    const effectiveConfig = { ...config };
    if (opts.endpoint) effectiveConfig.cloud.endpoint = opts.endpoint;
    if (opts.apiKey) effectiveConfig.cloud.api_key = opts.apiKey;

    const gatewayUrl = `http://${effectiveConfig.gateway.bind}:${effectiveConfig.gateway.port}`;
    const manifest = buildNexusCloudManifest(effectiveConfig, { gatewayUrl });
    const validationErrors = validateNexusCloudManifest(manifest);

    if (validationErrors.length > 0) {
      console.log(chalk.red("Nexus Cloud manifest validation failed:"));
      for (const err of validationErrors) {
        console.log(chalk.red(`  - ${err}`));
      }
      memory.close();
      gsd.close();
      process.exit(1);
    }

    console.log(chalk.green("Nexus Cloud manifest is valid."));
    console.log(JSON.stringify(manifest, null, 2));

    memory.close();
    gsd.close();
  });

cloudCommand
  .command("register")
  .description("Register this AnyClaw instance with Nexus Cloud")
  .option("-c, --config <path>", "Path to config file")
  .option("-f, --force", "Register even if cloud.enabled is false")
  .option("--endpoint <url>", "Override cloud.endpoint from config")
  .option("--api-key <key>", "Override cloud.api_key from config")
  .action(async (opts) => {
    const { config, memory, gsd, safety, tools, hasMcpServers, mcpServers, hooks, pluginManager, recipes } = bootstrap({ config: opts.config });

    if (!config.cloud.enabled && !opts.force) {
      console.log(chalk.yellow("Nexus Cloud registration is disabled in config. Use --force to override."));
      memory.close();
      gsd.close();
      return;
    }

    const effectiveConfig = { ...config };
    if (opts.endpoint) effectiveConfig.cloud.endpoint = opts.endpoint;
    if (opts.apiKey) effectiveConfig.cloud.api_key = opts.apiKey;

    if (!effectiveConfig.cloud.endpoint) {
      console.log(chalk.red("Missing cloud.endpoint in config or --endpoint override."));
      memory.close();
      gsd.close();
      process.exit(1);
    }

    const gatewayUrl = `http://${effectiveConfig.gateway.bind}:${effectiveConfig.gateway.port}`;
    const manifest = buildNexusCloudManifest(effectiveConfig, { gatewayUrl });
    const validationErrors = validateNexusCloudManifest(manifest);

    if (validationErrors.length > 0) {
      console.log(chalk.red("Nexus Cloud manifest validation failed:"));
      for (const err of validationErrors) {
        console.log(chalk.red(`  - ${err}`));
      }
      memory.close();
      gsd.close();
      process.exit(1);
    }

    console.log(chalk.cyan.bold("\nRegistering using Nexus Cloud manifest:\n"));
    console.log(JSON.stringify(manifest, null, 2));

    try {
      const result = await registerWithNexusCloud(effectiveConfig, { gatewayUrl });
      console.log(chalk.green(`Nexus Cloud registration successful: ${JSON.stringify(result)}`));
    } catch (err: any) {
      console.log(chalk.red(`Nexus Cloud registration failed: ${err.message}`));
      process.exit(1);
    } finally {
      memory.close();
      gsd.close();
    }
  });

cloudCommand
  .command("deregister")
  .description("Deregister this AnyClaw instance from Nexus Cloud")
  .option("-c, --config <path>", "Path to config file")
  .option("-f, --force", "Deregister even if cloud.enabled is false")
  .option("--endpoint <url>", "Override cloud.endpoint from config")
  .option("--api-key <key>", "Override cloud.api_key from config")
  .action(async (opts) => {
    const { config, memory, gsd } = bootstrap({ config: opts.config });

    if (!config.cloud.enabled && !opts.force) {
      console.log(chalk.yellow("Nexus Cloud deregistration is disabled in config. Use --force to override."));
      memory.close();
      gsd.close();
      return;
    }

    const effectiveConfig = { ...config };
    if (opts.endpoint) effectiveConfig.cloud.endpoint = opts.endpoint;
    if (opts.apiKey) effectiveConfig.cloud.api_key = opts.apiKey;

    if (!effectiveConfig.cloud.endpoint) {
      console.log(chalk.red("Missing cloud.endpoint in config or --endpoint override."));
      memory.close();
      gsd.close();
      process.exit(1);
    }

    const gatewayUrl = `http://${effectiveConfig.gateway.bind}:${effectiveConfig.gateway.port}`;

    try {
      const result = await deregisterFromNexusCloud(effectiveConfig, { gatewayUrl });
      console.log(chalk.green(`Nexus Cloud deregistration successful: ${JSON.stringify(result)}`));
    } catch (err: any) {
      console.log(chalk.red(`Nexus Cloud deregistration failed: ${err.message}`));
      process.exit(1);
    } finally {
      memory.close();
      gsd.close();
    }
  });

// ─── Doctor command ─────────────────────────────────────────────

program
  .command("doctor")
  .description("Check system health and configuration")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    loadDotEnv();
    const config = loadConfig(opts.config);
    const checks: { name: string; ok: boolean; detail: string }[] = [];

    const nodeVersion = parseInt(process.versions.node.split(".")[0], 10);
    checks.push({
      name: "Node.js version",
      ok: nodeVersion >= 22,
      detail: `v${process.versions.node} ${nodeVersion >= 22 ? "" : "(need >=22)"}`,
    });

    checks.push({
      name: "Anthropic API key",
      ok: !!process.env.ANTHROPIC_API_KEY,
      detail: process.env.ANTHROPIC_API_KEY ? "configured" : "missing",
    });
    checks.push({
      name: "OpenAI API key",
      ok: !!process.env.OPENAI_API_KEY,
      detail: process.env.OPENAI_API_KEY ? "configured" : "missing (optional)",
    });

    checks.push({ name: "Default model", ok: true, detail: config.agent.model });

    checks.push({
      name: "Gateway bind",
      ok: config.gateway.bind === "127.0.0.1" || config.gateway.bind === "localhost",
      detail: `${config.gateway.bind}:${config.gateway.port} ${config.gateway.bind === "127.0.0.1" ? "(localhost ✓)" : "publicly accessible!"}`,
    });

    checks.push({
      name: "Blocked commands",
      ok: config.safety.blocked_commands.length > 0,
      detail: `${config.safety.blocked_commands.length} patterns blocked`,
    });

    checks.push({
      name: "Data directory",
      ok: existsSync(getDataDir()),
      detail: getDataDir(),
    });

    checks.push({
      name: "Memory DB",
      ok: true, // Will be created on first use
      detail: config.memory.episodic_db,
    });

    checks.push({
      name: "GSD DB",
      ok: true,
      detail: config.gsd.db,
    });

    // Check MCP servers
    const mcpCount = Object.keys(config.tools.mcp_servers).length;
    checks.push({
      name: "MCP servers",
      ok: true,
      detail: mcpCount > 0 ? `${mcpCount} configured` : "none configured (optional)",
    });

    // Check skills
    const skillCount = Object.keys(config.skills).length;
    checks.push({
      name: "Skills",
      ok: true,
      detail: skillCount > 0 ? `${skillCount} configured` : "none configured (optional)",
    });

    console.log(chalk.cyan.bold("\nAnyClaw Doctor\n"));
    for (const check of checks) {
      const icon = check.ok ? chalk.green("✓") : chalk.red("✗");
      console.log(`  ${icon} ${chalk.white(check.name)}: ${chalk.gray(check.detail)}`);
    }
    console.log();

    const failed = checks.filter((c) => !c.ok);
    if (failed.length === 0) {
      console.log(chalk.green("  All checks passed!\n"));
    } else {
      console.log(chalk.yellow(`  ${failed.length} issue(s) found.\n`));
    }
  });

// ─── Validate command (Phase 10.8) ──────────────────────────────────────

program
  .command("validate")
  .description("Validate AnyClaw configuration file and report issues")
  .option("-c, --config <path>", "Path to config file")
  .action(async (opts) => {
    loadDotEnv();
    console.log(chalk.cyan.bold("\nAnyClaw Config Validation\n"));

    const configPaths = opts.config
      ? [opts.config]
      : [
          join(process.cwd(), "anyclaw.yaml"),
          join(process.cwd(), "anyclaw.yml"),
          join(getDataDir(), "anyclaw.yaml"),
        ];

    let found = false;
    for (const p of configPaths) {
      const resolved = p.startsWith("~/") ? join(homedir(), p.slice(2)) : p;
      if (existsSync(resolved)) {
        console.log(chalk.white(`  Config file: ${resolved}`));
        found = true;

        try {
          const raw = readFileSync(resolved, "utf-8");
          const { load } = await import("js-yaml");
          const parsed = load(raw) as Record<string, unknown>;

          // Validate with Zod
          const { AnyClawConfigSchema } = await import("./config/index.js");
          const result = AnyClawConfigSchema.safeParse(parsed || {});

          if (result.success) {
            console.log(chalk.green("  ✓ Config syntax: valid YAML"));
            console.log(chalk.green("  ✓ Schema validation: passed"));

            // Report parsed values summary
            const c = result.data;
            console.log(chalk.gray(`    Model: ${c.agent.model}`));
            console.log(chalk.gray(`    Thinking: ${c.agent.thinking}`));
            console.log(chalk.gray(`    Gateway: ${c.gateway.bind}:${c.gateway.port}`));
            console.log(chalk.gray(`    Sandbox profile: ${c.safety.sandbox_profile}`));
            console.log(chalk.gray(`    Rate limit: ${c.rate_limit.requests_per_minute} RPM / ${c.rate_limit.tokens_per_minute} TPM`));
            console.log(chalk.gray(`    Tools: ${Object.entries(c.tools.builtin).filter(([, v]) => v).map(([k]) => k).join(", ")}`));
            console.log(chalk.gray(`    MCP servers: ${Object.keys(c.tools.mcp_servers).length}`));
            console.log(chalk.gray(`    Skills: ${Object.keys(c.skills).length}`));

            // Warnings
            const warnings: string[] = [];
            if (c.agent.max_tokens > 32768) warnings.push("max_tokens > 32768 may cause issues with some models");
            if (c.safety.elevated) warnings.push("Elevated mode is ON — all safety checks bypassed");
            if (c.safety.sandbox_profile === "permissive") warnings.push("Permissive sandbox profile — no confirmation gates");
            if (c.rate_limit.requests_per_minute > 200) warnings.push("High RPM limit — may trigger provider rate limits");

            if (warnings.length > 0) {
              console.log(chalk.yellow("\n  Warnings:"));
              for (const w of warnings) console.log(chalk.yellow(`    ⚠ ${w}`));
            }

            console.log(chalk.green("\n  Config is valid!\n"));
          } else {
            console.log(chalk.green("  ✓ Config syntax: valid YAML"));
            console.log(chalk.red("  ✗ Schema validation: failed"));
            for (const issue of result.error.issues) {
              console.log(chalk.red(`    - ${issue.path.join(".")}: ${issue.message}`));
            }
            console.log();
            process.exit(1);
          }
        } catch (err: any) {
          if (err.name === "YAMLException") {
            console.log(chalk.red(`  ✗ Config syntax: invalid YAML`));
            console.log(chalk.red(`    ${err.message}`));
          } else {
            console.log(chalk.red(`  ✗ Error: ${err.message}`));
          }
          console.log();
          process.exit(1);
        }
        break;
      }
    }

    if (!found) {
      console.log(chalk.yellow("  No config file found. Run 'anyclaw init' to create one.\n"));
    }
  });


// ─── Self-update CLI (Phase 13.4) ───────────────────────────────────────

program
  .command("update")
  .description("Check for updates and self-update AnyClaw")
  .option("--check", "Only check, do not install")
  .action(async (opts) => {
    const currentVersion = program.version() ?? "0.0.0";
    console.log(chalk.cyan(`Current version: ${currentVersion}`));
    try {
      const { execSync: exec } = await import("node:child_process");
      const latest = exec("npm view anyclaw version", { encoding: "utf-8" }).trim();
      if (latest === currentVersion) {
        console.log(chalk.green("Already up to date."));
        return;
      }
      console.log(chalk.yellow(`Latest version: ${latest}`));
      if (opts.check) return;
      console.log(chalk.cyan("Updating..."));
      exec("npm install -g anyclaw@latest", { stdio: "inherit" });
      console.log(chalk.green(`Updated to ${latest}`));
    } catch (e: any) {
      // Package not published yet — offer local rebuild
      console.log(chalk.yellow("Package not found on npm (not published yet)."));
      console.log(chalk.dim("To update from source: git pull && npm run build"));
    }
  });

// ─── Import/Export Config (Phase 13.5) ──────────────────────────────────

program
  .command("config")
  .description("Import or export AnyClaw configuration")
  .argument("<action>", "Action: export, import")
  .argument("[path]", "File path for import/export")
  .action(async (action: string, filePath: string | undefined) => {
    const yamlLib = await import("js-yaml");
    const { writeFileSync: writeFs, readFileSync: readFs, copyFileSync } = await import("node:fs");

    if (action === "export") {
      const dest = filePath || "anyclaw-export.yaml";
      const config = loadConfig();
      writeFs(dest, yamlLib.dump(config, { lineWidth: 120 }), "utf-8");
      console.log(chalk.green(`Config exported to ${dest}`));
    } else if (action === "import") {
      if (!filePath) {
        console.log(chalk.red("Usage: anyclaw config import <path>"));
        process.exit(1);
      }
      if (!existsSync(filePath)) {
        console.log(chalk.red(`File not found: ${filePath}`));
        process.exit(1);
      }
      // Validate before importing
      const raw = readFs(filePath, "utf-8");
      const parsed = yamlLib.load(raw) as Record<string, unknown>;
      try {
        AnyClawConfigSchema.parse(parsed || {});
      } catch (e: any) {
        console.log(chalk.red("Invalid config:"), e.message);
        process.exit(1);
      }
      const target = join(process.cwd(), "anyclaw.yaml");
      copyFileSync(filePath, target);
      console.log(chalk.green(`Config imported to ${target}`));
    } else {
      console.log(chalk.red(`Unknown action: ${action}. Use export or import.`));
    }
  });

// ─── Init command (zero-config) ──────────────────────────────────────────

program
  .command("init")
  .description("Initialize AnyClaw in current directory with default config")
  .action(async () => {
    const configPath = join(process.cwd(), "anyclaw.yaml");
    if (existsSync(configPath)) {
      console.log(chalk.yellow("anyclaw.yaml already exists"));
      return;
    }
    const { generateDefaultConfig } = await import("./config/index.js");
    const { writeFileSync } = await import("node:fs");
    writeFileSync(configPath, generateDefaultConfig(), "utf-8");
    console.log(chalk.green("Created anyclaw.yaml"));
  });

// ─── Team command (Phase 2.4) ─────────────────────────────────────────────────

const teamCommand = program
  .command("team")
  .description("Manage and run team templates (multi-agent pipelines)");

teamCommand
  .command("list")
  .description("List all available team templates (built-in + user-defined)")
  .option("-c, --config <path>", "Path to config file")
  .action((opts) => {
    const { teamManager } = bootstrap({ config: opts.config });
    const templates = teamManager.list();
    console.log(chalk.cyan.bold("\nTeam Templates\n"));
    if (templates.length === 0) {
      console.log(chalk.gray("  No templates found."));
    } else {
      for (const t of templates) {
        const tags = t.tags && t.tags.length ? chalk.gray(`  [${t.tags.join(", ")}]`) : "";
        console.log(`  ${chalk.bold(t.name)} ${chalk.dim(`v${t.version ?? "1.0.0"}`)}${tags}`);
        if (t.description) console.log(chalk.gray(`    ${t.description}`));
        console.log(chalk.dim(`    ${t.agents.length} agent(s) · entry: ${t.entryAgent ?? t.agents[0]?.id ?? "?"}`));
      }
    }
    console.log();
  });

teamCommand
  .command("show")
  .description("Show agents and connections for a team template")
  .argument("<name>", "Template name")
  .option("-c, --config <path>", "Path to config file")
  .action((name: string, opts) => {
    const { teamManager } = bootstrap({ config: opts.config });
    const template = teamManager.get(name);
    if (!template) {
      console.log(chalk.red(`Template not found: ${name}`));
      process.exit(1);
    }
    console.log(chalk.cyan.bold(`\n${template.name}`) + chalk.dim(` v${template.version ?? "1.0.0"}`));
    if (template.description) console.log(chalk.gray(template.description));
    if (template.tags?.length) console.log(chalk.dim(`Tags: ${template.tags.join(", ")}`));

    console.log(chalk.cyan.bold("\nAgents:"));
    for (const agent of template.agents) {
      const entry = agent.id === template.entryAgent ? chalk.green(" ← entry") : "";
      console.log(`  ${chalk.bold(agent.id)}${entry}  ${chalk.dim(agent.role ?? "assistant")}  model: ${chalk.blue(agent.model ?? "default")}`);
      if (agent.soul) console.log(chalk.gray(`    soul: ${agent.soul.slice(0, 80)}...`));
    }

    console.log(chalk.cyan.bold("\nConnections:"));
    if (template.connections.length === 0) {
      console.log(chalk.gray("  (no connections)"));
    } else {
      for (const conn of template.connections) {
        const label = conn.label ? chalk.dim(` "${conn.label}"`) : "";
        console.log(`  ${chalk.bold(conn.from)} → ${chalk.bold(conn.to)}  on:${chalk.green(conn.on ?? "done")}${label}`);
      }
    }
    console.log();
  });

teamCommand
  .command("apply")
  .description("Spawn a team template's agents on the running gateway")
  .argument("<name>", "Template name")
  .option("-c, --config <path>", "Path to config file")
  .option("--run-id <id>", "Custom run ID suffix (default: random)")
  .action(async (name: string, opts) => {
    const { teamManager, orchestrator } = bootstrap({ config: opts.config });
    const template = teamManager.get(name);
    if (!template) {
      console.log(chalk.red(`Template not found: ${name}`));
      process.exit(1);
    }
    const { agentIds, entryAgentId } = teamManager.applyToOrchestrator(
      name,
      orchestrator,
      opts.runId,
    );
    console.log(chalk.green.bold(`\nTeam "${name}" applied\n`));
    console.log(chalk.dim(`Entry agent: ${entryAgentId}\n`));
    console.log(chalk.cyan.bold("Spawned agents:"));
    for (const [role, id] of Object.entries(agentIds)) {
      console.log(`  ${chalk.bold(role)} → ${chalk.blue(id)}`);
    }
    console.log();
  });

teamCommand
  .command("run")
  .description("Run a task through a team pipeline and print the final result")
  .argument("<name>", "Template name")
  .argument("<task>", "Task description to send to the entry agent")
  .option("-c, --config <path>", "Path to config file")
  .action(async (name: string, task: string, opts) => {
    const { teamManager, orchestrator, memory, gsd } = bootstrap({ config: opts.config });
    const template = teamManager.get(name);
    if (!template) {
      console.log(chalk.red(`Template not found: ${name}`));
      process.exit(1);
    }
    const spinner = ora(`Running team "${name}"...`).start();
    try {
      const result = await teamManager.runTemplate(name, task, orchestrator);
      spinner.succeed(`Completed in ${result.steps.length} step(s)  [run: ${result.teamRun}]`);

      if (result.steps.length > 1) {
        console.log(chalk.cyan.bold("\nSteps:"));
        for (const step of result.steps) {
          console.log(chalk.dim(`\n  [${step.agentId}]`));
          console.log(step.result.slice(0, 400) + (step.result.length > 400 ? "…" : ""));
        }
      }

      console.log(chalk.cyan.bold("\nFinal result:"));
      console.log(result.result);
    } catch (err: any) {
      spinner.fail(err.message);
      process.exit(1);
    } finally {
      memory.close();
      gsd.close();
    }
  });

teamCommand
  .command("save")
  .description("Save the current list of built-in templates to the teams directory")
  .option("-c, --config <path>", "Path to config file")
  .action((opts) => {
    const { teamManager } = bootstrap({ config: opts.config });
    let count = 0;
    for (const t of BUILTIN_TEMPLATES) {
      const path = teamManager.save(t);
      console.log(chalk.green(`  Saved: ${path}`));
      count++;
    }
    console.log(chalk.cyan(`\n${count} template(s) saved.\n`));
  });

// ─── Adapters command (Phase 2.5) ────────────────────────────────────────────

const adaptersCommand = program
  .command("adapters")
  .description("List registered Nexusclaw adapters");

adaptersCommand
  .command("list")
  .description("List all registered adapters (type, id, version)")
  .action(async () => {
    const { listAdapters } = await import("./adapters/index.js");
    const adapters = listAdapters();
    console.log(chalk.cyan.bold("\nRegistered Adapters\n"));
    if (adapters.length === 0) {
      console.log(chalk.gray("  No adapters registered."));
    } else {
      for (const a of adapters) {
        console.log(`  ${chalk.bold(a.id)}  ${chalk.blue(a.type)}  ${chalk.dim(`v${a.version}`)}`);
        if (a.description) console.log(chalk.gray(`    ${a.description}`));
      }
    }
    console.log();
  });

// ─── Workflow Commands (Phase 3.2)───────────────────────────────────────────

const workflowCommand = program
  .command("workflow")
  .description("Run multi-agent workflow pipelines");

workflowCommand
  .command("list")
  .description("List available workflows and benchmark suites")
  .action(async () => {
    console.log(chalk.cyan.bold("\nAvailable Workflows\n"));
    console.log(`  ${chalk.bold("research <topic>")}    ${chalk.gray("4-stage research pipeline (planner→researcher→writer→editor)")}`)
    console.log(`  ${chalk.bold("worker-pool <goal>")}  ${chalk.gray("HiClaw-style manager/worker task decomposition")}`);
    console.log(`  ${chalk.bold("benchmark [suite]")}   ${chalk.gray("Run a benchmark suite against the active model")}`);
    console.log(`  ${chalk.bold("stub [docsDir]")}       ${chalk.gray("Build feature stubs from docs and write .nexus/feature-stubs.json")}`);
    const { BUILTIN_SUITES } = await import("./workflows/index.js");
    console.log(chalk.cyan.bold("\nBenchmark Suites\n"));
    for (const s of BUILTIN_SUITES) {
      console.log(`  ${chalk.bold(s.id)}  ${chalk.dim(s.description || '')}  ${chalk.gray(`(${s.tasks.length} tasks)`)}`);
    }
    console.log();
  });

workflowCommand
  .command("stub [docsDir]")
  .description("Generate feature stub catalog from documentation")
  .option("-o, --output <dir>", "Output directory for feature-stubs.json", ".nexus")
  .option("--show-top <n>", "Print top N planned stubs", "20")
  .action(async (docsDir, opts) => {
    const baseDir = docsDir || join(process.cwd(), "docs");

    const defaultDocs = [
      join(baseDir, "NEXUS_AI_PLATFORM_TAXONOMY.md"),
      join(baseDir, "NEXUS_INTEGRATION_ARCHITECTURE.md"),
      join(baseDir, "NEXUSCLAW_MASSIVE_FEATURE_BACKLOG.md"),
      join(baseDir, "NEXUSCLAW_PHASE1_IMPLEMENTATION_PLAN.md"),
      join(baseDir, "NEXUSCLAW_PLUGIN_SPEC.md"),
      join(baseDir, "NEXUSCLAW_PRODUCT_SPEC.md"),
      join(baseDir, "personal_notes.md"),
      join(process.cwd(), "README.md"),
      join(process.cwd(), "ROADMAP.md"),
    ];

    const { FeatureStubScaffolder } = await import("./workflows/index.js");
    const scaffolder = new FeatureStubScaffolder(defaultDocs);
    const { outputPath, catalog } = scaffolder.saveCatalog(opts.output);

    console.log(chalk.green(`\n✓ Feature stub catalog generated: ${outputPath}`));
    console.log(chalk.dim(`  ${scaffolder.renderSummary(catalog)}`));

    const topN = Math.max(1, parseInt(opts.showTop, 10) || 20);
    const top = catalog.stubs.slice(0, topN);

    if (top.length > 0) {
      console.log(chalk.cyan.bold("\nTop Planned Stubs\n"));
      for (const stub of top) {
        console.log(`  ${chalk.bold(stub.id)}  ${stub.title}`);
        console.log(chalk.gray(`    modules: ${stub.modules.join(", ")} | source: ${stub.sourceDoc}:${stub.sourceLine}`));
      }
    } else {
      console.log(chalk.yellow("\nNo feature IDs were parsed from the selected docs."));
    }

    console.log();
  });

workflowCommand
  .command("research <topic>")
  .description("Run a 4-stage research pipeline on a topic")
  .option("-c, --config <path>", "Path to config file")
  .action(async (topic, opts) => {
    const { orchestrator } = bootstrap({ config: opts.config });
    const { ResearchWorkflow } = await import("./workflows/index.js");
    const wf = new ResearchWorkflow();
    console.log(chalk.cyan.bold(`\nResearch: ${topic}\n`));
    console.log(chalk.dim("Starting pipeline: Planner → Researcher → Writer → Editor\n"));
    try {
      const result = await wf.run(topic, orchestrator);
      console.log(chalk.green(`\n✓ Done in ${Math.round((result.durationMs||0)/1000)}s`));
      if (result.outputPath) console.log(chalk.dim(`  Saved: ${result.outputPath}`));
      console.log(chalk.cyan.bold("\n=== Final Document ===\n"));
      console.log(result.finalDocument);
    } catch (err: any) {
      console.error(chalk.red(`✗ Research failed: ${err.message}`));
      process.exit(1);
    }
  });

workflowCommand
  .command("worker-pool <goal>")
  .description("Decompose a goal into parallel worker tasks (HiClaw style)")
  .option("-c, --config <path>", "Path to config file")
  .option("-n, --concurrency <n>", "Number of parallel workers", "3")
  .action(async (goal, opts) => {
    const { orchestrator } = bootstrap({ config: opts.config });
    const { WorkerPool } = await import("./workflows/index.js");
    const pool = new WorkerPool({ concurrency: parseInt(opts.concurrency, 10) || 3 });
    console.log(chalk.cyan.bold(`\nWorker Pool: ${goal}\n`));
    try {
      const result = await pool.run(goal, orchestrator);
      console.log(chalk.dim(`  Tasks: ${result.tasks.length}  Workers: ${result.workerCount}`));
      console.log();
      for (const task of result.tasks) {
        const icon = task.status === 'done' ? chalk.green('✓') : chalk.red('✗');
        console.log(`  ${icon}  ${task.description.slice(0, 70)}`);
      }
      console.log(chalk.cyan.bold("\n=== Summary ===\n"));
      console.log(result.finalSummary);
    } catch (err: any) {
      console.error(chalk.red(`✗ Worker pool failed: ${err.message}`));
      process.exit(1);
    }
  });

workflowCommand
  .command("benchmark [suite]")
  .description("Run a benchmark suite (default: nexus-core)")
  .option("-c, --config <path>", "Path to config file")
  .option("-m, --model <model>", "Override model for benchmarking")
  .action(async (suite, opts) => {
    const { orchestrator } = bootstrap({ config: opts.config });
    const { BenchmarkRunner } = await import("./workflows/index.js");
    const runner = new BenchmarkRunner();
    const suiteId = suite || "nexus-core";
    console.log(chalk.cyan.bold(`\nBenchmark: ${suiteId}\n`));
    try {
      const result = await runner.run(suiteId, orchestrator, { model: opts.model });
      const color = result.percentScore >= 80 ? chalk.green : result.percentScore >= 50 ? chalk.yellow : chalk.red;
      console.log(color(`  Score: ${result.percentScore}%  (${result.totalScore}/${result.maxScore})`));
      console.log();
      for (const t of result.taskResults) {
        const icon = t.score >= 60 ? chalk.green('✓') : chalk.red('✗');
        console.log(`  ${icon}  ${t.taskName.padEnd(30)}  ${String(t.score).padStart(3)}pts  ${t.durationMs}ms`);
      }
      console.log(chalk.dim(`\n  Summary: ${result.summary}`));
    } catch (err: any) {
      console.error(chalk.red(`✗ Benchmark failed: ${err.message}`));
      process.exit(1);
    }
  });

// ─── Manifest Commands (Phase 4.1) ───────────────────────────────────────────

const manifestCommand = program
  .command("manifest")
  .description("Nexus platform manifest utilities");

manifestCommand
  .command("generate")
  .description("Generate a Nexus Tool Manifest (NTM v1) for this service")
  .option("-c, --config <path>", "Path to config file")
  .option("-o, --output <dir>", "Output directory", ".nexus")
  .option("--public-url <url>", "Public URL as issued by Nexus Cloud")
  .option("--tags <tags>", "Comma-separated service tags")
  .action(async (opts) => {
    const { config, tools, marketplace } = bootstrap({ config: opts.config, lightweight: true });
    const { listAdapters: la } = await import("./adapters/index.js");
    const allTools = tools.list().map(t => ({ id: t.name, name: t.name, description: t.description }));
    const allAdapters = la().map(a => a.id);
    const allSkills = (await marketplace.listInstalled()).map(s => s.id);
    const manifest = buildNexusToolManifest(config, {
      tools: allTools,
      adapters: allAdapters,
      skills: allSkills,
      publicUrl: opts.publicUrl,
      tags: opts.tags ? opts.tags.split(",").map((t: string) => t.trim()) : [],
    });
    const outPath = writeNexusToolManifest(manifest, opts.output);
    console.log(chalk.green(`\n✓ Nexus Tool Manifest written to ${outPath}`));
    console.log(chalk.dim(`  Service: ${manifest.serviceName} (${manifest.serviceId})`));
    console.log(chalk.dim(`  Capabilities: ${manifest.capabilities.length}  Adapters: ${manifest.adapters.length}  Skills: ${manifest.skills.length}`));
    console.log();
  });

manifestCommand
  .command("hosting")
  .description("Generate a Nexus Hosting deployment descriptor (.nexus/hosting.json)")
  .option("-c, --config <path>", "Path to config file")
  .option("-o, --output <dir>", "Project root directory", ".")
  .option("--service-name <name>", "Override service name")
  .option("--image <image>", "Docker image reference")
  .option("--domain <hint>", "Domain hint for Nexus Cloud URL issuance")
  .action(async (opts) => {
    const { config } = bootstrap({ config: opts.config, lightweight: true });
    const descriptor = buildHostingDescriptor(config, {
      serviceName: opts.serviceName,
      image: opts.image,
      domainHint: opts.domain,
    });
    const outPath = writeHostingDescriptor(descriptor, opts.output);
    console.log(chalk.green(`\n✓ Hosting descriptor written to ${outPath}`));
    console.log(chalk.dim(`  Service: ${descriptor.serviceName}  Port: ${descriptor.port}`));
    console.log(chalk.dim(`  Required env: ${descriptor.requiredEnv.join(", ")}\n`));
  });

manifestCommand
  .command("show")
  .description("Show the current Nexus Tool Manifest if it exists")
  .option("-d, --dir <dir>", "Directory containing nexus-tool-manifest.json", ".nexus")
  .action(async (opts) => {
    const { readFileSync, existsSync } = await import("node:fs");
    const path = join(opts.dir, "nexus-tool-manifest.json");
    if (!existsSync(path)) {
      console.log(chalk.yellow(`No manifest found at ${path}. Run \`nexusclaw manifest generate\` first.`));
      return;
    }
    const manifest = JSON.parse(readFileSync(path, "utf-8"));
    console.log(chalk.cyan.bold(`\nNexus Tool Manifest v${manifest.ntmVersion}\n`));
    console.log(`  Service:      ${manifest.serviceName} (${manifest.serviceId})`);
    console.log(`  Internal URL: ${manifest.internalUrl}`);
    if (manifest.publicUrl) console.log(`  Public URL:   ${chalk.green(manifest.publicUrl)}`);
    console.log(`  Generated:    ${manifest.generatedAt}`);
    console.log(`  Capabilities: ${manifest.capabilities.length}`);
    console.log(`  Adapters:     ${manifest.adapters.join(", ") || "(none)"}`);
    console.log(`  Skills:       ${manifest.skills.join(", ") || "(none)"}\n`);
  });

// ─── RBAC Commands (Phase 4.2) ─────────────────────────────────────────────

const rbacCommand = program
  .command("rbac")
  .description("Role-based access control management");

rbacCommand
  .command("roles")
  .description("List all roles (built-in + custom)")
  .action(async () => {
    const { rbac: _rbac } = await import("./rbac/index.js");
    const roles = _rbac.listRoles();
    console.log(chalk.cyan.bold(`\nRoles (${roles.length})\n`));
    for (const r of roles) {
      console.log(`  ${chalk.bold(r.id)}  ${chalk.dim(r.name)}`);
      if (r.description) console.log(chalk.gray(`    ${r.description}`));
      console.log(chalk.green(`    allow: ${r.allow.join(", ")}`) );
      if (r.deny?.length) console.log(chalk.red(`    deny:  ${r.deny.join(", ")}`) );
    }
    console.log();
  });

rbacCommand
  .command("policies")
  .description("List all RBAC policies (subject → roles)")
  .action(async () => {
    const { rbac: _rbac } = await import("./rbac/index.js");
    const policies = _rbac.listPolicies();
    console.log(chalk.cyan.bold(`\nPolicies (${policies.length})\n`));
    if (policies.length === 0) {
      console.log(chalk.gray("  No policies configured."));
    } else {
      for (const p of policies) {
        console.log(`  ${chalk.bold(p.subject)}  →  ${p.roles.join(", ")}`);
      }
    }
    console.log();
  });

rbacCommand
  .command("check <subject> <action>")
  .description("Check if a subject can perform an action")
  .action(async (subject, action) => {
    const { rbac: _rbac } = await import("./rbac/index.js");
    const result = _rbac.checkPolicy(subject, action as any);
    if (result.allowed) {
      console.log(chalk.green(`✓ ALLOWED — ${result.reason}`));
    } else {
      console.log(chalk.red(`✗ DENIED  — ${result.reason}`));
      process.exit(1);
    }
  });

// ─── Adapter Subcommands (Phase 3.1 additions) ──────────────────────────────

const adaptersCommand2 = program
  .command("adapter")
  .description("Register and inspect Nexusclaw adapters");

adaptersCommand2
  .command("register-router")
  .description("Register an HTTP model router adapter (ClawRouter-style)")
  .requiredOption("--url <url>", "Base URL of the router (e.g. http://localhost:4000)")
  .option("--api-key <key>", "API key for the router")
  .option("--name <name>", "Display name")
  .action(async (opts) => {
    const { HttpModelRouterAdapter, registerAdapter } = await import("./adapters/index.js");
    const adapter = new HttpModelRouterAdapter({
      baseUrl: opts.url,
      apiKey: opts.apiKey,
      name: opts.name,
    });
    registerAdapter(adapter);
    const models = await adapter.listModels();
    console.log(chalk.green(`✓ Registered router: ${adapter.info.name}`));
    if (models.length > 0) {
      console.log(chalk.dim(`  Models: ${models.slice(0, 8).join(", ")}${models.length > 8 ? ` (+${models.length-8} more)` : ""}\n`));
    }
  });

adaptersCommand2
  .command("register-docker")
  .description("Register a Docker sandbox adapter (NemoClaw-style)")
  .option("--image <image>", "Docker image to use", "node:22-slim")
  .option("--memory <limit>", "Memory limit (e.g. 512m)", "512m")
  .option("--cpus <n>", "CPU limit", "1.0")
  .action(async (opts) => {
    const { DockerSandboxAdapter, registerAdapter } = await import("./adapters/index.js");
    const adapter = new DockerSandboxAdapter({
      image: opts.image,
      memory: opts.memory,
      cpus: opts.cpus,
    });
    registerAdapter(adapter);
    const caps = adapter.capabilities() as any;
    const dockerOk = caps._dockerAvailable;
    const icon = dockerOk ? chalk.green("✓") : chalk.yellow("⚠");
    console.log(`${icon} Registered Docker sandbox: ${adapter.info.name}`);
    if (!dockerOk) console.log(chalk.yellow("  Docker not available — will fall back to local sandbox."));
    console.log();
  });

// ─── Run ────────────────────────────────────────────────────────────────────

// ─── Metrics CLI ────────────────────────────────────────────────────────────

const metricsCommand = program
  .command("metrics")
  .description("Token usage, cost, and tool call statistics");

metricsCommand
  .command("show")
  .description("Print a live metrics snapshot")
  .option("-j, --json", "Output as JSON")
  .action(async (opts) => {
    bootstrap({ lightweight: true });
    const snap = metrics.snapshot();
    if (opts.json) { console.log(JSON.stringify(snap, null, 2)); return; }
    console.log(chalk.bold("\nMetrics Snapshot"));
    console.log(chalk.gray("─".repeat(44)));
    console.log(`  Input tokens :  ${chalk.cyan(snap.totalInputTokens.toLocaleString())}`);
    console.log(`  Output tokens:  ${chalk.cyan(snap.totalOutputTokens.toLocaleString())}`);
    console.log(`  Est. cost    :  ${chalk.yellow("$" + snap.totalCostUsd.toFixed(4))}`);
    console.log(`  Tool calls   :  ${snap.totalToolCalls}  (${snap.failedToolCalls} failed)`);
    console.log(`  Agents       :  ${snap.uniqueAgents}`);
    console.log(`  Sessions     :  ${snap.uniqueSessions}`);
    if (snap.topModels.length) {
      console.log(chalk.bold("\nTop models:"));
      for (const m of snap.topModels.slice(0, 5)) {
        console.log(`  ${chalk.cyan(m.model.padEnd(35))} in=${m.inputTokens.toLocaleString()} out=${m.outputTokens.toLocaleString()} cost=$${m.costUsd.toFixed(4)}`);
      }
    }
    if (snap.topTools.length) {
      console.log(chalk.bold("\nTop tools:"));
      for (const t of snap.topTools.slice(0, 8)) {
        const fail = t.failRate > 0 ? chalk.red(` (${(t.failRate * 100).toFixed(0)}% fail)`) : "";
        console.log(`  ${t.name.padEnd(30)} ${t.calls} calls${fail}`);
      }
    }
    if (snap.avgLlmLatencyMs) console.log(`\n  Avg LLM latency:  ${snap.avgLlmLatencyMs.toFixed(0)}ms`);
    if (snap.avgToolLatencyMs) console.log(`  Avg tool latency: ${snap.avgToolLatencyMs.toFixed(0)}ms`);
    console.log();
  });

metricsCommand
  .command("reset")
  .description("Clear all in-memory metrics (does not delete the JSONL file)")
  .action(async () => {
    bootstrap({ lightweight: true });
    metrics.reset();
    console.log(chalk.green("✓ Metrics reset"));
  });

// ─── Webhooks CLI ────────────────────────────────────────────────────────────

const webhooksCommand = program
  .command("webhooks")
  .description("Manage outbound webhook subscriptions");

webhooksCommand
  .command("list")
  .description("List all registered webhooks")
  .action(() => {
    bootstrap({ lightweight: true });
    const all = webhooks.list();
    if (!all.length) { console.log(chalk.gray("No webhooks registered.")); return; }
    console.log(chalk.bold(`\nWebhooks (${all.length}):`));
    for (const h of all) {
      const status = h.enabled ? chalk.green("enabled") : chalk.red("disabled");
      console.log(`  ${chalk.cyan(h.id)}  ${status}  ${h.url}`);
      console.log(`    events: ${h.events.join(", ")}`);
      if (h.description) console.log(`    desc: ${h.description}`);
      if (h.lastDeliveryAt) console.log(`    last delivery: ${new Date(h.lastDeliveryAt).toISOString()}  status=${h.lastDeliveryStatus ?? "?"}`);
    }
    console.log();
  });

webhooksCommand
  .command("add <url>")
  .description("Register a new webhook")
  .option("-e, --events <events>", "Comma-separated event list (default: *)", "*")
  .option("-s, --secret <secret>", "HMAC signing secret")
  .option("-d, --description <desc>", "Description")
  .action((url, opts) => {
    bootstrap({ lightweight: true });
    const events = (opts.events as string).split(",").map(e => e.trim()) as WebhookEvent[];
    const entry = webhooks.register({ url, events, secret: opts.secret, description: opts.description });
    console.log(chalk.green(`✓ Registered webhook: ${entry.id}`));
    console.log(`  URL: ${url}  events: ${events.join(", ")}`);
  });

webhooksCommand
  .command("remove <id>")
  .description("Unregister a webhook")
  .action((id) => {
    bootstrap({ lightweight: true });
    const ok = webhooks.unregister(id);
    console.log(ok ? chalk.green(`✓ Removed ${id}`) : chalk.red(`Webhook not found: ${id}`));
  });

webhooksCommand
  .command("enable <id>")
  .description("Re-enable a disabled webhook")
  .action((id) => {
    bootstrap({ lightweight: true });
    const ok = webhooks.enable_hook(id);
    console.log(ok ? chalk.green(`✓ Enabled ${id}`) : chalk.red(`Webhook not found: ${id}`));
  });

webhooksCommand
  .command("disable <id>")
  .description("Disable a webhook without deleting it")
  .action((id) => {
    bootstrap({ lightweight: true });
    const ok = webhooks.disable_hook(id);
    console.log(ok ? chalk.yellow(`⏸ Disabled ${id}`) : chalk.red(`Webhook not found: ${id}`));
  });

webhooksCommand
  .command("test <id>")
  .description("Send a test ping event to a webhook")
  .action(async (id) => {
    bootstrap({ lightweight: true });
    const hook = webhooks.get(id);
    if (!hook) { console.log(chalk.red(`Webhook not found: ${id}`)); return; }
    const spinner = ora(`Sending test ping to ${hook.url}...`).start();
    try {
      const results = await webhooks.dispatch("*" as WebhookEvent, { test: true, webhookId: id, source: "nexusclaw-cli" });
      const r = results.find(x => x.webhookId === id);
      if (r?.ok) { spinner.succeed(`Delivered — status ${r.status} in ${r.durationMs}ms`); }
      else { spinner.fail(`Failed — status ${r?.status ?? "ERR"}: ${r?.error ?? ""}`); }
    } catch (err: unknown) {
      spinner.fail(`Error: ${err instanceof Error ? err.message : String(err)}`);
    }
  });

// ─── Session Export (Phase 6) ────────────────────────────────────────────────

const sessionCommand = program
  .command("session")
  .description("Manage and export chat sessions");

sessionCommand
  .command("list")
  .description("List recent sessions")
  .option("-n, --limit <n>", "Max sessions to show", "20")
  .action((opts) => {
    const ctx = bootstrap({ lightweight: true });
    const sessions = ctx.memory.listSessions({ limit: parseInt(opts.limit) });
    if (sessions.length === 0) {
      console.log(chalk.gray("No sessions found"));
      return;
    }
    console.log(chalk.cyan(`\nRecent Sessions (${sessions.length})\n`));
    for (const s of sessions) {
      const age = Math.round((Date.now() - s.updatedAt) / 60000);
      const ageStr = age < 60 ? `${age}m ago` : age < 1440 ? `${Math.round(age/60)}h ago` : `${Math.round(age/1440)}d ago`;
      console.log(
        chalk.bold(s.id.slice(0, 12)) + "  " +
        chalk.gray(`[${s.agentId}]`) + "  " +
        chalk.white(`${s.messageCount} msgs`) + "  " +
        chalk.gray(ageStr),
      );
    }
  });

sessionCommand
  .command("export <sessionId>")
  .description("Export a session transcript")
  .option("-f, --format <fmt>", "Output format: json | md | txt", "md")
  .option("-o, --output <path>", "Write to file instead of stdout")
  .action(async (sessionId, opts) => {
    const ctx = bootstrap({ lightweight: true });
    // Support short IDs (prefix match)
    const all = ctx.memory.listSessions({ limit: 1000 });
    const match = all.find((s) => s.id === sessionId || s.id.startsWith(sessionId));
    if (!match) {
      console.error(chalk.red(`Session not found: ${sessionId}`));
      process.exit(1);
    }
    const messages = ctx.memory.getSessionMessages(match.id);
    let output = "";
    if (opts.format === "json") {
      output = JSON.stringify({ sessionId: match.id, agentId: match.agentId, messages }, null, 2);
    } else if (opts.format === "txt") {
      output = messages
        .filter((m: any) => m.role !== "system")
        .map((m: any) => `[${m.role.toUpperCase()}]\n${m.content}`)
        .join("\n\n---\n\n");
    } else {
      // markdown (default)
      output = `# Session: ${match.id}\n\nAgent: \`${match.agentId}\`  \nMessages: ${messages.length}\n\n---\n\n`;
      output += messages
        .filter((m: any) => m.role !== "system")
        .map((m: any) => `### ${m.role === "user" ? "You" : "Assistant"}\n\n${m.content}`)
        .join("\n\n---\n\n");
    }
    if (opts.output) {
      const { writeFileSync } = await import("node:fs");
      writeFileSync(opts.output, output, "utf8");
      console.log(chalk.green(`✓ Exported to ${opts.output}`));
    } else {
      console.log(output);
    }
  });

// ─── Logs (Phase 7, 10.4) ─────────────────────────────────────────────────────
const logsCmd = program.command("logs").description("View structured logs");

logsCmd
  .command("tail")
  .description("Tail the structured log file")
  .option("-n, --lines <n>", "Number of lines to show", "50")
  .option("-l, --level <level>", "Filter by minimum level (debug|info|warn|error)")
  .option("-c, --component <name>", "Filter by component name")
  .option("-f, --follow", "Follow (watch) the log file for new entries")
  .action(async (opts: { lines: string; level?: string; component?: string; follow?: boolean }) => {
    const { readFileSync, existsSync, watch } = await import("node:fs");
    const logFile = defaultLogFile();
    if (!existsSync(logFile)) {
      console.log(chalk.yellow(`No log file found at ${logFile}`));
      return;
    }
    const levelOrder: Record<string, number> = { debug: 0, info: 1, warn: 2, error: 3 };
    const minLevel = opts.level ? levelOrder[opts.level] ?? 1 : 0;
    const levelColors: Record<string, (s: string) => string> = {
      debug: chalk.gray,
      info: chalk.cyan,
      warn: chalk.yellow,
      error: chalk.red,
    };

    const printLine = (raw: string) => {
      const trimmed = raw.trim();
      if (!trimmed) return;
      try {
        const entry = JSON.parse(trimmed);
        if ((levelOrder[entry.level] ?? 0) < minLevel) return;
        if (opts.component && entry.component !== opts.component) return;
        const ts = entry.ts ? new Date(entry.ts).toLocaleTimeString() : "";
        const lvlColor = levelColors[entry.level] ?? chalk.white;
        const comp = entry.component ? chalk.magenta(`[${entry.component}]`) : "";
        const { ts: _ts, level, component, msg, ...rest } = entry;
        const extra = Object.keys(rest).length ? " " + chalk.gray(JSON.stringify(rest)) : "";
        console.log(`${chalk.dim(ts)} ${lvlColor(level.toUpperCase().padEnd(5))} ${comp} ${entry.msg}${extra}`);
      } catch {
        console.log(trimmed);
      }
    };

    const lines = readFileSync(logFile, "utf8").split("\n");
    const n = parseInt(opts.lines, 10) || 50;
    const slice = lines.slice(-n - 1);
    slice.forEach(printLine);

    if (opts.follow) {
      console.log(chalk.dim(`--- watching ${logFile} ---`));
      let buf = "";
      watch(logFile, { persistent: true }, () => {
        const content = readFileSync(logFile, "utf8");
        const newContent = content.slice(buf.length);
        buf = content;
        newContent.split("\n").forEach(printLine);
      });
      buf = readFileSync(logFile, "utf8");
    }
  });

logsCmd
  .command("path")
  .description("Print the path to the current log file")
  .action(() => {
    console.log(defaultLogFile());
  });

// ─── Secrets (Phase 8, 9.4) ──────────────────────────────────────────────────
const secretsCmd = program.command("secrets").description("Manage encrypted secrets");

secretsCmd
  .command("set <key> <value>")
  .description("Store an encrypted secret")
  .action((key: string, value: string) => {
    setSecret(key, value);
    console.log(chalk.green(`✓ Secret '${key}' saved`));
  });

secretsCmd
  .command("get <key>")
  .description("Retrieve a secret value")
  .action((key: string) => {
    const val = getSecret(key);
    if (val === undefined) {
      console.log(chalk.yellow(`Secret '${key}' not found`));
      process.exit(1);
    }
    console.log(val);
  });

secretsCmd
  .command("delete <key>")
  .description("Delete a secret")
  .action((key: string) => {
    const ok = deleteSecret(key);
    if (ok) console.log(chalk.green(`✓ Secret '${key}' deleted`));
    else console.log(chalk.yellow(`Secret '${key}' not found`));
  });

secretsCmd
  .command("list")
  .description("List all secret keys (values are never shown)")
  .action(() => {
    const keys = listSecrets();
    if (keys.length === 0) {
      console.log(chalk.dim("No secrets stored yet. Use `anyclaw secrets set KEY VALUE`"));
      return;
    }
    console.log(chalk.bold("Stored secrets:"));
    keys.forEach(k => console.log("  " + chalk.cyan(k)));
  });

secretsCmd
  .command("path")
  .description("Print the path to the secrets store")
  .action(() => {
    console.log(secretsFile());
  });

program.parse();
