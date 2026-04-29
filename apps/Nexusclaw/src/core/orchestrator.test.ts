import { describe, it, expect, vi, beforeEach } from "vitest";
import { Orchestrator } from "./orchestrator.js";

// Minimal mock deps — we only need the routing rule logic (no LLM/DB)
function makeMockDeps() {
  return {
    llm: { chat: vi.fn(), streamChat: vi.fn(), getRateLimitStats: vi.fn(() => ({})), getProviders: vi.fn(() => []) },
    tools: { list: vi.fn(() => []), register: vi.fn(), get: vi.fn() },
    memory: {
      createSession: vi.fn((agentId: string) => ({ id: "sess-1", agentId, messages: [], createdAt: Date.now() })),
      getSession: vi.fn(),
      getSessionMessages: vi.fn(() => []),
      pushMessage: vi.fn(),
      getVaultContext: vi.fn(() => ""),
      autoFlushToVault: vi.fn(),
      getStatus: vi.fn(() => ({})),
      ingestSession: vi.fn(() => 0),
    },
    gsd: { listTasks: vi.fn(() => []), getKanbanContext: vi.fn(() => "") },
    safety: { check: vi.fn(async () => ({ allowed: true, reason: "" })) },
  } as any;
}

describe("Orchestrator routing rules (7.3)", () => {
  let orch: Orchestrator;

  beforeEach(() => {
    orch = new Orchestrator(makeMockDeps());
    // Create a couple of agents
    const a1Config = { id: "agent1", name: "Agent One", model: "mock/dev" };
    const a2Config = { id: "agent2", name: "Agent Two", model: "mock/dev" };
    // Manually register agents via createAgent — but createAgent creates an Agent which calls
    // webhooks. We'll suppress that. The important thing is getAgent/listAgents work.
    // Use the internal agents map through createAgent.
    orch.createAgent(a1Config);
    orch.createAgent(a2Config);
  });

  it("adds and lists routing rules", () => {
    const id1 = orch.addRoutingRule({ pattern: "hello", agentId: "agent1", priority: 0 });
    const id2 = orch.addRoutingRule({ pattern: "/^code.*/i", agentId: "agent2", priority: 10 });
    const rules = orch.listRoutingRules();
    expect(rules).toHaveLength(2);
    // Higher priority first
    expect(rules[0].id).toBe(id2);
    expect(rules[1].id).toBe(id1);
  });

  it("removes a routing rule", () => {
    const id = orch.addRoutingRule({ pattern: "foo", agentId: "agent1", priority: 0 });
    expect(orch.removeRoutingRule(id)).toBe(true);
    expect(orch.listRoutingRules()).toHaveLength(0);
  });

  it("returns false when removing non-existent rule", () => {
    expect(orch.removeRoutingRule("ghost")).toBe(false);
  });

  it("rules are sorted by priority descending", () => {
    orch.addRoutingRule({ pattern: "low", agentId: "agent1", priority: 1 });
    orch.addRoutingRule({ pattern: "high", agentId: "agent2", priority: 100 });
    orch.addRoutingRule({ pattern: "mid", agentId: "agent1", priority: 50 });
    const rules = orch.listRoutingRules();
    expect(rules[0].priority).toBe(100);
    expect(rules[1].priority).toBe(50);
    expect(rules[2].priority).toBe(1);
  });

  it("exposes listAgents with expected shape", () => {
    const agents = orch.listAgents();
    expect(agents.length).toBe(2);
    expect(agents[0]).toMatchObject({ id: expect.any(String), name: expect.any(String), state: expect.any(String) });
  });

  it("stopAgent and restartAgent work", () => {
    const agents = orch.listAgents();
    const id = agents[0].id;
    expect(orch.stopAgent(id)).toBe(true);
    expect(orch.isAgentStopped(id)).toBe(true);
    expect(orch.restartAgent(id)).toBe(true);
    expect(orch.isAgentStopped(id)).toBe(false);
  });
});
