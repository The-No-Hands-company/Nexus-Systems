import { randomUUID } from "node:crypto";
import { type IncomingMessage, type ServerResponse, createServer } from "node:http";

type LogLevel = "info" | "warn" | "error";
interface LogEntry {
  level: LogLevel;
  time: string;
  service: string;
  requestId?: string;
  message: string;
  [key: string]: unknown;
}
type ValidationResult = string | null;

export interface Provider {
  name: string;
  available: boolean;
}
export interface HubState {
  routesHandled: number;
  nextRouteId: number;
}
export interface ServerOptions {
  serviceName?: string;
  now?: () => string;
  providers?: Provider[];
}
export interface ServerHandle {
  server: ReturnType<typeof createServer>;
  state: HubState;
  close: () => Promise<void>;
}

function log(
  level: LogLevel,
  service: string,
  message: string,
  ctx?: Record<string, unknown>,
): void {
  const entry: LogEntry = { level, time: new Date().toISOString(), service, message, ...ctx };
  process.stdout.write(JSON.stringify(entry) + "\n");
}
function createLogger(service: string) {
  return {
    info: (msg: string, ctx?: Record<string, unknown>) => log("info", service, msg, ctx),
    warn: (msg: string, ctx?: Record<string, unknown>) => log("warn", service, msg, ctx),
    error: (msg: string, ctx?: Record<string, unknown>) => log("error", service, msg, ctx),
  };
}

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
  return raw ? JSON.parse(raw) : {};
}

const VALID_PRIORITIES = new Set(["low", "normal", "high"]);

function validateRoutePayload(payload: unknown): ValidationResult {
  if (!payload || typeof payload !== "object") return "payload must be a JSON object";
  const p = payload as Record<string, unknown>;
  if (typeof p["task"] !== "string" || p["task"].trim() === "")
    return "task is required and must be a non-empty string";
  if (typeof p["tenant"] !== "string" || p["tenant"].trim() === "")
    return "tenant is required and must be a non-empty string";
  if (typeof p["priority"] !== "string" || !VALID_PRIORITIES.has(p["priority"]))
    return "priority is required and must be one of: low, normal, high";
  if (typeof p["timestamp"] !== "string" || Number.isNaN(Date.parse(p["timestamp"])))
    return "timestamp is required and must be an ISO 8601 string";
  return null;
}

export function createAiHubServer(options: ServerOptions = {}): ServerHandle {
  const serviceName = options.serviceName ?? "nexus-ai-hub";
  const getNow = options.now ?? (() => new Date().toISOString());
  const providers: Provider[] = options.providers ?? [{ name: "local-default", available: true }];
  const logger = createLogger(serviceName);
  const startedAt = Date.now();
  const state: HubState = { routesHandled: 0, nextRouteId: 1 };

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

      if (req.method === "GET" && req.url === "/api/v1/hub/providers/status") {
        sendJson(res, 200, { status: "ok", hub: { mode: "standalone", providers } }, requestId);
        done(200);
        return;
      }

      if (req.method === "POST" && req.url === "/api/v1/hub/route") {
        const payload = await readJsonBody(req);
        const err = validateRoutePayload(payload);
        if (err) {
          sendJson(res, 400, { error: err, code: "VALIDATION_ERROR", requestId }, requestId);
          logger.warn("validation error", { requestId, error: err });
          return;
        }

        state.routesHandled += 1;
        const routeId = `hub-${String(state.nextRouteId).padStart(6, "0")}`;
        state.nextRouteId += 1;
        const selectedProvider = providers.find((p) => p.available)?.name ?? "local-default";
        sendJson(res, 200, { status: "ok", provider: selectedProvider, routeId }, requestId);
        done(200);
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
