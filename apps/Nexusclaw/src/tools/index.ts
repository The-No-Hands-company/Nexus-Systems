// Tool registry and built-in tools for AnyClaw
// Phase 3.1-3.13: Expanded tool set
// Phase 8.1-8.6: Session management tools
// Phase 6.16-6.17: File change tracking for /undo and /diff

import { execSync, spawn, type ChildProcess } from "node:child_process";
import { randomUUID } from "node:crypto";
import {
  readFileSync,
  writeFileSync,
  existsSync,
  readdirSync,
  statSync,
  mkdirSync,
  unlinkSync,
} from "node:fs";
import { join, resolve, relative, isAbsolute, dirname } from "node:path";
import type { Tool, ToolParameter, ToolContext, LLMToolDef } from "../core/types.js";
import { webSearchTool } from "./web-search.js";
import { browserTools } from "./browser.js";
import { createImageTool } from "./image.js";
import { pdfReadTool } from "./pdf.js";
import type { LLMRouter } from "../llm/index.js";

// ─── File Change Tracker (Phase 6.16/6.17) ─────────────────────────────────

export interface FileChange {
  path: string;
  type: "create" | "modify";
  previousContent: string | null; // null = file didn't exist before
  timestamp: number;
}

const fileChanges: FileChange[] = [];

function trackFileWrite(filepath: string): void {
  const prev = existsSync(filepath) ? readFileSync(filepath, "utf-8") : null;
  fileChanges.push({
    path: filepath,
    type: prev === null ? "create" : "modify",
    previousContent: prev,
    timestamp: Date.now(),
  });
}

export function getFileChanges(): FileChange[] {
  return [...fileChanges];
}

export function undoLastChange(): { path: string; action: string } | null {
  const change = fileChanges.pop();
  if (!change) return null;
  if (change.previousContent === null) {
    // File was created — delete it
    if (existsSync(change.path)) unlinkSync(change.path);
    return { path: change.path, action: "deleted (was newly created)" };
  } else {
    // File was modified — restore previous content
    writeFileSync(change.path, change.previousContent, "utf-8");
    return { path: change.path, action: "restored to previous version" };
  }
}

export function clearFileChanges(): void {
  fileChanges.length = 0;
}

// ─── SSRF Policy (Phase 9.5) ───────────────────────────────────────────────

export interface SSRFPolicy {
  blockedHosts: string[];
  allowPrivate: boolean;
  allowedHosts: string[];
  blockedUrlPatterns: string[];
}

const DEFAULT_SSRF: SSRFPolicy = {
  blockedHosts: ["169.254.169.254", "metadata.google.internal", "100.100.100.200"],
  allowPrivate: false,
  allowedHosts: [],
  blockedUrlPatterns: [],
};

let ssrfPolicy: SSRFPolicy = { ...DEFAULT_SSRF };

export function setSSRFPolicy(policy: Partial<SSRFPolicy>): void {
  ssrfPolicy = { ...DEFAULT_SSRF, ...policy };
}

function validateSSRF(url: string): void {
  const parsed = new URL(url);
  const hostname = parsed.hostname;

  // Allow-listed hosts bypass all checks
  if (ssrfPolicy.allowedHosts.length > 0 && ssrfPolicy.allowedHosts.includes(hostname)) return;

  // Blocked hosts (metadata services, etc.)
  if (ssrfPolicy.blockedHosts.includes(hostname)) {
    throw new Error(`Request to "${hostname}" is blocked (SSRF protection)`);
  }

  // URL pattern blocklist
  for (const pattern of ssrfPolicy.blockedUrlPatterns) {
    if (url.includes(pattern)) {
      throw new Error(`Request URL matches blocked pattern "${pattern}" (SSRF protection)`);
    }
  }

  // Private network ranges
  if (!ssrfPolicy.allowPrivate && hostname !== "localhost" && hostname !== "127.0.0.1") {
    if (/^(10\.|172\.(1[6-9]|2\d|3[01])\.|192\.168\.)/.test(hostname)) {
      throw new Error("Request to private network ranges is blocked (SSRF protection)");
    }
  }
}

// ─── Tool Registry ─────────────────────────────────────────────────────────

export class ToolRegistry {
  private tools = new Map<string, Tool>();

  register(tool: Tool): void {
    this.tools.set(tool.name, tool);
  }

