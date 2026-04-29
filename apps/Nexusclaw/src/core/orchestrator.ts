// Multi-agent orchestrator
// Phase 7.3: Agent routing rules
// Phase 7.4: Agent delegation
// Phase 7.6: Agent-to-agent messaging
// Phase 5: Skill management
// Phase 17.1: Agent swarms (foundation)

import { randomUUID } from "node:crypto";
import type { AgentConfig, Skill } from "./types.js";
import { Agent, type AgentDeps, type AgentResponse } from "./agent.js";
import type { LLMRouter } from "../llm/index.js";
import type { ToolRegistry } from "../tools/index.js";
import type { MemoryManager } from "../memory/index.js";
import type { GSDManager } from "../gsd/index.js";
import type { SafetyManager } from "../safety/index.js";
import type { HookRunner } from "../hooks/index.js";
import { webhooks } from "../webhooks/index.js";

export interface RoutingRule {
  id: string;
  /** Glob-style prefix or regex string (if wrapped in /.../) matched against the message */
  pattern: string;
  /** Target agent id */
  agentId: string;
  /** Higher priority rules are checked first (default: 0) */
  priority: number;
  /** Human-readable description */
  description?: string;
}

export class Orchestrator {
  private agents = new Map<string, Agent>();
  private stoppedAgents = new Set<string>();
  private skills: Skill[] = [];
  private routingRules: RoutingRule[] = [];
  private llm: LLMRouter;
  private tools: ToolRegistry;
  private memory: MemoryManager;
  private gsd: GSDManager;
  private safety: SafetyManager;
  private hooks?: HookRunner;

  constructor(deps: {
    llm: LLMRouter;
    tools: ToolRegistry;
    memory: MemoryManager;
    gsd: GSDManager;
    safety: SafetyManager;
    hooks?: HookRunner;
  }) {
    this.llm = deps.llm;
    this.tools = deps.tools;
    this.memory = deps.memory;
    this.gsd = deps.gsd;
    this.safety = deps.safety;
    this.hooks = deps.hooks;
  }

  // ─── Agent Lifecycle ──────────────────────────────────────────────

  createAgent(config: AgentConfig): Agent {
    const agent = new Agent({
      config,
      llm: this.llm,
      tools: this.tools,
      memory: this.memory,
      gsd: this.gsd,
      safety: this.safety,
      hooks: this.hooks,
    });
    // Load skills into agent
    if (this.skills.length > 0) {
      agent.loadSkills(this.skills);
    }
    this.agents.set(agent.id, agent);
    webhooks.emit("agent.started", { agentId: agent.id, name: config.name, model: config.model });
    return agent;
  }

  getAgent(id: string): Agent | undefined {
    return this.agents.get(id);
  }

  removeAgent(id: string): boolean {
    this.stoppedAgents.delete(id);
    return this.agents.delete(id);
  }

  /** Soft-stop: mark agent as stopped (will not accept new messages). */
  stopAgent(id: string): boolean {
    if (!this.agents.has(id)) return false;
    this.stoppedAgents.add(id);
    webhooks.emit("agent.stopped", { agentId: id });
    return true;
  }

  /** Resume a previously stopped agent. */
  restartAgent(id: string): boolean {
    if (!this.stoppedAgents.has(id)) return false;
    this.stoppedAgents.delete(id);
    webhooks.emit("agent.started", { agentId: id, resumed: true });
    return true;
  }

  isAgentStopped(id: string): boolean {
    return this.stoppedAgents.has(id);
  }

  getDefaultAgent(): Agent {
    if (this.agents.size === 0) {
      return this.createAgent({
        id: "main",
        name: "AnyClaw",
        model: "anthropic/claude-sonnet-4-6",
      });
    }
    return this.agents.values().next().value!;
  }

  listAgents(): { id: string; name: string; state: string; stopped: boolean }[] {
    return [...this.agents.values()].map((a) => ({
      id: a.id,
      name: a.config.name,
      state: a.getState().phase,
      stopped: this.stoppedAgents.has(a.id),
    }));
  }

  // ─── Skills Management (Phase 5) ─────────────────────────────────

  registerSkill(skill: Skill): void {
    this.skills.push(skill);
    // Push to all existing agents
    for (const agent of this.agents.values()) {
      agent.loadSkills([skill]);
    }
  }

  listSkills(): Skill[] {
    return [...this.skills];
  }

  // ─── Routing Rules (Phase 7.3) ───────────────────────────────────

  addRoutingRule(rule: Omit<RoutingRule, "id">): string {
    const id = randomUUID();
    this.routingRules.push({ ...rule, id });
    // Keep sorted by priority descending
    this.routingRules.sort((a, b) => b.priority - a.priority);
    return id;
  }

  removeRoutingRule(ruleId: string): boolean {
    const before = this.routingRules.length;
    this.routingRules = this.routingRules.filter((r) => r.id !== ruleId);
    return this.routingRules.length < before;
  }

