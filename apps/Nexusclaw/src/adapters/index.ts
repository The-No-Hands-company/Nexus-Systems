// Adapter interfaces — Phase 2.5
// Defines the four pluggable adapter contracts for the Nexusclaw ecosystem.
// Third-party "claw" repositories implement these interfaces and register
// them via registerAdapter().  Nexusclaw uses the registered implementations
// instead of built-in defaults when available.
//
// Adapters follow the Nexus Adapter Spec (NAS v1):
//   https://github.com/The-No-Hands-company/Nexus-Systems#nas-spec (future)

import type { LLMProvider } from "../llm/index.js";
import type { Tool, AgentConfig } from "../core/types.js";

// ─── Adapter metadata ───────────────────────────────────────────────────────

export interface AdapterInfo {
  /** Unique adapter identifier, e.g. "claw-hub/skill-registry" */
  id: string;
  /** Human-readable name */
  name: string;
  version: string;
  /** The adapter type tag */
  type: "model-router" | "skill-registry" | "sandbox" | "acp" | "unknown";
  description?: string;
  author?: string;
  homepage?: string;
}

// ─── 1. OpenClaw / Agent Communication Protocol (ACP) ──────────────────────
//
// Defines how Nexusclaw agents communicate with agents running in **other**
// services (e.g. a remote HiClaw swarm, another Nexusclaw gateway, or an
// ACP-compatible agent platform).

export interface ACPMessage {
  id: string;
  from: string;          // sender agent id
  to: string;            // recipient agent id or "*" for broadcast
  type: "task" | "result" | "event" | "ping";
  content: string;
  metadata?: Record<string, unknown>;
  timestamp: number;
}

export interface ACPAdapter {
  readonly info: AdapterInfo;

  /** Connect to the remote transport (HTTP SSE, WebSocket, NATS, etc.) */
  connect(endpoint: string, options?: Record<string, unknown>): Promise<void>;

  /** Send a message to a remote agent. */
  send(message: ACPMessage): Promise<void>;

  /** Subscribe to incoming messages.  Returns an unsubscribe function. */
  subscribe(
    agentId: string,
    handler: (message: ACPMessage) => void | Promise<void>,
  ): () => void;

  /** Discover agents available on this transport. */
  discoverAgents(): Promise<Array<{ id: string; name: string; capabilities: string[] }>>;

  disconnect(): Promise<void>;
}

// ─── 2. Model Router Adapter ────────────────────────────────────────────────
//
// Allows external model routers (e.g. ClawRouter, LiteLLM proxy, OpenRouter)
// to sit in front of Nexusclaw's LLM layer.

export interface ModelRouterAdapter {
  readonly info: AdapterInfo;

  /**
   * Return an LLMProvider that routes requests through this adapter.
   * The returned provider is registered with Nexusclaw's LLMRouter.
   */
  createProvider(config: Record<string, unknown>): LLMProvider;

  /** List available model IDs as seen by this router. */
  listModels(): Promise<string[]>;

  /** Estimate the cost of a request before sending it (optional). */
  estimateCost?(modelId: string, inputTokens: number, outputTokens: number): number | null;
}

// ─── 3. Sandbox / Runtime Adapter ───────────────────────────────────────────
//
// Provides an execution environment for shell/code tools.
// Built-in = local process.  Adapters can provide Docker, Firecracker,
// NemoClaw, Nix, or any other isolated runtime.

export interface SandboxExecOptions {
  cwd?: string;
  env?: Record<string, string>;
  timeoutMs?: number;
  /** Memory limit in MB */
  memoryLimitMb?: number;
  /** Filesystem access mode */
  fsMode?: "none" | "read-only" | "workspace";
}

export interface SandboxExecResult {
  stdout: string;
  stderr: string;
  exitCode: number;
  durationMs: number;
}

export interface SandboxAdapter {
  readonly info: AdapterInfo;

  /** Execute a shell command in the sandbox. */
  exec(command: string, options?: SandboxExecOptions): Promise<SandboxExecResult>;

  /**
   * Execute a block of code in the specified language.
   * The sandbox handles interpreter selection.
   */
  runCode(
    language: "python" | "javascript" | "typescript" | "bash" | "ruby",
    code: string,
    options?: SandboxExecOptions,
  ): Promise<SandboxExecResult>;

  /** Return the capabilities of this sandbox (what it can and can't do). */
  capabilities(): {
    languages: string[];
    networking: boolean;
    fileSystem: boolean;
    gpu: boolean;
    maxTimeoutMs: number;
    maxMemoryMb: number;
  };
}

// ─── 4. Skill Registry Adapter ──────────────────────────────────────────────
//
// Provides access to a remote or local skill catalogue.
// Built-in = ~/.anyclaw/skills/ directory.
// Adapters can provide ClawHub, a private Nexus registry, or git-based sources.