  unregister(name: string): boolean {
    return this.tools.delete(name);
  }

  get(name: string): Tool | undefined {
    return this.tools.get(name);
  }

  list(): Tool[] {
    return [...this.tools.values()];
  }

  names(): string[] {
    return [...this.tools.keys()];
  }

  toLLMTools(): LLMToolDef[] {
    return this.list().map((t) => ({
      name: t.name,
      description: t.description,
      inputSchema: {
        type: "object",
        properties: Object.fromEntries(
          t.parameters.map((p) => [
            p.name,
            {
              type: p.type,
              description: p.description,
              ...(p.default !== undefined && { default: p.default }),
            },
          ]),
        ),
        required: t.parameters.filter((p) => p.required).map((p) => p.name),
      },
    }));
  }

  async execute(
    toolName: string,
    args: Record<string, unknown>,
    context: ToolContext,
  ): Promise<unknown> {
    const tool = this.tools.get(toolName);
    if (!tool) throw new Error(`Tool not found: ${toolName}`);

    if (
      context.permissions.deniedTools.length > 0 &&
      context.permissions.deniedTools.includes(toolName)
    ) {
      throw new Error(`Tool "${toolName}" is denied by permissions`);
    }
    if (
      context.permissions.allowedTools.length > 0 &&
      !context.permissions.allowedTools.includes(toolName)
    ) {
      throw new Error(`Tool "${toolName}" is not in the allowed list`);
    }

    return tool.execute(args, context);
  }
}

// ─── Path Safety ──────────────────────────────────────────────────────────

function isPathAllowed(filepath: string, allowedPaths: string[]): boolean {
  if (allowedPaths.length === 0) return true;
  const resolved = resolve(filepath);
  return allowedPaths.some((allowed) => resolved.startsWith(resolve(allowed)));
}

function assertPathAllowed(filepath: string, context: ToolContext): void {
  if (context.permissions.elevated) return;
  if (!isPathAllowed(filepath, context.permissions.allowedPaths)) {
    throw new Error(
      `Path "${filepath}" is outside allowed directories: ${context.permissions.allowedPaths.join(", ")}`,
    );
  }
}

function isCommandBlocked(command: string, blockedCommands: string[]): boolean {
  return blockedCommands.some((b) => command.toLowerCase().includes(b.toLowerCase()));
}

// ─── Sandbox enforcement (Phase 8, 7.2) ────────────────────────────────────
/**
 * If context.sandboxRoot is set, resolve the given path and throw if it escapes
 * the sandbox directory. Returns the resolved path.
 */
function sandboxPath(p: string, context: import("../core/types.js").ToolContext): string {
  if (!context.sandboxRoot) return p;
  const resolved = resolve(p);
  const root = resolve(context.sandboxRoot);
  if (!resolved.startsWith(root + "/") && resolved !== root) {
    throw new Error(`Sandbox violation: path "${resolved}" is outside sandbox root "${root}"`);
  }
  return resolved;
}

/**
 * Resolve a working directory against the sandbox root. If sandboxRoot is set and
 * the given cwd would escape it, clamp it to the sandbox root instead of throwing.
 */
function sandboxCwd(cwd: string, context: import("../core/types.js").ToolContext): string {
  if (!context.sandboxRoot) return cwd;
  const root = resolve(context.sandboxRoot);
  const resolved = resolve(cwd);
  if (!resolved.startsWith(root + "/") && resolved !== root) return root;
  return resolved;
}

// ─── Core Tools ────────────────────────────────────────────────────────────

export const shellTool: Tool = {
  name: "shell",
  description:
    "Execute a shell command in the working directory. Use for running scripts, builds, tests, git commands, etc.",
  parameters: [
    { name: "command", type: "string", description: "The shell command to execute", required: true },
    { name: "cwd", type: "string", description: "Working directory (defaults to agent's working dir)", required: false },
    { name: "timeout", type: "number", description: "Timeout in milliseconds (default: 30000)", required: false, default: 30000 },
  ],
  async execute(args, context) {
    const command = args.command as string;
    const cwd = sandboxCwd((args.cwd as string) || context.workingDir, context);
    const timeout = (args.timeout as number) || 30000;

    if (!context.permissions.elevated && isCommandBlocked(command, context.permissions.blockedCommands)) {
      throw new Error(`Command blocked by safety policy: ${command}`);
    }

    try {
      const output = execSync(command, {
        cwd,
        timeout,
        maxBuffer: 1024 * 1024,
        encoding: "utf-8",
        env: { ...process.env, LANG: "en_US.UTF-8" },
      });
      return { stdout: output, exitCode: 0 };
    } catch (err: any) {
      return { stdout: err.stdout || "", stderr: err.stderr || err.message, exitCode: err.status || 1 };
    }
  },
};

