// Structured logger — Phase 7 (10.4)
// JSON-lines output, configurable levels, daily file rotation.

import { appendFileSync, mkdirSync, existsSync, renameSync } from "node:fs";
import { join, dirname } from "node:path";
import { homedir } from "node:os";

export type LogLevel = "debug" | "info" | "warn" | "error" | "silent";

const LEVELS: Record<LogLevel, number> = {
  debug: 0,
  info: 1,
  warn: 2,
  error: 3,
  silent: 99,
};

export interface LogEntry {
  ts: string;              // ISO timestamp
  level: LogLevel;
  component: string;       // e.g. "agent", "gateway", "tool", "llm"
  msg: string;
  [key: string]: unknown;  // extra structured fields
}

export class Logger {
  private level: number;
  private component: string;
  private filePath: string | null = null;
  private currentDate: string = "";
  private jsonMode: boolean;

  constructor(component: string, opts: { level?: LogLevel; jsonMode?: boolean } = {}) {
    this.component = component;
    this.level = LEVELS[opts.level ?? "info"];
    this.jsonMode = opts.jsonMode ?? false;
  }

  // ─── Configuration ────────────────────────────────────────────────

  setLevel(level: LogLevel): void {
    this.level = LEVELS[level];
  }

  setFile(path: string): void {
    this.filePath = path;
    const dir = dirname(path);
    if (!existsSync(dir)) mkdirSync(dir, { recursive: true });
  }

  child(component: string): Logger {
    const child = new Logger(`${this.component}:${component}`, {
      level: Object.entries(LEVELS).find(([, v]) => v === this.level)?.[0] as LogLevel ?? "info",
      jsonMode: this.jsonMode,
    });
    child.filePath = this.filePath;
    return child;
  }

  // ─── Core ──────────────────────────────────────────────────────────

  log(level: LogLevel, msg: string, fields: Record<string, unknown> = {}): void {
    if (LEVELS[level] < this.level) return;

    const entry: LogEntry = {
      ts: new Date().toISOString(),
      level,
      component: this.component,
      msg,
      ...fields,
    };

    this._write(entry);
  }

  debug(msg: string, fields?: Record<string, unknown>): void { this.log("debug", msg, fields); }
  info(msg: string, fields?: Record<string, unknown>): void { this.log("info", msg, fields); }
  warn(msg: string, fields?: Record<string, unknown>): void { this.log("warn", msg, fields); }
  error(msg: string, fields?: Record<string, unknown>): void { this.log("error", msg, fields); }

  // ─── Output ────────────────────────────────────────────────────────

  private _write(entry: LogEntry): void {
    const json = JSON.stringify(entry);

    // File output (JSONL)
    if (this.filePath) {
      this._rotateIfNeeded();
      try {
        appendFileSync(this.filePath, json + "\n", "utf8");
      } catch {
        // Silently fail file writes — don't crash the app due to logging
      }
    }

    // Console output
    if (!this.jsonMode) {
      const levelColors: Record<LogLevel, string> = {
        debug: "\x1b[90m",  // dark gray
        info:  "\x1b[36m",  // cyan
        warn:  "\x1b[33m",  // yellow
        error: "\x1b[31m",  // red
        silent: "",
      };
      const reset = "\x1b[0m";
      const dim = "\x1b[2m";
      const color = levelColors[entry.level] ?? "";
      const extras = Object.entries(entry)
        .filter(([k]) => !["ts", "level", "component", "msg"].includes(k))
        .map(([k, v]) => `${k}=${typeof v === "object" ? JSON.stringify(v) : v}`)
        .join(" ");
      const line = `${dim}${entry.ts.slice(11, 23)}${reset} ${color}${entry.level.toUpperCase().padEnd(5)}${reset} ${dim}[${entry.component}]${reset} ${entry.msg}${extras ? " " + dim + extras + reset : ""}`;
      if (entry.level === "error") process.stderr.write(line + "\n");
      else process.stdout.write(line + "\n");
    } else {
      process.stdout.write(json + "\n");
    }
  }

  // ─── Daily rotation ────────────────────────────────────────────────

  private _rotateIfNeeded(): void {
    if (!this.filePath) return;
    const today = new Date().toISOString().slice(0, 10); // YYYY-MM-DD
    if (this.currentDate && this.currentDate !== today) {
      // Rotate: rename current file to dated copy
      const rotated = this.filePath.replace(/\.jsonl$/, "") + "." + this.currentDate + ".jsonl";
      try { renameSync(this.filePath, rotated); } catch { /* ignore */ }
    }
    this.currentDate = today;
  }
}

// ─── Default log path ────────────────────────────────────────────────────────

export function defaultLogFile(): string {
  return join(homedir(), ".anyclaw", "logs", "nexusclaw.jsonl");
}

// ─── Root logger singleton ────────────────────────────────────────────────────

let _root: Logger | null = null;

export function getLogger(component = "nexusclaw"): Logger {
  if (!_root) {
    _root = new Logger("nexusclaw", { level: "info" });
  }
  return component === "nexusclaw" ? _root : _root.child(component.replace("nexusclaw:", ""));
}

export function configureLogger(opts: { level?: LogLevel; file?: string; json?: boolean }): void {
  if (!_root) _root = new Logger("nexusclaw", { level: opts.level ?? "info", jsonMode: opts.json });
  else _root.setLevel(opts.level ?? "info");
  if (opts.file) _root.setFile(opts.file);
}
