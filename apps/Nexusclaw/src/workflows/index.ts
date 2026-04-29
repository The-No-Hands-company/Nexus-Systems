// Vertical Workflow Plugins — Phase 3.2 + 3.1 (HiClaw-style workers)
//
// Provides three workflow abstractions built on top of the existing
// Orchestrator and TeamManager:
//
//  1. WorkerPool      — HiClaw-style manager/worker task distribution
//  2. ResearchWorkflow — Research → Outline → Write → Review pipeline
//  3. BenchmarkRunner — Runs a suite of scored tasks and reports metrics

import { randomUUID } from "node:crypto";
import { join } from "node:path";
import { existsSync, mkdirSync, writeFileSync, readFileSync } from "node:fs";
import type { Orchestrator } from "../core/orchestrator.js";
import type { TeamManager } from "../teams/index.js";
import { getDataDir } from "../config/index.js";

// ─── 1. WorkerPool — HiClaw-style manager/worker orchestration ────────────
//
// A manager agent decomposes a goal into a list of sub-tasks.
// N worker agents run those tasks in parallel (up to concurrency limit).
// Results are collected and merged into a final summary by the manager.

export interface WorkerTask {
  id: string;
  description: string;
  priority?: number;
  /** Worker that picked up the task */
  workerId?: string;
  status: "queued" | "running" | "done" | "failed";
  result?: string;
  error?: string;
  startedAt?: number;
  completedAt?: number;
}

export interface WorkerPoolOptions {
  /** Number of parallel workers.  Default: 3 */
  concurrency?: number;
  /** Model to use for the manager agent.  Default: nexus-ai/auto */
  managerModel?: string;
  /** Model to use for worker agents.  Default: nexus-ai/auto */
  workerModel?: string;
  /** Soul/system prompt override for the manager */
  managerSoul?: string;
  /** Soul/system prompt override for workers */
  workerSoul?: string;
}

export interface WorkerPoolRun {
  runId: string;
  goal: string;
  tasks: WorkerTask[];
  finalSummary: string;
  startedAt: number;
  completedAt: number;
  workerCount: number;
}

export class WorkerPool {
  private opts: Required<WorkerPoolOptions>;

  constructor(opts: WorkerPoolOptions = {}) {
    this.opts = {
      concurrency: opts.concurrency ?? 3,
      managerModel: opts.managerModel ?? "nexus-ai/auto",
      workerModel: opts.workerModel ?? "nexus-ai/auto",
      managerSoul: opts.managerSoul ??
        "You are a manager agent. Given a high-level goal, decompose it into clear, parallel-executable sub-tasks. Output ONLY a JSON array of task descriptions, e.g. [\"task 1\", \"task 2\"].",
      workerSoul: opts.workerSoul ??
        "You are a worker agent. Complete the given task thoroughly and concisely. Output only your result.",
    };
  }