export const readFileTool: Tool = {
  name: "read_file",
  description: "Read the contents of a file",
  parameters: [
    { name: "path", type: "string", description: "Path to the file", required: true },
    { name: "startLine", type: "number", description: "Start line (1-based, optional)", required: false },
    { name: "endLine", type: "number", description: "End line (1-based, optional)", required: false },
  ],
  async execute(args, context) {
    let filepath = isAbsolute(args.path as string)
      ? (args.path as string)
      : join(context.workingDir, args.path as string);
    filepath = sandboxPath(filepath, context);
    assertPathAllowed(filepath, context);
    if (!existsSync(filepath)) throw new Error(`File not found: ${filepath}`);

    const content = readFileSync(filepath, "utf-8");
    if (args.startLine || args.endLine) {
      const lines = content.split("\n");
      const start = ((args.startLine as number) || 1) - 1;
      const end = (args.endLine as number) || lines.length;
      return lines.slice(start, end).join("\n");
    }
    return content;
  },
};

export const writeFileTool: Tool = {
  name: "write_file",
  description: "Write content to a file. Creates parent directories if needed.",
  parameters: [
    { name: "path", type: "string", description: "Path to the file", required: true },
    { name: "content", type: "string", description: "Content to write", required: true },
  ],
  async execute(args, context) {
    let filepath = isAbsolute(args.path as string)
      ? (args.path as string)
      : join(context.workingDir, args.path as string);
    filepath = sandboxPath(filepath, context);
    assertPathAllowed(filepath, context);
    const dir = dirname(filepath);
    if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
    trackFileWrite(filepath);
    writeFileSync(filepath, args.content as string, "utf-8");
    return { written: filepath };
  },
};

export const listDirTool: Tool = {
  name: "list_dir",
  description: "List contents of a directory",
  parameters: [
    { name: "path", type: "string", description: "Path to directory", required: true },
  ],
  async execute(args, context) {
    let dirpath = isAbsolute(args.path as string)
      ? (args.path as string)
      : join(context.workingDir, args.path as string);
    dirpath = sandboxPath(dirpath, context);
    assertPathAllowed(dirpath, context);
    if (!existsSync(dirpath)) throw new Error(`Directory not found: ${dirpath}`);

    return readdirSync(dirpath).map((name) => {
      const full = join(dirpath, name);
      const stat = statSync(full);
      return { name, type: stat.isDirectory() ? "directory" : "file", size: stat.size, modified: stat.mtimeMs };
    });
  },
};

export const searchFilesTool: Tool = {
  name: "search_files",
  description: "Search for text patterns in files using grep",
  parameters: [
    { name: "pattern", type: "string", description: "Search pattern (regex supported)", required: true },
    { name: "path", type: "string", description: "Directory to search in", required: false },
    { name: "include", type: "string", description: "File glob pattern (e.g., '*.ts')", required: false },
  ],
  async execute(args, context) {
    let searchPath = (args.path as string) || context.workingDir;
    if (!isAbsolute(searchPath)) searchPath = join(context.workingDir, searchPath);
    searchPath = sandboxPath(searchPath, context);
    const include = args.include ? `--include=${JSON.stringify(args.include)}` : "";
    const command = `grep -rn ${include} -- ${JSON.stringify(args.pattern as string)} ${JSON.stringify(searchPath)} 2>/dev/null | head -50`;
    try {
      return execSync(command, { encoding: "utf-8", maxBuffer: 512 * 1024, timeout: 10000 }).trim();
    } catch {
      return "No matches found.";
    }
  },
};

