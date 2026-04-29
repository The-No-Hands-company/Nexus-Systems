// ─── Nexusclaw Metrics Collector ───────────────────────────────────────────────
// Phase 5: Token usage tracking, tool call counters, cost estimation, per-agent
// stats, latency histograms. Designed as an append-only in-memory store with
// optional file persistence to ~/.anyclaw/data/metrics.jsonl

import * as fs from "fs";
import * as path from "path";
import * as os from "os";

// ─── Types ──────────────────────────────────────────────────────────────────

export interface TokenUsageRecord {
  ts: number;
  agentId: string;
  sessionId?: string;
  model: string;
  inputTokens: number;
  outputTokens: number;
  costUsd: number;
}

export interface ToolCallRecord {
  ts: number;
  agentId: string;
  sessionId?: string;
  toolName: string;
  durationMs: number;
  success: boolean;
  error?: string;
}

export interface LatencyRecord {
  ts: number;
  agentId: string;
  type: "llm" | "tool" | "session" | "request";
  durationMs: number;
}

export interface MetricsSnapshot {
  totalInputTokens: number;
  totalOutputTokens: number;
  totalCostUsd: number;
  totalToolCalls: number;
  failedToolCalls: number;
  uniqueAgents: number;
  uniqueSessions: number;
  topTools: Array<{ name: string; calls: number; failRate: number }>;
  topModels: Array<{ model: string; inputTokens: number; outputTokens: number; costUsd: number }>;
  avgLlmLatencyMs: number;
  avgToolLatencyMs: number;
  tokensByAgent: Record<string, { input: number; output: number; cost: number }>;
  hourlyTokens: Array<{ hour: string; input: number; output: number }>;
  recentErrors: Array<{ ts: number; toolName: string; agentId: string; error: string }>;
}

// ─── Pricing table (per 1M tokens, USD) ─────────────────────────────────────

const TOKEN_PRICING: Record<string, { input: number; output: number }> = {
  "claude-opus-4-5":          { input: 15.00, output: 75.00 },
  "claude-sonnet-4-5":        { input: 3.00,  output: 15.00 },
  "claude-haiku-4-5":         { input: 0.80,  output: 4.00 },
  "claude-3-5-sonnet-20241022":{ input: 3.00,  output: 15.00 },
  "claude-3-opus-20240229":   { input: 15.00, output: 75.00 },
  "claude-3-haiku-20240307":  { input: 0.25,  output: 1.25 },
  "gpt-4o":                   { input: 5.00,  output: 15.00 },
  "gpt-4o-mini":              { input: 0.15,  output: 0.60 },
  "gpt-4-turbo":              { input: 10.00, output: 30.00 },
  "gpt-3.5-turbo":            { input: 0.50,  output: 1.50 },
};

function estimateTokenCost(model: string, inputTokens: number, outputTokens: number): number {
  const pricing = TOKEN_PRICING[model];
  if (!pricing) return 0;
  return (inputTokens / 1_000_000) * pricing.input + (outputTokens / 1_000_000) * pricing.output;
}

// ─── MetricsCollector ────────────────────────────────────────────────────────

export class MetricsCollector {
  private tokenUsage: TokenUsageRecord[] = [];
  private toolCalls: ToolCallRecord[] = [];
  private latencies: LatencyRecord[] = [];
  private enabled = false;
  private metricsFile: string | undefined;
  private maxRecords = 10_000; // cap in-memory to prevent unbounded growth

  enable(): void {
    this.enabled = true;
  }

  setFile(filePath: string): void {
    this.metricsFile = filePath;
    fs.mkdirSync(path.dirname(filePath), { recursive: true });
    this.enabled = true;
  }

  // ─── Record methods ───────────────────────────────────────────────────────

  recordTokenUsage(opts: {
    agentId: string;
    sessionId?: string;
    model: string;
    inputTokens: number;
    outputTokens: number;
  }): void {
    if (!this.enabled) return;
    const costUsd = estimateTokenCost(opts.model, opts.inputTokens, opts.outputTokens);
    const record: TokenUsageRecord = { ts: Date.now(), costUsd, ...opts };
    this.tokenUsage.push(record);
    if (this.tokenUsage.length > this.maxRecords) this.tokenUsage.shift();
    this._appendToFile({ type: "token_usage", ...record });
  }

  recordToolCall(opts: {
    agentId: string;
    sessionId?: string;
    toolName: string;
    durationMs: number;
    success: boolean;
    error?: string;
  }): void {
    if (!this.enabled) return;
    const record: ToolCallRecord = { ts: Date.now(), ...opts };
    this.toolCalls.push(record);
    if (this.toolCalls.length > this.maxRecords) this.toolCalls.shift();
    this._appendToFile({ type: "tool_call", ...record });
  }

  recordLatency(opts: {
    agentId: string;
    type: "llm" | "tool" | "session" | "request";
    durationMs: number;
  }): void {
    if (!this.enabled) return;
    const record: LatencyRecord = { ts: Date.now(), ...opts };
    this.latencies.push(record);
    if (this.latencies.length > this.maxRecords * 2) this.latencies.shift();
  }