  async run(goal: string, orchestrator: Orchestrator): Promise<WorkerPoolRun> {
    const runId = randomUUID().slice(0, 8);
    const startedAt = Date.now();

    // ── Step 1: Manager decomposes the goal ──────────────────────────────
    const managerId = `worker-manager-${runId}`;
    orchestrator.createAgent({
      id: managerId,
      name: "Manager",
      model: this.opts.managerModel,
      soul: this.opts.managerSoul,
    });

    let taskDescriptions: string[] = [];
    try {
      const managerResponse = await orchestrator.sendMessage(
        `Goal: ${goal}\n\nDecompose this into parallel sub-tasks. Respond with ONLY a JSON array of task description strings.`,
        managerId,
      );
      const raw = managerResponse.content ?? "[]";
      // Extract JSON array from response (tolerates surrounding text)
      const match = raw.match(/\[[\s\S]*\]/);
      if (match) {
        taskDescriptions = JSON.parse(match[0]) as string[];
      }
    } catch {
      taskDescriptions = [goal]; // fallback: treat entire goal as one task
    } finally {
      orchestrator.removeAgent(managerId);
    }

    // ── Step 2: Workers run tasks in parallel (up to concurrency) ────────
    const tasks: WorkerTask[] = taskDescriptions.map((desc) => ({
      id: randomUUID().slice(0, 8),
      description: desc,
      status: "queued",
    }));

    // Batch into chunks of size `concurrency`
    const chunks: WorkerTask[][] = [];
    for (let i = 0; i < tasks.length; i += this.opts.concurrency) {
      chunks.push(tasks.slice(i, i + this.opts.concurrency));
    }

    for (const chunk of chunks) {
      await Promise.all(
        chunk.map(async (task) => {
          const workerId = `worker-${runId}-${task.id}`;
          task.workerId = workerId;
          task.status = "running";
          task.startedAt = Date.now();

          orchestrator.createAgent({
            id: workerId,
            name: `Worker ${task.id}`,
            model: this.opts.workerModel,
            soul: this.opts.workerSoul,
          });

          try {
            const res = await orchestrator.sendMessage(task.description, workerId);
            task.result = res.content ?? "";
            task.status = "done";
          } catch (err: any) {
            task.error = err.message;
            task.status = "failed";
          } finally {
            task.completedAt = Date.now();
            orchestrator.removeAgent(workerId);
          }
        }),
      );
    }

    // ── Step 3: Manager merges results ───────────────────────────────────
    const summarizerId = `worker-summary-${runId}`;
    orchestrator.createAgent({
      id: summarizerId,
      name: "Summarizer",
      model: this.opts.managerModel,
      soul: "You are a synthesis agent. Given multiple task results, produce a clear, comprehensive final summary that combines all the work.",
    });

    let finalSummary = "";
    try {
      const resultsText = tasks
        .filter((t) => t.status === "done")
        .map((t, i) => `Task ${i + 1}: ${t.description}\nResult: ${t.result}`)
        .join("\n\n---\n\n");

      const summaryRes = await orchestrator.sendMessage(
        `Merge and synthesise the following completed task results into a comprehensive final answer:\n\n${resultsText}`,
        summarizerId,
      );
      finalSummary = summaryRes.content ?? "";
    } catch (err: any) {
      finalSummary = `Summary failed: ${err.message}`;
    } finally {
      orchestrator.removeAgent(summarizerId);
    }

    return {
      runId,
      goal,
      tasks,
      finalSummary,
      startedAt,
      completedAt: Date.now(),
      workerCount: this.opts.concurrency,
    };
  }
}

// ─── 2. ResearchWorkflow ────────────────────────────────────────────────────
//
// A purpose-built research pipeline using a 4-stage agent graph:
//   Planner → Researcher → Writer → Editor
//
// Could be registered as a team template but is implemented here as a
// first-class workflow to support per-stage streaming and artifact saving.

export interface ResearchOptions {
  /** Maximum depth of research topics to pursue.  Default: 3 */
  depth?: number;
  model?: string;
  /** Where to save the final document.  Default: ~/.anyclaw/data/research/ */
  outputDir?: string;
}

export interface ResearchResult {
  runId: string;
  topic: string;
  outline: string;
  researchNotes: string;
  draft: string;
  finalDocument: string;
  outputPath?: string;
  completedAt: number;
  durationMs: number;
}

export class ResearchWorkflow {
  private opts: Required<ResearchOptions>;

  constructor(opts: ResearchOptions = {}) {
    this.opts = {
      depth: opts.depth ?? 3,
      model: opts.model ?? "nexus-ai/auto",
      outputDir: opts.outputDir ?? join(getDataDir(), "data", "research"),
    };
  }