  listRoutingRules(): RoutingRule[] {
    return [...this.routingRules];
  }

  /** Match a message against routing rules; returns first matched agentId or undefined */
  private matchRoute(message: string): string | undefined {
    for (const rule of this.routingRules) {
      if (!this.agents.has(rule.agentId)) continue;
      const p = rule.pattern;
      if (p.startsWith("/") && p.endsWith("/")) {
        // Regex pattern
        try {
          if (new RegExp(p.slice(1, -1), "i").test(message)) return rule.agentId;
        } catch {
          // invalid regex — skip
        }
      } else if (p.includes("*")) {
        // Glob prefix — convert * to .*
        if (new RegExp("^" + p.replace(/[.+^${}()|[\]\\]/g, "\\$&").replace(/\*/g, ".*") + "$", "i").test(message))
          return rule.agentId;
      } else {
        // Substring match
        if (message.toLowerCase().includes(p.toLowerCase())) return rule.agentId;
      }
    }
    return undefined;
  }

  // ─── Message Routing ──────────────────────────────────────────────

  async sendMessage(
    message: string,
    agentId?: string,
    sessionId?: string,
  ): Promise<AgentResponse> {
    // Explicit agentId takes precedence; if none, check routing rules; else use default
    const resolvedId = agentId ?? this.matchRoute(message);
    const agent = resolvedId
      ? this.agents.get(resolvedId)
      : this.getDefaultAgent();

    if (!agent) throw new Error(`Agent not found: ${resolvedId}`);
    if (this.stoppedAgents.has(agent.id))
      throw new Error(`Agent "${agent.id}" is stopped. Use 'agents restart' to resume it.`);
    return agent.processMessage(message, sessionId);
  }

  // ─── Agent-to-Agent Messaging (Phase 7.6) ────────────────────────

  async sendToAgent(
    targetAgentId: string,
    message: string,
    fromAgentId?: string,
    sessionId?: string,
  ): Promise<AgentResponse> {
    const target = this.agents.get(targetAgentId);
    if (!target) throw new Error(`Target agent not found: ${targetAgentId}`);
    if (this.stoppedAgents.has(targetAgentId))
      throw new Error(`Agent "${targetAgentId}" is stopped`);

    // Prefix the message with sender context so the target agent knows the origin
    const prefixed = fromAgentId
      ? `[Message from agent ${fromAgentId}]\n${message}`
      : message;

    webhooks.emit("agent.message", { from: fromAgentId ?? "external", to: targetAgentId, preview: message.slice(0, 100) });
    return target.processMessage(prefixed, sessionId);
  }

  // ─── Agent Delegation (Phase 7.4) ────────────────────────────────

  async delegate(
    fromAgentId: string,
    task: string,
    delegateConfig: Partial<AgentConfig> & { name: string },
  ): Promise<AgentResponse> {
    const parent = this.agents.get(fromAgentId);
    const parentConfig = parent?.config;

    const subAgent = this.createAgent({
      id: `delegate-${delegateConfig.name}-${Date.now()}`,
      model: delegateConfig.model || parentConfig?.model || "anthropic/claude-sonnet-4-6",
      ...delegateConfig,
    });

    const response = await subAgent.processMessage(task);

    // Ingest sub-agent session into memory
    subAgent.ingestSession(response.sessionId);

    // Clean up sub-agent
    this.agents.delete(subAgent.id);

    return response;
  }

  getRateLimitStats() {
    return this.llm.getRateLimitStats();
  }

  getProviders(): string[] {
    return this.llm.getProviders();
  }

  // ─── Sub-Agent Spawning ───────────────────────────────────────────

  spawnSubAgent(
    parentId: string,
    config: Partial<AgentConfig> & { name: string },
  ): Agent {
    const parent = this.agents.get(parentId);
    const parentConfig = parent?.config;

    return this.createAgent({
      id: `sub-${config.name}-${Date.now()}`,
      model: config.model || parentConfig?.model || "anthropic/claude-sonnet-4-6",
      ...config,
    });
  }

  // ─── Swarm (Phase 17.1 foundation) ───────────────────────────────

  async swarm(
    tasks: { message: string; agentConfig: Partial<AgentConfig> & { name: string } }[],
  ): Promise<AgentResponse[]> {
    // Execute tasks in parallel with separate agents
    const promises = tasks.map(async (t) => {
      const agent = this.createAgent({
        id: `swarm-${t.agentConfig.name}-${Date.now()}`,
        model: t.agentConfig.model || "anthropic/claude-sonnet-4-6",
        ...t.agentConfig,
      });
      const response = await agent.processMessage(t.message);
      agent.ingestSession(response.sessionId);
      this.agents.delete(agent.id);
      return response;
    });

    return Promise.all(promises);
  }
}
