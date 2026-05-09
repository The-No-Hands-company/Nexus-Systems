import { randomUUID } from "node:crypto";
import { type IncomingMessage, type ServerResponse, createServer } from "node:http";

// ── Types ─────────────────────────────────────────────────────────────────────

type LogLevel = "info" | "warn" | "error";

interface LogEntry {
  level: LogLevel;
  time: string;
  service: string;
  requestId?: string;
  message: string;
  [key: string]: unknown;
}

export interface MonitorState {
  eventsReceived: number;
  heartbeatsReceived: number;
  nextId: number;
}

export interface ServerOptions {
  serviceName?: string;
  now?: () => string;
}

export interface ServerHandle {
  server: ReturnType<typeof createServer>;
  state: MonitorState;
  close: () => Promise<void>;
}

type ValidationResult = string | null;

// ── Logger ────────────────────────────────────────────────────────────────────

function log(
  level: LogLevel,
  service: string,
  message: string,
  context?: Record<string, unknown>,
): void {
  const entry: LogEntry = { level, time: new Date().toISOString(), service, message, ...context };
  process.stdout.write(JSON.stringify(entry) + "\n");
}

function createLogger(service: string) {
  return {
    info: (msg: string, ctx?: Record<string, unknown>) => log("info", service, msg, ctx),
    warn: (msg: string, ctx?: Record<string, unknown>) => log("warn", service, msg, ctx),
    error: (msg: string, ctx?: Record<string, unknown>) => log("error", service, msg, ctx),
  };
}

// ── HTTP helpers ──────────────────────────────────────────────────────────────

const SECURITY_HEADERS = {
  "x-content-type-options": "nosniff",
  "x-frame-options": "DENY",
  "cache-control": "no-store",
} as const;

function sendJson(res: ServerResponse, status: number, payload: unknown, requestId: string): void {
  const body = JSON.stringify(payload);
  res.writeHead(status, {
    "content-type": "application/json; charset=utf-8",
    "content-length": Buffer.byteLength(body),
    "x-request-id": requestId,
    ...SECURITY_HEADERS,
  });
  res.end(body);
}

async function readJsonBody(req: IncomingMessage): Promise<unknown> {
  const chunks: Buffer[] = [];
  for await (const chunk of req) chunks.push(chunk as Buffer);
  const raw = Buffer.concat(chunks).toString("utf8");
  if (!raw) return {};
  return JSON.parse(raw);
}

// ── Validation ────────────────────────────────────────────────────────────────

const VALID_KINDS = new Set(["heartbeat", "alert", "event"]);

function validateIngestPayload(payload: unknown): ValidationResult {
  if (!payload || typeof payload !== "object") return "payload must be a JSON object";
  const p = payload as Record<string, unknown>;

  if (typeof p["source"] !== "string" || p["source"].trim() === "")
    return "source is required and must be a non-empty string";

  if (typeof p["kind"] !== "string" || !VALID_KINDS.has(p["kind"]))
    return "kind is required and must be one of: heartbeat, alert, event";

  if (typeof p["timestamp"] !== "string" || Number.isNaN(Date.parse(p["timestamp"])))
    return "timestamp is required and must be an ISO 8601 string";

  if (
    p["kind"] === "heartbeat" &&
    (typeof p["state"] !== "string" || (p["state"] as string).trim() === "")
  )
    return "state is required for heartbeat kind";

  return null;
}

// ── Server factory ────────────────────────────────────────────────────────────

export function createMonitorServer(options: ServerOptions = {}): ServerHandle {
  const serviceName = options.serviceName ?? "nexus-monitor";
  const getNow = options.now ?? (() => new Date().toISOString());
  const logger = createLogger(serviceName);
  const startedAt = Date.now();

  const state: MonitorState = { eventsReceived: 0, heartbeatsReceived: 0, nextId: 1 };

  const server = createServer(async (req: IncomingMessage, res: ServerResponse) => {
    const requestId = randomUUID();
    const start = Date.now();

    const done = (status: number) =>
      logger.info("request", {
        requestId,
        method: req.method,
        path: req.url,
        status,
        durationMs: Date.now() - start,
      });

    try {
      if (req.method === "GET" && req.url === "/health") {
        sendJson(
          res,
          200,
          {
            service: serviceName,
            status: "ok",
            version: "0.1.0",
            uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
            timestamp: getNow(),
          },
          requestId,
        );
        done(200);
        return;
      }

      if (req.method === "GET" && req.url === "/api/v1/monitor/status") {
        sendJson(
          res,
          200,
          {
            status: "ok",
            monitor: {
              mode: "standalone",
              eventsReceived: state.eventsReceived,
              heartbeatsReceived: state.heartbeatsReceived,
            },
          },
          requestId,
        );
        done(200);
        return;
      }

      if (req.method === "POST" && req.url === "/api/v1/monitor/ingest") {
        const payload = await readJsonBody(req);
        const err = validateIngestPayload(payload);
        if (err) {
          sendJson(res, 400, { error: err, code: "VALIDATION_ERROR", requestId }, requestId);
          logger.warn("validation error", { requestId, error: err });
          return;
        }

        const p = payload as Record<string, unknown>;
        state.eventsReceived += 1;
        if (p["kind"] === "heartbeat") state.heartbeatsReceived += 1;

        const id = `evt-${String(state.nextId).padStart(6, "0")}`;
        state.nextId += 1;

        sendJson(res, 202, { status: "accepted", id }, requestId);
        done(202);
        return;
      }

      sendJson(res, 404, { error: "not found", code: "NOT_FOUND", requestId }, requestId);
      done(404);
    } catch (error) {
      const message = error instanceof Error ? error.message : String(error);
      sendJson(
        res,
        500,
        { error: "internal server error", code: "INTERNAL_ERROR", requestId },
        requestId,
      );
      logger.error("unhandled error", { requestId, error: message });
    }
  });

  return {
    server,
    state,
    close: () =>
      new Promise<void>((resolve, reject) => server.close((e) => (e ? reject(e) : resolve()))),
  };
}