  async run(topic: string, orchestrator: Orchestrator): Promise<ResearchResult> {
    const runId = randomUUID().slice(0, 8);
    const startedAt = Date.now();

    // ── Stage 1: Planner creates a research outline ───────────────────────
    const plannerId = `research-planner-${runId}`;
    orchestrator.createAgent({
      id: plannerId,
      name: "Research Planner",
      model: this.opts.model,
      soul: `You are a research planning agent. Given a topic, produce a detailed research outline with ${this.opts.depth} main sections. Each section should have 2–3 sub-questions to investigate. Output in clean Markdown.`,
    });

    let outline = "";
    try {
      const res = await orchestrator.sendMessage(
        `Create a comprehensive research outline for: "${topic}"`,
        plannerId,
      );
      outline = res.content ?? "";
    } finally {
      orchestrator.removeAgent(plannerId);
    }

    // ── Stage 2: Researcher gathers information ──────────────────────────
    const researcherId = `researcher-${runId}`;
    orchestrator.createAgent({
      id: researcherId,
      name: "Researcher",
      model: this.opts.model,
      soul: "You are a deep research agent. Given a research outline, systematically investigate each section. Gather facts, context, and supporting evidence. Output detailed research notes in Markdown with clear headings for each section.",
      allowedTools: ["web_fetch", "web_search"],
    });

    let researchNotes = "";
    try {
      const res = await orchestrator.sendMessage(
        `Research the following outline thoroughly:\n\n${outline}`,
        researcherId,
      );
      researchNotes = res.content ?? "";
    } finally {
      orchestrator.removeAgent(researcherId);
    }

    // ── Stage 3: Writer produces a draft document ────────────────────────
    const writerId = `writer-${runId}`;
    orchestrator.createAgent({
      id: writerId,
      name: "Writer",
      model: this.opts.model,
      soul: "You are a technical writer. Given research notes, produce a well-structured, accessible, and authoritative document. Use clear headings, concise prose, and cite key evidence. Output polished Markdown.",
    });

    let draft = "";
    try {
      const res = await orchestrator.sendMessage(
        `Write a comprehensive document about "${topic}" based on these research notes:\n\n${researchNotes}`,
        writerId,
      );
      draft = res.content ?? "";
    } finally {
      orchestrator.removeAgent(writerId);
    }

    // ── Stage 4: Editor refines the draft ────────────────────────────────
    const editorId = `editor-${runId}`;
    orchestrator.createAgent({
      id: editorId,
      name: "Editor",
      model: this.opts.model,
      soul: "You are a senior editor. Review the draft for clarity, accuracy, flow, and completeness. Fix any issues and improve the writing. Output the final polished document in Markdown.",
    });

    let finalDocument = draft;
    try {
      const res = await orchestrator.sendMessage(
        `Review and improve this draft document:\n\n${draft}`,
        editorId,
      );
      finalDocument = res.content ?? draft;
    } finally {
      orchestrator.removeAgent(editorId);
    }

    // ── Save output ───────────────────────────────────────────────────────
    let outputPath: string | undefined;
    try {
      mkdirSync(this.opts.outputDir, { recursive: true });
      const safeTopic = topic.replace(/[^a-z0-9\-_ ]/gi, "").replace(/\s+/g, "-").slice(0, 60);
      outputPath = join(this.opts.outputDir, `${safeTopic}-${runId}.md`);
      writeFileSync(outputPath, finalDocument, "utf-8");
    } catch { /* don't fail if we can't write */ }

    return {
      runId,
      topic,
      outline,
      researchNotes,
      draft,
      finalDocument,
      outputPath,
      completedAt: Date.now(),
      durationMs: Date.now() - startedAt,
    };
  }
}

// ─── 3. BenchmarkRunner ────────────────────────────────────────────────────
//
// Runs a suite of scored tasks to evaluate agent capability and cost/speed.
// Each task has expected characteristics (difficulty, required tools, expected
// output patterns).  The scorer grades results 0–100.

export interface BenchmarkTask {
  id: string;
  name: string;
  prompt: string;
  /** Keywords or patterns that should appear in a good answer */
  successPatterns?: string[];
  /** Maximum expected tokens in response */
  maxExpectedTokens?: number;
  difficulty: "easy" | "medium" | "hard";
  category: "reasoning" | "coding" | "writing" | "research" | "math";
  allowedTools?: string[];
}

export interface BenchmarkTaskResult {
  taskId: string;
  taskName: string;
  prompt: string;
  response: string;
  score: number;           // 0–100
  scoreBreakdown: {
    lenient: number;       // Partial credit for any sensible response
    patterns: number;      // Credit for matching success patterns
    brevity: number;       // Credit for not being excessively long
  };
  usage: { inputTokens: number; outputTokens: number };
  durationMs: number;
  error?: string;
}

export interface BenchmarkSuite {
  id: string;
  name: string;
  description: string;
  tasks: BenchmarkTask[];
}

export interface BenchmarkRunResult {
  runId: string;
  suiteId: string;
  model: string;
  startedAt: number;
  completedAt: number;
  durationMs: number;
  totalScore: number;
  maxScore: number;
  percentScore: number;
  taskResults: BenchmarkTaskResult[];
  summary: string;
}