export const gitTool: Tool = {
  name: "git",
  description: "Execute git commands",
  parameters: [
    { name: "subcommand", type: "string", description: "Git subcommand and arguments", required: true },
  ],
  async execute(args, context) {
    const subcommand = args.subcommand as string;
    const dangerous = ["push", "reset --hard", "force"];
    if (!context.permissions.elevated && dangerous.some((d) => subcommand.toLowerCase().includes(d))) {
      throw new Error(`Dangerous git operation "${subcommand}" requires confirmation or elevated mode.`);
    }
    try {
      const output = execSync(`git ${subcommand}`, {
        cwd: context.workingDir,
        encoding: "utf-8",
        maxBuffer: 1024 * 1024,
        timeout: 30000,
      });
      return { output: output.trim(), exitCode: 0 };
    } catch (err: any) {
      return { output: err.stdout || "", error: err.stderr || err.message, exitCode: err.status || 1 };
    }
  },
};

export const httpTool: Tool = {
  name: "http_request",
  description: "Make HTTP requests (GET, POST, etc.)",
  parameters: [
    { name: "url", type: "string", description: "URL to request", required: true },
    { name: "method", type: "string", description: "HTTP method", required: false, default: "GET" },
    { name: "headers", type: "object", description: "Request headers", required: false },
    { name: "body", type: "string", description: "Request body", required: false },
  ],
  async execute(args) {
    const url = args.url as string;
    validateSSRF(url);
    const response = await fetch(url, {
      method: (args.method as string) || "GET",
      headers: (args.headers as Record<string, string>) || {},
      body: args.body as string | undefined,
    });
    const text = await response.text();
    return {
      status: response.status,
      headers: Object.fromEntries(response.headers.entries()),
      body: text.slice(0, 50000),
    };
  },
};

// ─── Phase 3.3: web_fetch ─────────────────────────────────────────────────

export const webFetchTool: Tool = {
  name: "web_fetch",
  description: "Fetch a web page and extract its text content (strips HTML tags). Useful for reading documentation, articles, etc.",
  parameters: [
    { name: "url", type: "string", description: "URL to fetch", required: true },
    { name: "max_length", type: "number", description: "Max characters to return (default: 50000)", required: false, default: 50000 },
  ],
  async execute(args) {
    const url = args.url as string;
    const maxLen = (args.max_length as number) || 50000;
    validateSSRF(url);

    const response = await fetch(url, {
      headers: { "User-Agent": "AnyClaw/0.1 (web_fetch)" },
    });
    const html = await response.text();

    // Strip HTML to text
    const text = html
      .replace(/<script[\s\S]*?<\/script>/gi, "")
      .replace(/<style[\s\S]*?<\/style>/gi, "")
      .replace(/<[^>]+>/g, " ")
      .replace(/\s+/g, " ")
      .replace(/&nbsp;/g, " ")
      .replace(/&amp;/g, "&")
      .replace(/&lt;/g, "<")
      .replace(/&gt;/g, ">")
      .replace(/&quot;/g, '"')
      .trim();

    return {
      url,
      status: response.status,
      title: html.match(/<title[^>]*>([\s\S]*?)<\/title>/i)?.[1]?.trim() || "",
      content: text.slice(0, maxLen),
      length: text.length,
      truncated: text.length > maxLen,
    };
  },
};

// ─── Phase 3.4: apply_patch ───────────────────────────────────────────────

export const applyPatchTool: Tool = {
  name: "apply_patch",
  description:
    "Apply a text patch to a file. Specify old text to find and new text to replace it with. Safer than writing the entire file.",
  parameters: [
    { name: "path", type: "string", description: "Path to the file", required: true },
    { name: "old_text", type: "string", description: "Exact text to find and replace", required: true },
    { name: "new_text", type: "string", description: "Text to replace old_text with", required: true },
  ],
  async execute(args, context) {
    const filepath = isAbsolute(args.path as string)
      ? (args.path as string)
      : join(context.workingDir, args.path as string);
    assertPathAllowed(filepath, context);
    if (!existsSync(filepath)) throw new Error(`File not found: ${filepath}`);

    const content = readFileSync(filepath, "utf-8");
    const oldText = args.old_text as string;
    const newText = args.new_text as string;

    const idx = content.indexOf(oldText);
    if (idx === -1) throw new Error("old_text not found in file");

    // Ensure unique match
    const secondIdx = content.indexOf(oldText, idx + 1);
    if (secondIdx !== -1) {
      throw new Error("old_text matches multiple locations — provide more context to make it unique");
    }

    const patched = content.slice(0, idx) + newText + content.slice(idx + oldText.length);
    trackFileWrite(filepath);
    writeFileSync(filepath, patched, "utf-8");
    return { patched: filepath, replacements: 1 };
  },
};

