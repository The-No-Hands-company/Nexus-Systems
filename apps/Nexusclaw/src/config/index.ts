// Configuration system for AnyClaw
// Phase 4: MCP server config
// Phase 5: Skills config
// Phase 9.2: Elevated mode
// Phase 13.7: Context file (.context.md) support
// Phase 14.1: Lightweight mode
// Phase 14.3: Zero-config quickstart

import { z } from "zod";
import { readFileSync, existsSync, mkdirSync, readdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { homedir } from "node:os";
import yaml from "js-yaml";

// ─── Zod Schemas ──────────────────────────────────────────────────────────

const PermissionsSchema = z.object({
  allowed_paths: z.array(z.string()).default(["~/projects/", "/tmp/anyclaw/"]),
  blocked_commands: z.array(z.string()).default(["rm -rf /", "dd if=", "mkfs"]),
  confirmation_required: z
    .array(z.string())
    .default(["git push", "docker rm", "docker rmi"]),
  elevated: z.boolean().default(false),
  ssrf: z.object({
    blocked_hosts: z.array(z.string()).default([
      "169.254.169.254", "metadata.google.internal", "100.100.100.200",
    ]),
    allow_private: z.boolean().default(false),
    allowed_hosts: z.array(z.string()).default([]),
    blocked_url_patterns: z.array(z.string()).default([]),
  }).default({}),
  sandbox_profile: z.enum(["strict", "standard", "permissive"]).default("standard"),
});

const MemorySchema = z.object({
  episodic_db: z.string().default("~/.anyclaw/data/memory.db"),
  vault_dir: z.string().default("~/.anyclaw/data/vault/"),
  alignment: z.boolean().default(true),
  auto_ingest: z.boolean().default(true),
  max_episodes: z.number().default(10000),
  compact_after: z.number().default(50),
});

const GSDSchema = z.object({
  auto_commit: z.boolean().default(true),
  verify_before_done: z.boolean().default(true),
  max_task_context_tokens: z.number().default(50000),
  fresh_context_per_task: z.boolean().default(true),
  db: z.string().default("~/.anyclaw/data/gsd.db"),
});

const GatewaySchema = z.object({
  port: z.number().default(18800),
  bind: z.string().default("127.0.0.1"),
});

const BuiltinToolsSchema = z.object({
  shell: z.boolean().default(true),
  filesystem: z.boolean().default(true),
  git: z.boolean().default(true),
  browser: z.boolean().default(false),
  http: z.boolean().default(true),
  web_search: z.boolean().default(false),
  web_fetch: z.boolean().default(true),
  apply_patch: z.boolean().default(true),
  process: z.boolean().default(true),
  image: z.boolean().default(true),
  pdf: z.boolean().default(true),
});

const RateLimitSchema = z.object({
  requests_per_minute: z.number().default(60),
  tokens_per_minute: z.number().default(100000),
  max_retries: z.number().default(3),
  backoff_ms: z.number().default(1000),
});

const MCPServerSchema = z.object({
  transport: z.enum(["stdio", "sse"]).default("stdio"),
  command: z.string().optional(),
  args: z.array(z.string()).optional(),
  url: z.string().optional(),
  env: z.record(z.string(), z.string()).optional(),
  name: z.string().optional(),
  enabled: z.boolean().default(true),
});

const ToolsSchema = z.object({
  builtin: BuiltinToolsSchema.default({}),
  mcp_servers: z.record(z.string(), MCPServerSchema).default({}),
});

const SkillSchema = z.object({
  path: z.string(),
  enabled: z.boolean().default(true),
  agents: z.array(z.string()).default([]),
});

const PersonaSchema = z.object({
  name: z.string().optional(),
  role: z.string().optional(),
  personality: z.array(z.string()).default([]),
  voice: z.string().optional(),
  constraints: z.array(z.string()).default([]),
  expertise: z.array(z.string()).default([]),
}).optional();

const AgentSchema = z.object({
  model: z.string().default("anthropic/claude-sonnet-4-6"),
  fallback_model: z.string().optional(),
  thinking: z
    .enum(["off", "minimal", "low", "medium", "high"])
    .default("medium"),
  max_tokens: z.number().default(8192),
  temperature: z.number().default(0.7),
  max_tool_iterations: z.number().default(20),
  persona: z.union([z.string(), PersonaSchema]).optional(),
  soul: z.string().optional(),
  context_files: z.array(z.string()).default([]),
  allowed_tools: z.array(z.string()).default([]),
  denied_tools: z.array(z.string()).default([]),
});

const LightweightSchema = z.object({
  enabled: z.boolean().default(false),
  disable_memory: z.boolean().default(true),
  disable_gsd: z.boolean().default(true),
  disable_gateway: z.boolean().default(true),
});

const PluginSchema = z.object({
  dirs: z.array(z.string()).default([]),
  packages: z.array(z.string()).default([]),
  auto_discover: z.boolean().default(true),
});

const CloudSchema = z.object({
  enabled: z.boolean().default(false),
  endpoint: z.string().url().optional(),
  api_key: z.string().optional(),
  service_id: z.string().default("anyclaw"),
  name: z.string().default("AnyClaw"),
  description: z.string().default("Nexusclaw agent runtime"),
  version: z.string().default("0.1.0"),
  capabilities: z.array(z.string()).default([
    "agent-runtime",
    "tool-registry",
    "memory",
    "gateway",
    "health",
  ]),
  auto_register: z.boolean().default(true),
  heartbeat_interval_seconds: z.number().default(60),
});

/**
 * Nexus AI engine integration.
 * When `url` is set (or NEXUS_AI_URL env var is present), Nexusclaw routes
 * LLM calls through Nexus AI instead of directly to upstream providers.
 *
 * Set `model` to "nexus-ai/auto" to let Nexus AI choose the best model,
 * or "nexus-ai/anthropic", "nexus-ai/ollama", etc. to pin a provider.
 */
const NexusAISchema = z.object({
  url: z.string().url().optional(),
  api_key: z.string().optional(),
  default_model: z.string().default("nexus-ai/auto"),
  enabled: z.boolean().default(true),
});

export const AnyClawConfigSchema = z.object({
  agent: AgentSchema.default({}),
  memory: MemorySchema.default({}),
  safety: PermissionsSchema.default({}),
  gsd: GSDSchema.default({}),
  gateway: GatewaySchema.default({}),
  tools: ToolsSchema.default({}),
  skills: z.record(z.string(), SkillSchema).default({}),
  lightweight: LightweightSchema.default({}),
  plugins: PluginSchema.default({}),
  cloud: CloudSchema.default({}),
  nexus_ai: NexusAISchema.default({}),
  rate_limit: RateLimitSchema.default({}),
});

export type AnyClawConfig = z.infer<typeof AnyClawConfigSchema>;

// ─── Helpers ──────────────────────────────────────────────────────────────

function expandHome(filepath: string): string {
  if (filepath.startsWith("~/")) {
    return join(homedir(), filepath.slice(2));
  }
  return filepath;
}

export function getDataDir(): string {
  return expandHome(process.env.ANYCLAW_DATA_DIR || "~/.anyclaw");
}

// ─── Config Loading ───────────────────────────────────────────────────────

export function loadConfig(configPath?: string): AnyClawConfig {
  const paths = configPath
    ? [configPath]
    : [
        join(process.cwd(), "anyclaw.yaml"),
        join(process.cwd(), "anyclaw.yml"),
        join(getDataDir(), "anyclaw.yaml"),
      ];

  let config: AnyClawConfig;

  let found = false;
  for (const p of paths) {
    const resolved = expandHome(p);
    if (existsSync(resolved)) {
      const raw = readFileSync(resolved, "utf-8");
      const parsed = yaml.load(raw) as Record<string, unknown>;
      config = AnyClawConfigSchema.parse(parsed || {});
      found = true;
      break;
    }
  }

  if (!found) {
    config = AnyClawConfigSchema.parse({});
  }

  // Expand ~ in paths
  config!.memory.episodic_db = expandHome(config!.memory.episodic_db);
  config!.memory.vault_dir = expandHome(config!.memory.vault_dir);
  config!.gsd.db = expandHome(config!.gsd.db);
  config!.safety.allowed_paths = config!.safety.allowed_paths.map(expandHome);

  return config!;
}

// ─── Directory Setup ──────────────────────────────────────────────────────

export function ensureDataDirs(config: AnyClawConfig): void {
  const dataDir = getDataDir();
  const dirs = [
    dataDir,
    join(dataDir, "data"),
    config.memory.vault_dir,
    join(dataDir, "sessions"),
    join(dataDir, "logs"),
    join(dataDir, "workspace"),
    join(dataDir, "workspace", "skills"),
  ];

  for (const dir of dirs) {
    if (!existsSync(dir)) {
      mkdirSync(dir, { recursive: true });
    }
  }
}

// ─── Context Files (Phase 13.7: .context.md) ─────────────────────────────

export function loadContextFiles(paths: string[], cwd: string): string {
  const contextParts: string[] = [];

  // Auto-discover .context.md in cwd
  const autoFiles = [".context.md", "CONTEXT.md", ".anyclaw/context.md"];
  for (const f of autoFiles) {
    const fp = join(cwd, f);
    if (existsSync(fp)) {
      contextParts.push(readFileSync(fp, "utf-8"));
    }
  }

  // Load explicitly configured context files
  for (const p of paths) {
    const resolved = resolve(cwd, expandHome(p));
    if (existsSync(resolved)) {
      contextParts.push(readFileSync(resolved, "utf-8"));
    }
  }

  return contextParts.join("\n\n---\n\n");
}

// ─── Zero-config Default YAML (Phase 14.3) ──────────────────────────────

export function generateDefaultConfig(): string {
  return `# AnyClaw Configuration (auto-generated)
agent:
  model: anthropic/claude-sonnet-4-20250514
  max_tokens: 16384
  temperature: 0.7
  thinking: medium

memory:
  episodic_db: ~/.anyclaw/data/memory.db
  vault_dir: ~/.anyclaw/data/vault

safety:
  allowed_paths: ["."]
  blocked_commands: ["rm -rf /", "dd if=", "mkfs"]
  confirmation_required: ["git push --force", "git reset --hard"]

gsd:
  auto_commit: true
  verify_before_done: true
  db: ~/.anyclaw/data/gsd.db

gateway:
  port: 18800
  bind: 127.0.0.1

tools:
  builtin:
    shell: true
    filesystem: true
    git: true
    http: true
    web_fetch: true
    apply_patch: true
`;
}

export function ensureDefaultConfig(): void {
  const configPath = join(getDataDir(), "anyclaw.yaml");
  if (!existsSync(configPath)) {
    const dir = getDataDir();
    if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
    writeFileSync(configPath, generateDefaultConfig(), "utf-8");
  }
}