// Built-in benchmark suites
export const BUILTIN_SUITES: BenchmarkSuite[] = [
  {
    id: "nexus-core",
    name: "Nexus Core Benchmark",
    description: "Tests core reasoning, writing, and instruction-following capabilities",
    tasks: [
      {
        id: "reason-1",
        name: "Chain-of-thought reasoning",
        prompt: "If it takes 5 machines 5 minutes to make 5 widgets, how long would it take 100 machines to make 100 widgets? Explain your reasoning step by step.",
        successPatterns: ["5 minutes", "each machine", "parallel"],
        difficulty: "easy",
        category: "reasoning",
      },
      {
        id: "code-1",
        name: "FizzBuzz",
        prompt: "Write a JavaScript function that prints numbers 1 to 30. For multiples of 3 print 'Fizz', for multiples of 5 print 'Buzz', for multiples of both print 'FizzBuzz'.",
        successPatterns: ["FizzBuzz", "for", "if", "function"],
        difficulty: "easy",
        category: "coding",
      },
      {
        id: "write-1",
        name: "Technical summary",
        prompt: "Write a 3-sentence summary of what a REST API is, suitable for a developer who has never used one.",
        successPatterns: ["HTTP", "endpoint", "request", "response"],
        difficulty: "easy",
        category: "writing",
      },
      {
        id: "reason-2",
        name: "Logic puzzle",
        prompt: "Alice, Bob, and Carol each have a different pet (cat, dog, fish). Alice does not have the cat. Bob does not have the dog. Carol has the fish. Who has what pet?",
        successPatterns: ["Alice", "Bob", "Carol", "dog", "cat", "fish"],
        difficulty: "medium",
        category: "reasoning",
      },
      {
        id: "code-2",
        name: "Async/await pattern",
        prompt: "Write a TypeScript function that fetches user data from https://example.com/api/users/{id} and returns the user's name, handling errors gracefully.",
        successPatterns: ["async", "await", "fetch", "try", "catch"],
        difficulty: "medium",
        category: "coding",
      },
    ],
  },
  {
    id: "nexus-advanced",
    name: "Nexus Advanced Benchmark",
    description: "Tests advanced capabilities including multi-step reasoning and code generation",
    tasks: [
      {
        id: "adv-reason-1",
        name: "Multi-step planning",
        prompt: "You need to plan a database migration from PostgreSQL to MongoDB for a 10M user social media app with zero downtime. List the 5 most critical steps and risks.",
        successPatterns: ["dual write", "migration", "rollback", "testing", "downtime"],
        difficulty: "hard",
        category: "reasoning",
      },
      {
        id: "adv-code-1",
        name: "Algorithm implementation",
        prompt: "Implement a binary search tree in TypeScript with insert, search, and inorder traversal methods.",
        successPatterns: ["insert", "search", "inorder", "left", "right", "node"],
        difficulty: "hard",
        category: "coding",
      },
    ],
  },
];

