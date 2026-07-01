// ─── Structured JSON Logging ──────────────────────────────────────────────
// Every Nexus app logs to stdout as newline-delimited JSON.

import type { LogEntry, LogLevel } from "./types";

function log(
  level: LogLevel,
  service: string,
  message: string,
  ctx?: Record<string, unknown>,
): void {
  const entry: LogEntry = {
    level,
    time: new Date().toISOString(),
    service,
    message,
    ...ctx,
  };
  process.stdout.write(JSON.stringify(entry) + "\n");
}

export interface Logger {
  info: (msg: string, ctx?: Record<string, unknown>) => void;
  warn: (msg: string, ctx?: Record<string, unknown>) => void;
  error: (msg: string, ctx?: Record<string, unknown>) => void;
  debug: (msg: string, ctx?: Record<string, unknown>) => void;
}

export function createLogger(service: string): Logger {
  return {
    info: (msg: string, ctx?: Record<string, unknown>) => log("info", service, msg, ctx),
    warn: (msg: string, ctx?: Record<string, unknown>) => log("warn", service, msg, ctx),
    error: (msg: string, ctx?: Record<string, unknown>) => log("error", service, msg, ctx),
    debug: (msg: string, ctx?: Record<string, unknown>) => log("debug", service, msg, ctx),
  };
}
