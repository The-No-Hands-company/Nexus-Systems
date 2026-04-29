// Core type definitions for AnyClaw

export interface AgentConfig {
  id: string;
  name: string;
  model: string;
  fallbackModel?: string;
  thinking?: "off" | "minimal" | "low" | "medium" | "high";
  soul?: string;
  maxTokens?: number;
  temperature?: number;
  /** Per-agent tool policy (Phase 7.1) */
  allowedTools?: string[];
  deniedTools?: string[];
  /** Per-agent workspace sandbox: working directory (Phase 7.2) */
  sandboxDir?: string;
  /** Per-agent filesystem jail: tools cannot access paths outside this root (Phase 8, 7.2) */
  sandboxRoot?: string;
  /** Agent persona — string or structured object (Phase 7.5) */
  persona?: string | PersonaConfig;
  /** Context files to auto-load (Phase 13.7) */
  contextFiles?: string[];
  /** Max tool use iterations per turn */
  maxToolIterations?: number;
}

/** Structured persona definition (Phase 7.5) */
export interface PersonaConfig {
  name?: string;
  role?: string;
  personality?: string[];
  voice?: string;
  constraints?: string[];
  expertise?: string[];
}

export interface Message {
  id: string;
  role: "user" | "assistant" | "system" | "tool";
  content: string;
  timestamp: number;
  metadata?: Record<string, unknown>;
  toolCalls?: ToolCall[];
  toolResults?: ToolResult[];
}

export interface ToolCall {
  id: string;
  name: string;
  arguments: Record<string, unknown>;
}

export interface ToolResult {
  callId: string;
  name: string;
  result: unknown;
  error?: string;
}

export interface Session {
  id: string;
  agentId: string;
  messages: Message[];
  createdAt: number;
  updatedAt: number;
  metadata: Record<string, unknown>;
  channel?: string;
  tags?: string[];
}

export interface Episode {
  id: string;
  sessionId: string;
  content: string;
  summary?: string;
  embedding?: number[];
  validAt: number;
  createdAt: number;
  tags: string[];
}

export interface VaultNote {
  id: string;
  category: "identity" | "expertise" | "preference" | "constraint" | "methodology";
  title: string;
  content: string;
  confidence: number;
  stale: boolean;
  createdAt: number;
  updatedAt: number;
  sourceEpisodes: string[];
}

export interface AlignmentEvent {
  id: string;
  sessionId: string;
  responseSnippet: string;
  score: number;
  verdict: "pass" | "warn" | "fail";
  reason: string;
  timestamp: number;
}

export interface GSDTask {
  id: string;
  specId: string;
  title: string;
  description: string;
  doneCriteria: string[];
  status: "pending" | "in-progress" | "verifying" | "done" | "failed";
  commitHash?: string;
  output?: string;
  createdAt: number;
  completedAt?: number;
  /** IDs of tasks that must be done before this one can start (Phase 2.4) */
  blockedBy?: string[];
}

export interface GSDSpec {
  id: string;
  title: string;
  description: string;
  tasks: GSDTask[];
  status: "draft" | "planned" | "executing" | "done";
  createdAt: number;
}

export interface Tool {
  name: string;
  description: string;
  parameters: ToolParameter[];
  execute: (args: Record<string, unknown>, context: ToolContext) => Promise<unknown>;
}

export interface ToolParameter {
  name: string;
  type: "string" | "number" | "boolean" | "object" | "array";
  description: string;
  required: boolean;
  default?: unknown;
}

export interface ToolContext {
  agentId: string;
  sessionId: string;
  workingDir: string;
  /** Phase 8 (7.2): Optional sandbox root; file/shell tools are confined to this directory */
  sandboxRoot?: string;
  permissions: Permissions;
}

export interface Permissions {
  allowedTools: string[];
  deniedTools: string[];
  allowedPaths: string[];
  blockedCommands: string[];
  confirmationRequired: string[];
  sandboxMode: boolean;
  /** Elevated mode bypasses safety (Phase 9.2) */
  elevated?: boolean;
}

export interface LLMRequest {
  model: string;
  messages: LLMMessage[];
  tools?: LLMToolDef[];
  maxTokens?: number;
  temperature?: number;
  thinking?: string;
  stream?: boolean;
}

export interface LLMMessage {
  role: "user" | "assistant" | "system";
  content: string | LLMContentBlock[];
}

export interface LLMContentBlock {
  type: "text" | "tool_use" | "tool_result" | "thinking";
  text?: string;
  id?: string;
  name?: string;
  input?: Record<string, unknown>;
  toolUseId?: string;
  content?: string;
  /** Budget tokens for extended thinking (Phase 1.7) */
  budgetTokens?: number;
}

export interface LLMToolDef {
  name: string;
  description: string;
  inputSchema: Record<string, unknown>;
}

export interface LLMResponse {
  id: string;
  content: LLMContentBlock[];
  model: string;
  usage: {
    inputTokens: number;
    outputTokens: number;
  };
  stopReason: string;
}

export type AgentPhase = "observe" | "plan" | "act" | "reflect";

export interface AgentState {
  phase: AgentPhase;
  sessionId: string;
  turnCount: number;
  lastAction?: string;
  pendingToolCalls: ToolCall[];
  reflections: string[];
}

/** Skill definition (Phase 5) */
export interface Skill {
  id: string;
  name: string;
  description: string;
  instructions: string;
  tools?: Tool[];
  resources?: string[];
  enabled: boolean;
  source?: string;
  tags?: string[];
  when?: string;
  /** Phase 5.4: If set, only these agent IDs can use this skill. Empty/unset = shared with all agents. */
  agents?: string[];
}

/** MCP server definition (Phase 4) */
export interface MCPServerConfig {
  transport: "stdio" | "sse";
  command?: string;
  args?: string[];
  url?: string;
  env?: Record<string, string>;
  name?: string;
  enabled: boolean;
}

/** Stream event for real-time updates (Phase 1.5) */
export interface StreamEvent {
  type: "text_delta" | "tool_start" | "tool_result" | "thinking" | "done" | "error";
  content?: string;
  toolName?: string;
  toolArgs?: Record<string, unknown>;
  toolResult?: unknown;
  usage?: { inputTokens: number; outputTokens: number };
}

// ─── Team Templates (Phase 2.4) ─────────────────────────────────────────────

/** A single agent role within a team template. */
export interface TeamAgent {
  id: string;
  name?: string;
  role?: string;
  model?: string;
  soul?: string;
  allowedTools?: string[];
  deniedTools?: string[];
  temperature?: number;
  maxTokens?: number;
}

/** A directed edge in the team's agent graph. */
export interface TeamConnection {
  from: string;
  to: string;
  /** Condition that triggers this edge.  Defaults to "done". */
  on?: "done" | "error" | string;
  label?: string;
}

/**
 * A reusable definition of a multi-agent pipeline.
 * Can be loaded from YAML/JSON files or defined inline.
 */
export interface TeamTemplate {
  name: string;
  description?: string;
  version?: string;
  tags?: string[];
  /** The agent id (from `agents`) that receives the initial task. */
  entryAgent?: string;
  agents: TeamAgent[];
  connections: TeamConnection[];
}
