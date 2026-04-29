// Phase 4.2 — Governance and Enterprise
//
// Provides:
//   - RBACManager: role-based access control for tools, routes, and operations
//   - AuditLogger: structured audit trail for agent actions
//   - DistributedAgentRegistry: view + control agents across multiple gateways
//
// Design: lightweight and opt-in.  Zero external deps.
// Consumers register policies and audit sinks; the runtime calls checkPolicy()
// before executing tools or routes.

import { appendFileSync, mkdirSync, existsSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { randomUUID } from "node:crypto";

// ─── RBAC ────────────────────────────────────────────────────────────────────

export type RBACAction =
  | "tool:execute"
  | "tool:register"
  | "memory:read"
  | "memory:write"
  | "session:create"
  | "session:delete"
  | "agent:stop"
  | "agent:restart"
  | "agent:create"
  | "skill:install"
  | "skill:uninstall"
  | "workflow:run"
  | "admin:*";

export interface RBACRole {
  id: string;
  name: string;
  description?: string;
  /** Glob-style action patterns granted to this role, e.g. "tool:*" or "memory:read" */
  allow: string[];
  /** Patterns explicitly denied (takes priority over allow) */
  deny?: string[];
}

export interface RBACPolicy {
  /** Subject identifier — agent ID, user ID, or "*" for default */
  subject: string;
  /** Roles assigned to this subject */
  roles: string[];
}

// Built-in roles
export const BUILTIN_ROLES: RBACRole[] = [
  {
    id: "admin",
    name: "Administrator",
    description: "Full unrestricted access",
    allow: ["admin:*", "tool:*", "memory:*", "session:*", "agent:*", "skill:*", "workflow:*"],
  },
  {
    id: "operator",
    name: "Operator",
    description: "Can run tools and workflows but cannot manage agents or skills",
    allow: ["tool:execute", "memory:read", "memory:write", "session:create", "workflow:run"],
    deny: ["agent:stop", "agent:restart", "agent:create", "skill:install", "skill:uninstall"],
  },
  {
    id: "viewer",
    name: "Viewer",
    description: "Read-only access to memory and sessions",
    allow: ["memory:read", "session:create"],
    deny: ["tool:execute", "tool:register", "memory:write", "agent:*", "skill:*", "workflow:*"],
  },
  {
    id: "agent",
    name: "Agent",
    description: "Default role for autonomous agents — can use tools and read/write memory",
    allow: ["tool:execute", "memory:read", "memory:write", "session:create", "workflow:run"],
    deny: ["agent:stop", "agent:create", "skill:install", "skill:uninstall"],
  },
];

function _patternMatch(pattern: string, action: string): boolean {
  if (pattern === "*" || pattern === action) return true;
  if (pattern.endsWith(":*")) {
    const ns = pattern.slice(0, -2);
    return action.startsWith(ns + ":");
  }
  if (pattern === "admin:*") return true;
  return false;
}

export class RBACManager {
  private _roles = new Map<string, RBACRole>();
  private _policies: RBACPolicy[] = [];
  private _enabled = false;

  constructor() {
    for (const r of BUILTIN_ROLES) this._roles.set(r.id, r);
  }

  /** Enable RBAC enforcement.  When disabled, all actions are allowed. */
  enable(): void { this._enabled = true; }
  disable(): void { this._enabled = false; }
  get enabled(): boolean { return this._enabled; }

  registerRole(role: RBACRole): void {
    this._roles.set(role.id, role);
  }

  assignRole(subject: string, roleId: string): void {
    const existing = this._policies.find(p => p.subject === subject);
    if (existing) {
      if (!existing.roles.includes(roleId)) existing.roles.push(roleId);
    } else {
      this._policies.push({ subject, roles: [roleId] });
    }
  }

  revokeRole(subject: string, roleId: string): void {
    const existing = this._policies.find(p => p.subject === subject);
    if (existing) existing.roles = existing.roles.filter(r => r !== roleId);
  }

  getRoles(subject: string): RBACRole[] {
    const policy = this._policies.find(p => p.subject === subject)
      ?? this._policies.find(p => p.subject === "*");
    if (!policy) return [];
    return policy.roles.map(id => this._roles.get(id)).filter(Boolean) as RBACRole[];
  }

  checkPolicy(subject: string, action: RBACAction): { allowed: boolean; reason: string } {
    if (!this._enabled) return { allowed: true, reason: "RBAC disabled" };
    const roles = this.getRoles(subject);
    if (roles.length === 0) return { allowed: false, reason: `No roles assigned to subject "${subject}"` };

    // Deny takes priority
    for (const role of roles) {
      for (const deny of role.deny ?? []) {
        if (_patternMatch(deny, action)) {
          return { allowed: false, reason: `Role "${role.id}" denies action "${action}"` };
        }
      }
    }
    // Check allow
    for (const role of roles) {
      for (const allow of role.allow) {
        if (_patternMatch(allow, action)) {
          return { allowed: true, reason: `Role "${role.id}" allows action "${action}"` };
        }
      }
    }
    return { allowed: false, reason: `No role grants action "${action}" to subject "${subject}"` };
  }

  listPolicies(): RBACPolicy[] { return [...this._policies]; }
  listRoles(): RBACRole[] { return [...this._roles.values()]; }

  toJSON() {
    return { enabled: this._enabled, roles: this.listRoles(), policies: this.listPolicies() };
  }

  saveToFile(path: string): void {
    mkdirSync(join(path, ".."), { recursive: true });
    writeFileSync(path, JSON.stringify(this.toJSON(), null, 2), "utf-8");
  }

  loadFromFile(path: string): void {
    if (!existsSync(path)) return;
    const data = JSON.parse(readFileSync(path, "utf-8"));
    if (data.enabled) this._enabled = true;
    for (const r of data.roles ?? []) this._roles.set(r.id, r);
    this._policies = data.policies ?? [];
  }
}

// ─── Audit Logger ────────────────────────────────────────────────────────────

export interface AuditEntry {
  id: string;
  timestamp: string;
  agentId: string;
  sessionId?: string;
  userId?: string;
  action: string;
  resource?: string;
  outcome: "allowed" | "denied" | "error";
  metadata?: Record<string, unknown>;
  durationMs?: number;
}

export type AuditSink = (entry: AuditEntry) => void | Promise<void>;

export class AuditLogger {
  private _sinks: AuditSink[] = [];
  private _jsonlPath?: string;
  private _enabled = false;

  enable(): void { this._enabled = true; }
  disable(): void { this._enabled = false; }
  get enabled(): boolean { return this._enabled; }

  /** Add an in-memory or remote sink (e.g. push to Nexus Cloud audit API). */
  addSink(sink: AuditSink): void {
    this._sinks.push(sink);
    this._enabled = true;
  }

  /** Write audit entries to a JSONL file. */
  setFile(path: string): void {
    mkdirSync(join(path, ".."), { recursive: true });
    this._jsonlPath = path;
    this._enabled = true;
  }

  async log(entry: Omit<AuditEntry, "id" | "timestamp">): Promise<void> {
    if (!this._enabled) return;
    const full: AuditEntry = {
      id: randomUUID(),
      timestamp: new Date().toISOString(),
      ...entry,
    };
    if (this._jsonlPath) {
      appendFileSync(this._jsonlPath, JSON.stringify(full) + "\n", "utf-8");
    }
    for (const sink of this._sinks) {
      try { await sink(full); } catch { /* sinks must not crash the runtime */ }
    }
  }

  /** Convenience wrapper: log a tool call outcome. */
  async logToolCall(opts: {
    agentId: string;
    sessionId?: string;
    toolId: string;
    outcome: "allowed" | "denied" | "error";
    durationMs?: number;
    metadata?: Record<string, unknown>;
  }): Promise<void> {
    return this.log({
      agentId: opts.agentId,
      sessionId: opts.sessionId,
      action: "tool:execute",
      resource: opts.toolId,
      outcome: opts.outcome,
      durationMs: opts.durationMs,
      metadata: opts.metadata,
    });
  }
}

// ─── Distributed Agent Registry ──────────────────────────────────────────────
// Tracks agents across multiple Nexusclaw gateway instances.
// Each gateway periodically pushes its agent state to a shared store
// (in-memory for single-node, backed by a KV store for multi-node).

export interface RemoteAgentRecord {
  id: string;
  name: string;
  state: string;
  stopped: boolean;
  gatewayUrl: string;
  lastSeenAt: number;
  capabilities?: string[];
  tags?: string[];
}

export class DistributedAgentRegistry {
  private _agents = new Map<string, RemoteAgentRecord>();
  private _ttlMs: number;

  constructor(opts: { ttlMs?: number } = {}) {
    this._ttlMs = opts.ttlMs ?? 120_000; // 2 min default TTL
  }

  /** Register or refresh an agent's presence. */
  upsert(agent: Omit<RemoteAgentRecord, "lastSeenAt">): void {
    this._agents.set(agent.id, { ...agent, lastSeenAt: Date.now() });
  }

  /** Bulk-register agents from a gateway (called on gateway heartbeat). */
  upsertAll(agents: Omit<RemoteAgentRecord, "lastSeenAt">[]): void {
    for (const a of agents) this.upsert(a);
  }

  /** Remove stale agents that haven't been seen within the TTL. */
  evictStale(): number {
    const before = this._agents.size;
    const cutoff = Date.now() - this._ttlMs;
    for (const [id, a] of this._agents) {
      if (a.lastSeenAt < cutoff) this._agents.delete(id);
    }
    return before - this._agents.size;
  }

  /** List all known remote agents (after evicting stale ones). */
  listAll(): RemoteAgentRecord[] {
    this.evictStale();
    return [...this._agents.values()];
  }

  get(agentId: string): RemoteAgentRecord | undefined {
    return this._agents.get(agentId);
  }

  /** Stop a remote agent by calling its gateway's stop API. */
  async stopRemote(agentId: string): Promise<boolean> {
    const record = this._agents.get(agentId);
    if (!record) return false;
    try {
      const res = await fetch(`${record.gatewayUrl}/api/agents/${agentId}/stop`, {
        method: "POST",
        signal: AbortSignal.timeout(5000),
      });
      return res.ok;
    } catch {
      return false;
    }
  }

  /** Send a task to a remote agent via its gateway's chat API. */
  async sendTask(agentId: string, task: string): Promise<string | null> {
    const record = this._agents.get(agentId);
    if (!record) return null;
    try {
      const res = await fetch(`${record.gatewayUrl}/api/chat`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ message: task, agentId }),
        signal: AbortSignal.timeout(30_000),
      });
      if (!res.ok) return null;
      const data: any = await res.json();
      return data.response ?? data.content ?? null;
    } catch {
      return null;
    }
  }

  get size(): number { return this._agents.size; }
}

// ─── Singleton exports ────────────────────────────────────────────────────────
export const rbac = new RBACManager();
export const auditLogger = new AuditLogger();
export const distributedAgents = new DistributedAgentRegistry();