export class BenchmarkRunner {
  async run(
    suiteId: string,
    orchestrator: Orchestrator,
    opts: { model?: string; agentId?: string } = {},
  ): Promise<BenchmarkRunResult> {
    const suite = BUILTIN_SUITES.find((s) => s.id === suiteId);
    if (!suite) throw new Error(`Suite not found: ${suiteId}. Available: ${BUILTIN_SUITES.map((s) => s.id).join(", ")}`);

    const runId = randomUUID().slice(0, 8);
    const startedAt = Date.now();
    const agentId = opts.agentId ?? `benchmark-${runId}`;
    const model = opts.model ?? "nexus-ai/auto";

    // Create a dedicated benchmark agent
    orchestrator.createAgent({
      id: agentId,
      name: "Benchmark Agent",
      model,
      soul: "You are a capability evaluation agent. Answer questions directly, concisely, and accurately.",
      maxTokens: 1024,
      temperature: 0.3,
    });

    const taskResults: BenchmarkTaskResult[] = [];

    try {
      for (const task of suite.tasks) {
        const taskStart = Date.now();
        let response = "";
        let usage = { inputTokens: 0, outputTokens: 0 };
        let error: string | undefined;

        try {
          const res = await orchestrator.sendMessage(task.prompt, agentId);
          response = res.content ?? "";
          usage = res.usage ?? { inputTokens: 0, outputTokens: 0 };
        } catch (err: any) {
          error = err.message;
          response = "";
        }

        const score = this._scoreTask(task, response, error);
        taskResults.push({
          taskId: task.id,
          taskName: task.name,
          prompt: task.prompt,
          response,
          score: score.total,
          scoreBreakdown: score.breakdown,
          usage,
          durationMs: Date.now() - taskStart,
          error,
        });
      }
    } finally {
      orchestrator.removeAgent(agentId);
    }

    const totalScore = taskResults.reduce((s, r) => s + r.score, 0);
    const maxScore = suite.tasks.length * 100;
    const percentScore = maxScore > 0 ? Math.round((totalScore / maxScore) * 100) : 0;

    const passCount = taskResults.filter((r) => r.score >= 60).length;
    const summary = `Benchmark: ${suite.name} | Model: ${model} | Score: ${percentScore}% (${passCount}/${suite.tasks.length} passed)`;

    // Save results to disk
    try {
      const resultsDir = join(getDataDir(), "data", "benchmarks");
      mkdirSync(resultsDir, { recursive: true });
      const result: BenchmarkRunResult = {
        runId,
        suiteId,
        model,
        startedAt,
        completedAt: Date.now(),
        durationMs: Date.now() - startedAt,
        totalScore,
        maxScore,
        percentScore,
        taskResults,
        summary,
      };
      writeFileSync(
        join(resultsDir, `${suiteId}-${runId}.json`),
        JSON.stringify(result, null, 2),
        "utf-8",
      );
      return result;
    } catch {
      return {
        runId,
        suiteId,
        model,
        startedAt,
        completedAt: Date.now(),
        durationMs: Date.now() - startedAt,
        totalScore,
        maxScore,
        percentScore,
        taskResults,
        summary,
      };
    }
  }

  private _scoreTask(
    task: BenchmarkTask,
    response: string,
    error?: string,
  ): { total: number; breakdown: BenchmarkTaskResult["scoreBreakdown"] } {
    if (error || !response) {
      return { total: 0, breakdown: { lenient: 0, patterns: 0, brevity: 0 } };
    }

    // Lenient: any non-empty response gets some credit
    const lenient = response.length > 20 ? 30 : 10;

    // Pattern matching
    const patterns = task.successPatterns ?? [];
    const matched = patterns.filter((p) =>
      response.toLowerCase().includes(p.toLowerCase()),
    ).length;
    const patternScore = patterns.length > 0
      ? Math.round((matched / patterns.length) * 50)
      : 30;

    // Brevity: penalise extremely long responses (>5000 chars)
    const brevityScore = response.length > 5000 ? 10 : response.length > 2000 ? 15 : 20;

    const total = Math.min(100, lenient + patternScore + brevityScore);
    return { total, breakdown: { lenient, patterns: patternScore, brevity: brevityScore } };
  }
}

// ─── 4. FeatureStubScaffolder ─────────────────────────────────────────────
//
// Converts documentation backlog items into executable scaffolding metadata.
// This is the first "stub everything" pass for large feature waves.

export type FeatureStubStatus = "planned" | "scaffolded" | "in_progress" | "done";

export interface FeatureStub {
  id: string;
  title: string;
  sourceDoc: string;
  sourceLine: number;
  modules: string[];
  status: FeatureStubStatus;
}

export interface FeatureStubCatalog {
  generatedAt: number;
  total: number;
  byModule: Record<string, number>;
  stubs: FeatureStub[];
}

const FEATURE_ID_LINE_RE = /^(?:\d+\.\s*)?([A-Z]{1,5}\d?-?\d{3})\s+(.+)$/;

