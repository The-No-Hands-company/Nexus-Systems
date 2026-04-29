// Team Templates — Phase 2.4
// Reusable agent graph definitions that can be applied to the Orchestrator.
// A TeamTemplate declares a set of agents with roles, personalities, and
// how tasks route between them.  Templates can be loaded from YAML/JSON files
// in ~/.anyclaw/teams/ or from inline config.

import { readFileSync, readdirSync, existsSync, writeFileSync, mkdirSync } from "node:fs";
import { join } from "node:path";
import yaml from "js-yaml";
import { randomUUID } from "node:crypto";
import type { AgentConfig, TeamTemplate, TeamAgent, TeamConnection } from "../core/types.js";
import type { Orchestrator } from "../core/orchestrator.js";

export { type TeamTemplate, type TeamAgent, type TeamConnection };

// ─── TeamManager ───────────────────────────────────────────────────────────

export class TeamManager {
  private templates = new Map<string, TeamTemplate>();
  private teamsDir: string;

  constructor(teamsDir: string) {
    this.teamsDir = teamsDir;
  }

  // ── Persistence ─────────────────────────────────────────────────────────

  /** Load all templates from the teams directory. */
  loadAll(): TeamTemplate[] {
    if (!existsSync(this.teamsDir)) return [];
    const files = readdirSync(this.teamsDir).filter(
      (f) => f.endsWith(".yaml") || f.endsWith(".yml") || f.endsWith(".json"),
    );
    const loaded: TeamTemplate[] = [];
    for (const file of files) {
      try {
        const t = this.loadFromFile(join(this.teamsDir, file));
        if (t) loaded.push(t);
      } catch {
        // skip malformed files
      }
    }
    return loaded;
  }

  loadFromFile(filePath: string): TeamTemplate | null {
    const raw = readFileSync(filePath, "utf-8");
    const data = filePath.endsWith(".json")
      ? JSON.parse(raw)
      : (yaml.load(raw) as Record<string, unknown>);
    return this.parseTemplate(data);
  }

  register(template: TeamTemplate): void {
    this.templates.set(template.name, template);
  }

  get(name: string): TeamTemplate | undefined {
    return this.templates.get(name);
  }

  list(): TeamTemplate[] {
    return [...this.templates.values()];
  }

  save(template: TeamTemplate): string {
    mkdirSync(this.teamsDir, { recursive: true });
    const filePath = join(this.teamsDir, `${template.name}.yaml`);
    writeFileSync(filePath, yaml.dump(template), "utf-8");
    this.templates.set(template.name, template);
    return filePath;
  }

  // ── Template parsing ──────────────────────────────────────────────────

  private parseTemplate(data: Record<string, unknown>): TeamTemplate | null {
    if (!data?.name || !Array.isArray(data?.agents)) return null;
    return {
      name: data.name as string,
      description: (data.description as string) || "",
      agents: (data.agents as any[]).map((a) => ({
        id: a.id || a.name.toLowerCase().replace(/\s+/g, "-"),
        name: a.name || a.id,
        role: a.role || "assistant",
        model: a.model || "nexus-ai/auto",
        soul: a.soul || a.system_prompt,
        allowedTools: a.allowed_tools || a.allowedTools || [],
        deniedTools: a.denied_tools || a.deniedTools || [],
        temperature: a.temperature,
        maxTokens: a.max_tokens || a.maxTokens,
      })),
      connections: ((data.connections as any[]) || []).map((c) => ({
        from: c.from,
        to: c.to,
        on: c.on || "done",
        label: c.label,
      })),
      entryAgent: (data.entry_agent || data.entryAgent) as string | undefined,
      tags: (data.tags as string[]) || [],
      version: (data.version as string) || "1.0.0",
    };
  }

  // ── Apply to Orchestrator ────────────────────────────────────────────────

  /**
   * Spawn all agents defined in a template.
   * Returns a map of role → agent id suffix so the caller can wire messages.
   */
  applyToOrchestrator(
    templateName: string,
    orchestrator: Orchestrator,
    runId?: string,
  ): { agentIds: Record<string, string>; entryAgentId: string } {
    const template = this.templates.get(templateName);
    if (!template) throw new Error(`Team template not found: ${templateName}`);

    const suffix = runId ?? randomUUID().slice(0, 8);
    const agentIds: Record<string, string> = {};

    for (const tAgent of template.agents) {
      const agentId = `${tAgent.id}-${suffix}`;
      agentIds[tAgent.id] = agentId;

      const config: AgentConfig = {
        id: agentId,
        name: tAgent.name || tAgent.id,
        model: tAgent.model || "nexus-ai/auto",
        soul: tAgent.soul,
        allowedTools: tAgent.allowedTools,
        deniedTools: tAgent.deniedTools,
        temperature: tAgent.temperature,
        maxTokens: tAgent.maxTokens,
        persona: tAgent.role ? { role: tAgent.role } : undefined,
      };

      orchestrator.createAgent(config);
    }

    const entryId = template.entryAgent
      ? agentIds[template.entryAgent]
      : Object.values(agentIds)[0];

    return { agentIds, entryAgentId: entryId };
  }