export interface RemoteSkill {
  id: string;
  name: string;
  description: string;
  version: string;
  author: string;
  tags: string[];
  /** URL to the raw SKILL.md content */
  contentUrl: string;
  /** Download count or popularity score */
  stars?: number;
  updatedAt: number;
}

export interface SkillRegistryAdapter {
  readonly info: AdapterInfo;

  /** Search the registry for skills matching the query. */
  search(query: string, options?: { tags?: string[]; limit?: number }): Promise<RemoteSkill[]>;

  /** Fetch the full SKILL.md content for a skill. */
  fetchContent(skillId: string): Promise<string>;

  /** List featured / trending skills. */
  featured(): Promise<RemoteSkill[]>;

  /** Publish a local skill to the registry (optional — requires auth). */
  publish?(skillContent: string, metadata: Omit<RemoteSkill, "contentUrl" | "updatedAt" | "stars">): Promise<string>;
}

// ─── Adapter Registry ────────────────────────────────────────────────────────
//
// Central registry — adapters call registerAdapter() to make themselves
// available.  Nexusclaw queries getAdapter() when it needs one.

type AnyAdapter = ACPAdapter | ModelRouterAdapter | SandboxAdapter | SkillRegistryAdapter;

const _registry = new Map<string, AnyAdapter>();
const _byType = new Map<string, AnyAdapter[]>();

export function registerAdapter(adapter: AnyAdapter): void {
  _registry.set(adapter.info.id, adapter);
  const list = _byType.get(adapter.info.type) ?? [];
  list.push(adapter);
  _byType.set(adapter.info.type, list);
}

export function getAdapter<T extends AnyAdapter>(id: string): T | undefined {
  return _registry.get(id) as T | undefined;
}

export function getAdaptersByType<T extends AnyAdapter>(type: AdapterInfo["type"]): T[] {
  return (_byType.get(type) ?? []) as T[];
}

export function listAdapters(): AdapterInfo[] {
  return [..._registry.values()].map((a) => a.info);
}

export function unregisterAdapter(id: string): boolean {
  const adapter = _registry.get(id);
  if (!adapter) return false;
  _registry.delete(id);
  const list = _byType.get(adapter.info.type) ?? [];
  _byType.set(
    adapter.info.type,
    list.filter((a) => a.info.id !== id),
  );
  return true;
}

// ─── Local sandbox (built-in default) ───────────────────────────────────────
// Registered automatically so agents always have a sandbox available.

import { execSync, spawnSync } from "node:child_process";

export const localSandboxAdapter: SandboxAdapter = {
  info: {
    id: "nexusclaw/local-sandbox",
    name: "Local Sandbox",
    version: "1.0.0",
    type: "sandbox",
    description: "Built-in sandbox using the local OS process environment.",
    author: "Nexus Systems",
  },

  async exec(command, options = {}) {
    const start = Date.now();
    try {
      const result = spawnSync(command, {
        shell: "/bin/sh",
        cwd: options.cwd ?? process.cwd(),
        env: { ...process.env, ...options.env },
        timeout: options.timeoutMs ?? 60000,
        maxBuffer: 10 * 1024 * 1024,
        encoding: "utf-8",
      });
      return {
        stdout: (result.stdout as string) ?? "",
        stderr: (result.stderr as string) ?? "",
        exitCode: result.status ?? 1,
        durationMs: Date.now() - start,
      };
    } catch (err: any) {
      return { stdout: "", stderr: String(err.message), exitCode: 1, durationMs: Date.now() - start };
    }
  },

  async runCode(language, code, options = {}) {
    const interpreters: Record<string, string> = {
      python: "python3",
      javascript: "node",
      typescript: "npx ts-node",
      bash: "bash",
      ruby: "ruby",
    };
    const interp = interpreters[language] || language;
    const tmpFile = `/tmp/nexusclaw-run-${Date.now()}.${language === "typescript" ? "ts" : language.slice(0, 2)}`;
    const { writeFileSync } = await import("node:fs");
    writeFileSync(tmpFile, code, "utf-8");
    return this.exec(`${interp} "${tmpFile}"`, options);
  },

  capabilities() {
    return {
      languages: ["python", "javascript", "typescript", "bash", "ruby"],
      networking: true,
      fileSystem: true,
      gpu: false,
      maxTimeoutMs: 300000,
      maxMemoryMb: 2048,
    };
  },
};

// Register the built-in sandbox automatically
registerAdapter(localSandboxAdapter);