// ─── Phase 3.5: process management ──────────────────────────────────────

const managedProcesses = new Map<string, ChildProcess>();

export const processSpawnTool: Tool = {
  name: "process_spawn",
  description: "Spawn a long-running background process (e.g., dev server). Returns a process ID to manage it.",
  parameters: [
    { name: "command", type: "string", description: "Command to run", required: true },
    { name: "cwd", type: "string", description: "Working directory", required: false },
    { name: "label", type: "string", description: "Human-readable label", required: false },
  ],
  async execute(args, context) {
    const command = args.command as string;
    const cwd = (args.cwd as string) || context.workingDir;
    const label = (args.label as string) || command.slice(0, 50);

    if (!context.permissions.elevated && isCommandBlocked(command, context.permissions.blockedCommands)) {
      throw new Error(`Command blocked: ${command}`);
    }

    const child = spawn("sh", ["-c", command], {
      cwd,
      detached: true,
      stdio: ["ignore", "pipe", "pipe"],
      env: { ...process.env, LANG: "en_US.UTF-8" },
    });

    const id = `proc-${child.pid}`;
    managedProcesses.set(id, child);

    let stdout = "";
    let stderr = "";
    child.stdout?.on("data", (d) => { stdout += d.toString(); stdout = stdout.slice(-10000); });
    child.stderr?.on("data", (d) => { stderr += d.toString(); stderr = stderr.slice(-10000); });
    child.on("exit", () => managedProcesses.delete(id));

    return { id, pid: child.pid, label };
  },
};

export const processListTool: Tool = {
  name: "process_list",
  description: "List all managed background processes",
  parameters: [],
  async execute() {
    return [...managedProcesses.entries()].map(([id, proc]) => ({
      id,
      pid: proc.pid,
      running: !proc.killed,
    }));
  },
};

export const processKillTool: Tool = {
  name: "process_kill",
  description: "Kill a managed background process",
  parameters: [
    { name: "id", type: "string", description: "Process ID from process_spawn", required: true },
  ],
  async execute(args) {
    const id = args.id as string;
    const proc = managedProcesses.get(id);
    if (!proc) throw new Error(`Process not found: ${id}`);
    proc.kill("SIGTERM");
    managedProcesses.delete(id);
    return { killed: id };
  },
};

// ─── Phase 2.3: cron scheduler ──────────────────────────────────────────────────────

interface CronJob {
  id: string;
  expression: string;
  command: string;
  description: string;
  nextRun: number;
  lastRun?: number;
  runCount: number;
  timer: ReturnType<typeof setTimeout>;
}

const cronJobs = new Map<string, CronJob>();

/** Parse a minimal cron expression (5 fields) and return ms until next fire. */
function msUntilNext(expression: string): number {
  const parts = expression.trim().split(/\s+/);
  if (parts.length !== 5) throw new Error("Cron expression must have 5 fields: min hour dom mon dow");
  const [minPart, hourPart] = parts;
  const now = new Date();
  const next = new Date(now);
  next.setSeconds(0, 0);
  // Simple: if only numeric minute/hour, compute exact next occurrence.
  // For '*' fields, fire on next minute boundary.
  if (minPart === "*" && hourPart === "*") {
    next.setMinutes(next.getMinutes() + 1);
    return next.getTime() - now.getTime();
  }
  const targetMin = minPart === "*" ? now.getMinutes() : parseInt(minPart, 10);
  const targetHour = hourPart === "*" ? now.getHours() : parseInt(hourPart, 10);
  next.setHours(targetHour, targetMin, 0, 0);
  if (next <= now) next.setDate(next.getDate() + 1);
  return next.getTime() - now.getTime();
}

function scheduleCron(job: CronJob, execute: (cmd: string) => void): void {
  const delay = msUntilNext(job.expression);
  job.nextRun = Date.now() + delay;
  job.timer = setTimeout(() => {
    job.lastRun = Date.now();
    job.runCount++;
    try { execute(job.command); } catch { /* best-effort */ }
    scheduleCron(job, execute); // reschedule
  }, delay);
  if (job.timer.unref) job.timer.unref();
}