const FEATURE_PREFIX_MODULE_MAP: Array<{ prefix: string; modules: string[] }> = [
  { prefix: "A-", modules: ["core", "workflows"] },
  { prefix: "T-", modules: ["gsd", "workflows"] },
  { prefix: "M-", modules: ["memory"] },
  { prefix: "X-", modules: ["tools"] },
  { prefix: "P-", modules: ["mcp", "gateway"] },
  { prefix: "S-", modules: ["skills", "plugins"] },
  { prefix: "G-", modules: ["safety"] },
  { prefix: "Q-", modules: ["secrets", "safety"] },
  { prefix: "D-", modules: ["artifacts"] },
  { prefix: "U-", modules: ["gateway"] },
  { prefix: "C-", modules: ["teams", "rbac"] },
  { prefix: "O-", modules: ["workflows", "hooks"] },
  { prefix: "E-", modules: ["metrics", "logger"] },
  { prefix: "R-", modules: ["llm"] },
  { prefix: "V-", modules: ["platform", "adapters"] },
  { prefix: "H-", modules: ["cloud"] },
  { prefix: "K-", modules: ["marketplace"] },
  { prefix: "F-", modules: ["core", "memory"] },
  { prefix: "Z-", modules: ["recipes", "workflows"] },
  { prefix: "N-", modules: ["core", "platform"] },
  { prefix: "DX2-", modules: ["core", "mcp", "safety", "memory", "metrics"] },
  { prefix: "DX3-", modules: ["skills", "marketplace", "safety", "teams", "workflows"] },
  { prefix: "CM-", modules: ["core", "mcp", "memory"] },
  { prefix: "CCM-", modules: ["skills", "safety", "platform"] },
  { prefix: "VG-", modules: ["metrics", "logger"] },
  { prefix: "VG3-", modules: ["metrics", "logger", "safety"] },
];

function normalizeDocName(path: string): string {
  const parts = path.split("/");
  return parts[parts.length - 1] || path;
}

function parseModulesFromSourceLine(line: string): string[] {
  const found = new Set<string>();
  const re = /src\/([a-zA-Z0-9_-]+)/g;
  let match: RegExpExecArray | null = re.exec(line);
  while (match) {
    found.add(match[1]);
    match = re.exec(line);
  }
  return [...found];
}

function inferModulesForFeature(id: string, sourceLine: string): string[] {
  const explicit = parseModulesFromSourceLine(sourceLine);
  if (explicit.length > 0) return explicit;

  for (const rule of FEATURE_PREFIX_MODULE_MAP) {
    if (id.startsWith(rule.prefix)) return rule.modules;
  }

  return ["core"];
}

export class FeatureStubScaffolder {
  private docs: string[];

  constructor(docs: string[]) {
    this.docs = docs;
  }

  buildCatalog(): FeatureStubCatalog {
    const stubsById = new Map<string, FeatureStub>();

    for (const docPath of this.docs) {
      if (!existsSync(docPath)) continue;
      const text = readFileSync(docPath, "utf-8");
      const lines = text.split(/\r?\n/);

      for (let i = 0; i < lines.length; i++) {
        const line = lines[i].trim();
        const match = line.match(FEATURE_ID_LINE_RE);
        if (!match) continue;

        const id = match[1];
        const title = match[2].trim();
        if (!title) continue;

        if (!stubsById.has(id)) {
          stubsById.set(id, {
            id,
            title,
            sourceDoc: normalizeDocName(docPath),
            sourceLine: i + 1,
            modules: inferModulesForFeature(id, line),
            status: "planned",
          });
        }
      }
    }

    const stubs = [...stubsById.values()].sort((a, b) => a.id.localeCompare(b.id));
    const byModule: Record<string, number> = {};
    for (const stub of stubs) {
      for (const module of stub.modules) {
        byModule[module] = (byModule[module] ?? 0) + 1;
      }
    }

    return {
      generatedAt: Date.now(),
      total: stubs.length,
      byModule,
      stubs,
    };
  }

  saveCatalog(outputDir = ".nexus"): { outputPath: string; catalog: FeatureStubCatalog } {
    const catalog = this.buildCatalog();
    mkdirSync(outputDir, { recursive: true });
    const outputPath = join(outputDir, "feature-stubs.json");
    writeFileSync(outputPath, JSON.stringify(catalog, null, 2), "utf-8");
    return { outputPath, catalog };
  }

  renderSummary(catalog?: FeatureStubCatalog): string {
    const activeCatalog = catalog ?? this.buildCatalog();
    const modules = Object.entries(activeCatalog.byModule)
      .sort((a, b) => b[1] - a[1])
      .map(([module, count]) => `${module}:${count}`)
      .join(", ");
    return `Feature stubs: ${activeCatalog.total} total | modules: ${modules}`;
  }
}