// ─── HttpModelRouterAdapter ──────────────────────────────────────────────────
// ClawRouter-style model routing adapter.
// Points at any OpenAI-compatible proxy (LiteLLM, OpenRouter, ClawRouter, etc.)
// and exposes all downstream models to Nexusclaw's LLM layer.
//
// Usage:
//   registerAdapter(new HttpModelRouterAdapter({ baseUrl: "http://localhost:4000" }));

import type { LLMMessage, LLMOptions, LLMResponse } from "../llm/index.js";

export interface HttpModelRouterConfig {
  baseUrl: string;
  apiKey?: string;
  /** Default timeout in ms (default: 90 000) */
  timeoutMs?: number;
  /** Optional display name for this router */
  name?: string;
}

export class HttpModelRouterAdapter implements ModelRouterAdapter {
  readonly info: AdapterInfo;
  private cfg: Required<HttpModelRouterConfig>;

  constructor(config: HttpModelRouterConfig) {
    this.cfg = {
      baseUrl: config.baseUrl.replace(/\/$/, ""),
      apiKey: config.apiKey ?? "",
      timeoutMs: config.timeoutMs ?? 90_000,
      name: config.name ?? "HTTP Model Router",
    };
    this.info = {
      id: `nexusclaw/http-model-router:${this.cfg.baseUrl}`,
      name: this.cfg.name,
      version: "1.0.0",
      type: "model-router",
      description: `Routes LLM requests to ${this.cfg.baseUrl} (OpenAI-compat proxy)`,
      author: "Nexus Systems",
    };
  }

  createProvider(config: Record<string, unknown>): LLMProvider {
    const routerBase = this.cfg.baseUrl;
    const routerKey = this.cfg.apiKey;
    const timeoutMs = this.cfg.timeoutMs;
    const modelId = String(config.model ?? "gpt-4o-mini");

    return {
      id: `router:${modelId}`,
      name: `${this.cfg.name} (${modelId})`,

      async complete(messages: LLMMessage[], opts: LLMOptions = {}): Promise<LLMResponse> {
        const body = {
          model: modelId,
          messages,
          max_tokens: opts.maxTokens ?? 4096,
          temperature: opts.temperature ?? 0.7,
          stream: false,
        };
        const res = await fetch(`${routerBase}/v1/chat/completions`, {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            ...(routerKey ? { Authorization: `Bearer ${routerKey}` } : {}),
          },
          body: JSON.stringify(body),
          signal: AbortSignal.timeout(timeoutMs),
        });
        if (!res.ok) {
          const txt = await res.text().catch(() => "");
          throw new Error(`Model router error (${res.status}): ${txt.slice(0, 200)}`);
        }
        const data: any = await res.json();
        const choice = data.choices?.[0];
        return {
          content: choice?.message?.content ?? "",
          usage: {
            inputTokens: data.usage?.prompt_tokens ?? 0,
            outputTokens: data.usage?.completion_tokens ?? 0,
          },
          model: data.model ?? modelId,
          stopReason: choice?.finish_reason ?? "stop",
        };
      },

      async *stream(messages: LLMMessage[], opts: LLMOptions = {}) {
        const body = {
          model: modelId,
          messages,
          max_tokens: opts.maxTokens ?? 4096,
          temperature: opts.temperature ?? 0.7,
          stream: true,
        };
        const res = await fetch(`${routerBase}/v1/chat/completions`, {
          method: "POST",
          headers: {
            "Content-Type": "application/json",
            ...(routerKey ? { Authorization: `Bearer ${routerKey}` } : {}),
          },
          body: JSON.stringify(body),
        });
        if (!res.ok || !res.body) throw new Error(`Router stream error (${res.status})`);
        const reader = res.body.getReader();
        const decoder = new TextDecoder();
        let buf = "";
        while (true) {
          const { done, value } = await reader.read();
          if (done) break;
          buf += decoder.decode(value, { stream: true });
          const lines = buf.split("\n");
          buf = lines.pop() ?? "";
          for (const line of lines) {
            if (!line.startsWith("data: ")) continue;
            const payload = line.slice(6).trim();
            if (payload === "[DONE]") return;
            try {
              const chunk: any = JSON.parse(payload);
              const delta = chunk.choices?.[0]?.delta?.content;
              if (delta) yield { type: "text" as const, text: delta };
            } catch { /* skip malformed */ }
          }
        }
      },
    };
  }

  async listModels(): Promise<string[]> {
    try {
      const res = await fetch(`${this.cfg.baseUrl}/v1/models`, {
        headers: this.cfg.apiKey ? { Authorization: `Bearer ${this.cfg.apiKey}` } : {},
        signal: AbortSignal.timeout(10_000),
      });
      if (!res.ok) return [];
      const data: any = await res.json();
      return (data.data ?? []).map((m: any) => String(m.id)).sort();
    } catch {
      return [];
    }
  }

  estimateCost(modelId: string, inputTokens: number, outputTokens: number): number | null {
    // Basic cost table (USD per 1M tokens) — overridable by subclassing
    const table: Record<string, [number, number]> = {
      "gpt-4o": [5, 15],
      "gpt-4o-mini": [0.15, 0.6],
      "gpt-4-turbo": [10, 30],
      "gpt-3.5-turbo": [0.5, 1.5],
      "claude-3-5-sonnet": [3, 15],
      "claude-3-haiku": [0.25, 1.25],
    };
    const key = Object.keys(table).find(k => modelId.includes(k));
    if (!key) return null;
    const [inRate, outRate] = table[key];
    return (inputTokens / 1_000_000) * inRate + (outputTokens / 1_000_000) * outRate;
  }
}

