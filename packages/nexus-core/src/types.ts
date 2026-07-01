// ─── Shared Types for Nexus Apps ───────────────────────────────────────────

export type LogLevel = "info" | "warn" | "error" | "debug";

export interface LogEntry {
  level: LogLevel;
  time: string;
  service: string;
  requestId?: string;
  message: string;
  [key: string]: unknown;
}

export type ValidationResult = string | null;

export interface SecurityHeaders {
  "x-content-type-options": "nosniff";
  "x-frame-options": "DENY";
  "cache-control": "no-store";
}

export interface ServerHandle {
  server: ReturnType<typeof import("node:http").createServer>;
  close: () => Promise<void>;
}

export interface ServerOptions {
  /** Name used in logs, health endpoint, and cloud registration. */
  serviceName?: string;
  /** Override Date.now for deterministic testing. */
  now?: () => string;
}