export const cronScheduleTool: Tool = {
  name: "cron_schedule",
  description: "Schedule a shell command to run on a cron schedule (5-field cron expression). Returns a job ID.",
  parameters: [
    { name: "expression", type: "string", description: "Cron expression (5 fields: min hour dom mon dow). Example: '0 9 * * 1' = 9am every Monday.", required: true },
    { name: "command", type: "string", description: "Shell command to run", required: true },
    { name: "description", type: "string", description: "Human-readable description", required: false, default: "" },
  ],
  async execute(args) {
    const { execSync } = await import("node:child_process");
    const expression = args.expression as string;
    const command = args.command as string;
    const description = (args.description as string) || command;
    msUntilNext(expression); // validate — throws if invalid
    const id = randomUUID().slice(0, 8);
    const job: CronJob = { id, expression, command, description, nextRun: 0, runCount: 0, timer: null as any };
    scheduleCron(job, (cmd) => execSync(cmd, { shell: "/bin/sh" }));
    cronJobs.set(id, job);
    return { id, expression, command, description, nextRun: new Date(job.nextRun).toISOString() };
  },
};

export const cronListTool: Tool = {
  name: "cron_list",
  description: "List all active cron jobs with their schedule, last run, and next run times.",
  parameters: [],
  async execute() {
    return [...cronJobs.values()].map((j) => ({
      id: j.id,
      expression: j.expression,
      description: j.description,
      nextRun: new Date(j.nextRun).toISOString(),
      lastRun: j.lastRun ? new Date(j.lastRun).toISOString() : null,
      runCount: j.runCount,
    }));
  },
};

export const cronCancelTool: Tool = {
  name: "cron_cancel",
  description: "Cancel a scheduled cron job by ID.",
  parameters: [
    { name: "id", type: "string", description: "Job ID from cron_schedule", required: true },
  ],
  async execute(args) {
    const id = args.id as string;
    const job = cronJobs.get(id);
    if (!job) throw new Error(`Cron job not found: ${id}`);
    clearTimeout(job.timer);
    cronJobs.delete(id);
    return { cancelled: id };
  },
};

// ─── Memory Tools ───────────────────────────────────────────────────────────

export function createMemoryTools(
  memory: import("../memory/index.js").MemoryManager,
): Tool[] {
  return [
    {
      name: "memory_search",
      description: "Search episodic memory for past conversations and knowledge. Optionally filter by session.",
      parameters: [
        { name: "query", type: "string", description: "Search query", required: true },
        { name: "limit", type: "number", description: "Max results (default: 5)", required: false, default: 5 },
        { name: "session_id", type: "string", description: "Filter results to a specific session ID", required: false },
      ],
      async execute(args) {
        const results = memory.searchEpisodes(args.query as string, {
          limit: (args.limit as number) || 5,
          sessionId: args.session_id as string | undefined,
        });
        return results.map((r) => ({
          content: r.content.slice(0, 500),
          date: new Date(r.validAt).toISOString(),
          tags: r.tags,
          sessionId: r.sessionId,
        }));
      },
    },
    {
      name: "memory_store",
      description: "Store an important fact, decision, or insight in long-term memory",
      parameters: [
        { name: "content", type: "string", description: "What to remember", required: true },
        { name: "tags", type: "array", description: "Tags for categorization", required: false },
      ],
      async execute(args) {
        const episode = memory.addEpisode({
          sessionId: "manual",
          content: args.content as string,
          validAt: Date.now(),
          tags: (args.tags as string[]) || [],
        });
        return { stored: episode.id };
      },
    },
    {
      name: "vault_read",
      description: "Read self-knowledge from the semantic vault",
      parameters: [
        { name: "category", type: "string", description: "Category (identity, expertise, preference, constraint, methodology) or omit for all", required: false },
      ],
      async execute(args) {
        if (args.category) return memory.getVaultNotes(args.category as string);
        return memory.getVaultContext();
      },
    },
    {
      name: "memory_status",
      description: "Get memory system status",
      parameters: [],
      async execute() {
        const status = memory.getStatus();
        const alignment = memory.getAlignmentStats();
        return { ...status, alignment };
      },
    },
  ];
}