// ─── DockerSandboxAdapter ────────────────────────────────────────────────────
// NemoClaw-style Docker container runtime adapter.
// Executes commands inside a Docker container for isolation.
// Falls back gracefully if Docker is not available.
//
// Usage:
//   const sandbox = new DockerSandboxAdapter({ image: "python:3.12-slim" });
//   registerAdapter(sandbox);

import { spawnSync as _spawnSync } from "node:child_process";

export interface DockerSandboxConfig {
  /** Docker image to use (default: "node:22-slim") */
  image?: string;
  /** Working directory inside the container (default: /workspace) */
  workdir?: string;
  /** Extra docker run flags */
  extraFlags?: string[];
  /** Memory limit, e.g. "512m" */
  memory?: string;
  /** CPU limit, e.g. "1.0" */
  cpus?: string;
}

export class DockerSandboxAdapter implements SandboxAdapter {
  readonly info: AdapterInfo;
  private cfg: Required<DockerSandboxConfig>;

  constructor(config: DockerSandboxConfig = {}) {
    this.cfg = {
      image: config.image ?? "node:22-slim",
      workdir: config.workdir ?? "/workspace",
      extraFlags: config.extraFlags ?? [],
      memory: config.memory ?? "512m",
      cpus: config.cpus ?? "1.0",
    };
    this.info = {
      id: `nexusclaw/docker-sandbox:${this.cfg.image}`,
      name: `Docker Sandbox (${this.cfg.image})`,
      version: "1.0.0",
      type: "sandbox",
      description: `Isolated execution inside Docker image: ${this.cfg.image}`,
      author: "Nexus Systems",
    };
  }

  private _dockerAvailable(): boolean {
    try {
      const r = _spawnSync("docker", ["info"], { encoding: "utf-8", timeout: 5000 });
      return r.status === 0;
    } catch {
      return false;
    }
  }

  async exec(command: string, options: SandboxExecOptions = {}): Promise<SandboxExecResult> {
    if (!this._dockerAvailable()) {
      return localSandboxAdapter.exec(command, options);
    }
    const start = Date.now();
    const dockerArgs = [
      "run", "--rm",
      "--memory", this.cfg.memory,
      "--cpus", this.cfg.cpus,
      "--network", options.fsMode === "none" ? "none" : "bridge",
      "-w", options.cwd ?? this.cfg.workdir,
      ...this.cfg.extraFlags,
      ...(options.env ? Object.entries(options.env).flatMap(([k, v]) => ["-e", `${k}=${v}`]) : []),
      this.cfg.image,
      "sh", "-c", command,
    ];
    const result = _spawnSync("docker", dockerArgs, {
      encoding: "utf-8",
      timeout: options.timeoutMs ?? 60_000,
      maxBuffer: 10 * 1024 * 1024,
    });
    return {
      stdout: (result.stdout as string) ?? "",
      stderr: (result.stderr as string) ?? "",
      exitCode: result.status ?? 1,
      durationMs: Date.now() - start,
    };
  }

  async runCode(
    language: "python" | "javascript" | "typescript" | "bash" | "ruby",
    code: string,
    options: SandboxExecOptions = {},
  ): Promise<SandboxExecResult> {
    const interp: Record<string, string> = {
      python: "python3",
      javascript: "node",
      typescript: "npx ts-node",
      bash: "bash",
      ruby: "ruby",
    };
    const escaped = code.replace(/'/g, "'\\''");
    return this.exec(`${interp[language] ?? language} -c '${escaped}'`, options);
  }

  capabilities() {
    const available = this._dockerAvailable();
    return {
      languages: ["python", "javascript", "typescript", "bash", "ruby"],
      networking: true,
      fileSystem: true,
      gpu: false,
      maxTimeoutMs: 300_000,
      maxMemoryMb: parseInt(this.cfg.memory) || 512,
      _dockerAvailable: available,
    };
  }
}