  // ─── Snapshot / aggregation ───────────────────────────────────────────────

  snapshot(): MetricsSnapshot {
    const totalInputTokens = this.tokenUsage.reduce((s, r) => s + r.inputTokens, 0);
    const totalOutputTokens = this.tokenUsage.reduce((s, r) => s + r.outputTokens, 0);
    const totalCostUsd = this.tokenUsage.reduce((s, r) => s + r.costUsd, 0);

    const toolMap = new Map<string, { calls: number; fails: number }>();
    for (const r of this.toolCalls) {
      const e = toolMap.get(r.toolName) ?? { calls: 0, fails: 0 };
      e.calls++;
      if (!r.success) e.fails++;
      toolMap.set(r.toolName, e);
    }
    const topTools = [...toolMap.entries()]
      .sort((a, b) => b[1].calls - a[1].calls)
      .slice(0, 10)
      .map(([name, { calls, fails }]) => ({ name, calls, failRate: calls ? fails / calls : 0 }));

    const modelMap = new Map<string, { inputTokens: number; outputTokens: number; costUsd: number }>();
    for (const r of this.tokenUsage) {
      const e = modelMap.get(r.model) ?? { inputTokens: 0, outputTokens: 0, costUsd: 0 };
      e.inputTokens += r.inputTokens;
      e.outputTokens += r.outputTokens;
      e.costUsd += r.costUsd;
      modelMap.set(r.model, e);
    }
    const topModels = [...modelMap.entries()]
      .sort((a, b) => b[1].costUsd - a[1].costUsd)
      .slice(0, 8)
      .map(([model, stats]) => ({ model, ...stats }));

    const agentMap = new Map<string, { input: number; output: number; cost: number }>();
    for (const r of this.tokenUsage) {
      const e = agentMap.get(r.agentId) ?? { input: 0, output: 0, cost: 0 };
      e.input += r.inputTokens;
      e.output += r.outputTokens;
      e.cost += r.costUsd;
      agentMap.set(r.agentId, e);
    }

    const llmLats = this.latencies.filter(l => l.type === "llm").map(l => l.durationMs);
    const toolLats = this.latencies.filter(l => l.type === "tool").map(l => l.durationMs);
    const avg = (arr: number[]) => arr.length ? arr.reduce((s, v) => s + v, 0) / arr.length : 0;

    // Hourly token buckets for the last 24h
    const now = Date.now();
    const hourlyMap = new Map<string, { input: number; output: number }>();
    for (let h = 23; h >= 0; h--) {
      const d = new Date(now - h * 3_600_000);
      const key = `${d.toISOString().slice(0, 13)}:00`;
      hourlyMap.set(key, { input: 0, output: 0 });
    }
    for (const r of this.tokenUsage) {
      if (now - r.ts < 24 * 3_600_000) {
        const key = `${new Date(r.ts).toISOString().slice(0, 13)}:00`;
        const e = hourlyMap.get(key);
        if (e) { e.input += r.inputTokens; e.output += r.outputTokens; }
      }
    }
    const hourlyTokens = [...hourlyMap.entries()].map(([hour, v]) => ({ hour, ...v }));

    const recentErrors = this.toolCalls
      .filter(r => !r.success && r.error)
      .slice(-20)
      .map(r => ({ ts: r.ts, toolName: r.toolName, agentId: r.agentId, error: r.error! }));

    return {
      totalInputTokens,
      totalOutputTokens,
      totalCostUsd,
      totalToolCalls: this.toolCalls.length,
      failedToolCalls: this.toolCalls.filter(r => !r.success).length,
      uniqueAgents: new Set(this.tokenUsage.map(r => r.agentId)).size,
      uniqueSessions: new Set(this.tokenUsage.map(r => r.sessionId).filter(Boolean)).size,
      topTools,
      topModels,
      avgLlmLatencyMs: avg(llmLats),
      avgToolLatencyMs: avg(toolLats),
      tokensByAgent: Object.fromEntries(agentMap),
      hourlyTokens,
      recentErrors,
    };
  }

  // ─── Reset ────────────────────────────────────────────────────────────────

  reset(): void {
    this.tokenUsage = [];
    this.toolCalls = [];
    this.latencies = [];
  }

  // ─── Internal ─────────────────────────────────────────────────────────────

  private _appendToFile(record: Record<string, unknown>): void {
    if (!this.metricsFile) return;
    try {
      fs.appendFileSync(this.metricsFile, JSON.stringify(record) + "\n");
    } catch { /* never crash on logging */ }
  }
}

// ─── Singleton ────────────────────────────────────────────────────────────────

export const metrics = new MetricsCollector();

// Helper for default metrics file path
export function defaultMetricsFile(): string {
  return path.join(os.homedir(), ".anyclaw", "data", "metrics.jsonl");
}