// ─── Session Management Tools (Phase 8) ──────────────────────────────────

export function createSessionTools(
  memory: import("../memory/index.js").MemoryManager,
): Tool[] {
  return [
    {
      name: "session_list",
      description: "List all saved sessions",
      parameters: [
        { name: "limit", type: "number", description: "Max sessions (default: 20)", required: false, default: 20 },
      ],
      async execute(args) {
        return memory.listSessions({ limit: (args.limit as number) || 20 });
      },
    },
    {
      name: "session_delete",
      description: "Delete a session by ID",
      parameters: [
        { name: "session_id", type: "string", description: "Session ID to delete", required: true },
      ],
      async execute(args) {
        memory.deleteSession(args.session_id as string);
        return { deleted: args.session_id };
      },
    },
    {
      name: "session_tag",
      description: "Add tags to a session for organization",
      parameters: [
        { name: "session_id", type: "string", description: "Session ID", required: true },
        { name: "tags", type: "array", description: "Tags to add", required: true },
      ],
      async execute(args) {
        memory.tagSession(args.session_id as string, args.tags as string[]);
        return { tagged: args.session_id, tags: args.tags };
      },
    },
    {
      name: "session_prune",
      description: "Delete sessions older than N days",
      parameters: [
        { name: "days", type: "number", description: "Delete sessions older than this many days", required: true },
      ],
      async execute(args) {
        const count = memory.pruneSessions((args.days as number) * 86_400_000);
        return { pruned: count };
      },
    },
    {
      name: "session_search",
      description: "Search within a specific session's messages (Phase 2.7)",
      parameters: [
        { name: "session_id", type: "string", description: "Session ID to search within", required: true },
        { name: "query", type: "string", description: "Search query (text to find in messages)", required: true },
        { name: "limit", type: "number", description: "Max results (default: 20)", required: false, default: 20 },
      ],
      async execute(args) {
        const results = memory.searchSessionMessages(
          args.session_id as string,
          args.query as string,
          { limit: (args.limit as number) || 20 },
        );
        return results.map((m) => ({
          role: m.role,
          content: m.content.slice(0, 500),
          timestamp: new Date(m.timestamp).toISOString(),
        }));
      },
    },
  ];
}

// ─── Register All Built-in Tools ──────────────────────────────────────────

export function createToolRegistry(
  config: {
    shell?: boolean;
    filesystem?: boolean;
    git?: boolean;
    browser?: boolean;
    http?: boolean;
    web_search?: boolean;
    web_fetch?: boolean;
    apply_patch?: boolean;
    process?: boolean;
    image?: boolean;
    pdf?: boolean;
  },
  memory?: import("../memory/index.js").MemoryManager,
  llm?: LLMRouter,
): ToolRegistry {
  const registry = new ToolRegistry();

  if (config.shell !== false) registry.register(shellTool);
  if (config.filesystem !== false) {
    registry.register(readFileTool);
    registry.register(writeFileTool);
    registry.register(listDirTool);
    registry.register(searchFilesTool);
  }
  if (config.git !== false) registry.register(gitTool);
  if (config.http !== false) registry.register(httpTool);
  if (config.web_fetch !== false) registry.register(webFetchTool);
  if (config.apply_patch !== false) registry.register(applyPatchTool);
  if (config.process !== false) {
    registry.register(processSpawnTool);
    registry.register(processListTool);
    registry.register(processKillTool);
    registry.register(cronScheduleTool);
    registry.register(cronListTool);
    registry.register(cronCancelTool);
  }

  // Web search (Phase 3.2)
  if (config.web_search) {
    registry.register(webSearchTool);
  }

  // Browser tools (Phase 3.1) — Playwright peer dependency
  if (config.browser) {
    for (const tool of browserTools) {
      registry.register(tool);
    }
  }

  // Image analysis (Phase 3.6) — requires LLM for vision
  if (config.image !== false && llm) {
    registry.register(createImageTool(llm));
  }

  // PDF extraction (Phase 3.7)
  if (config.pdf !== false) {
    registry.register(pdfReadTool);
  }

  if (memory) {
    for (const tool of createMemoryTools(memory)) {
      registry.register(tool);
    }
    for (const tool of createSessionTools(memory)) {
      registry.register(tool);
    }
  }

  return registry;
}