  /**
   * Run a team template: spawn all agents and route the task through the
   * entry agent.  Returns the final response from the last agent in the graph.
   */
  async runTemplate(
    templateName: string,
    task: string,
    orchestrator: Orchestrator,
  ): Promise<{
    result: string;
    agentId: string;
    teamRun: string;
    steps: Array<{ agentId: string; result: string }>;
  }> {
    const template = this.templates.get(templateName);
    if (!template) throw new Error(`Team template not found: ${templateName}`);

    const teamRun = randomUUID().slice(0, 8);
    const { agentIds, entryAgentId } = this.applyToOrchestrator(templateName, orchestrator, teamRun);
    const steps: Array<{ agentId: string; result: string }> = [];

    let currentAgentId = entryAgentId;
    let currentMessage = task;
    const visited = new Set<string>();

    // Walk the connection graph up to N hops
    const maxHops = template.agents.length + 1;
    for (let i = 0; i < maxHops; i++) {
      if (visited.has(currentAgentId)) break;
      visited.add(currentAgentId);

      const response = await orchestrator.sendMessage(currentMessage, currentAgentId);
      const result = response.content ?? "";
      steps.push({ agentId: currentAgentId, result });

      // Find the template agent id (strip suffix) to look up connections
      const templateAgentId = Object.entries(agentIds).find(
        ([, v]) => v === currentAgentId,
      )?.[0];

      const nextConnection = templateAgentId
        ? template.connections.find(
            (c) => c.from === templateAgentId && c.on === "done",
          )
        : undefined;

      if (!nextConnection) break;

      const nextAgentId = agentIds[nextConnection.to];
      if (!nextAgentId) break;

      currentMessage = result; // pass output as input to next agent
      currentAgentId = nextAgentId;
    }

    // Clean up spawned agents
    for (const agentId of Object.values(agentIds)) {
      orchestrator.removeAgent(agentId);
    }

    const lastStep = steps[steps.length - 1];
    return {
      result: lastStep?.result ?? "",
      agentId: lastStep?.agentId ?? entryAgentId,
      teamRun,
      steps,
    };
  }
}

// ─── Built-in templates ─────────────────────────────────────────────────────

export const BUILTIN_TEMPLATES: TeamTemplate[] = [
  {
    name: "research-writer",
    description: "Researcher gathers information, Writer synthesises it into a polished document.",
    version: "1.0.0",
    tags: ["research", "writing", "pipeline"],
    entryAgent: "researcher",
    agents: [
      {
        id: "researcher",
        name: "Researcher",
        role: "researcher",
        model: "nexus-ai/auto",
        soul: "You are an expert researcher. Your job is to thoroughly investigate the given topic and produce a detailed research brief with key facts, context, and source notes.",
        allowedTools: ["web_fetch", "web_search"],
      },
      {
        id: "writer",
        name: "Writer",
        role: "writer",
        model: "nexus-ai/auto",
        soul: "You are a technical writer. Given a research brief, produce a clear, well-structured document or report. Use headings, be concise, and cite key points.",
      },
    ],
    connections: [{ from: "researcher", to: "writer", on: "done", label: "brief → document" }],
  },
  {
    name: "code-review",
    description: "Developer writes code, Reviewer analyses it for bugs and improvements.",
    version: "1.0.0",
    tags: ["engineering", "review", "pipeline"],
    entryAgent: "developer",
    agents: [
      {
        id: "developer",
        name: "Developer",
        role: "developer",
        model: "nexus-ai/auto",
        soul: "You are a senior software engineer. Write clean, idiomatic code to solve the given problem. Include brief inline comments for non-obvious logic.",
        allowedTools: ["shell", "read_file", "write_file"],
      },
      {
        id: "reviewer",
        name: "Reviewer",
        role: "reviewer",
        model: "nexus-ai/auto",
        soul: "You are a rigorous code reviewer. Analyse the code for bugs, security issues, performance problems, and style violations. Give concrete, actionable feedback.",
      },
    ],
    connections: [{ from: "developer", to: "reviewer", on: "done", label: "code → review" }],
  },
  {
    name: "planner-executor",
    description: "Planner breaks a goal into tasks, Executor carries them out one by one.",
    version: "1.0.0",
    tags: ["orchestration", "planning", "execution"],
    entryAgent: "planner",
    agents: [
      {
        id: "planner",
        name: "Planner",
        role: "planner",
        model: "nexus-ai/auto",
        soul: "You are a strategic planning agent. Given a high-level goal, produce a numbered step-by-step plan. Each step should be a concrete, actionable task. Output ONLY the plan as a numbered list.",
      },
      {
        id: "executor",
        name: "Executor",
        role: "executor",
        model: "nexus-ai/auto",
        soul: "You are an execution agent. You receive a numbered plan and carry out each step in sequence. Use available tools. Report what you did for each step.",
        allowedTools: ["shell", "read_file", "write_file", "web_fetch"],
      },
    ],
    connections: [{ from: "planner", to: "executor", on: "done", label: "plan → execution" }],
  },
];
