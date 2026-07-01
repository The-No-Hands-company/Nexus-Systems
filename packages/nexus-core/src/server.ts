// ─── Nexus Server Factory ─────────────────────────────────────────────────
// Every Nexus app creates its HTTP server through this factory.
// It provides: health endpoint, request ID, error handling, structured logging,
// security headers, and graceful shutdown — so apps only define their routes.

import { randomUUID } from "node:crypto";
import { type IncomingMessage, type ServerResponse, createServer } from "node:http";
import { sendJson } from "./http";
import { createLogger } from "./logging";
import type { ServerHandle, ServerOptions } from "./types";

// ── Route Handler Type ────────────────────────────────────────────────────

export interface NexusRequestContext {
  req: IncomingMessage;
  res: ServerResponse;
  requestId: string;
  logger: ReturnType<typeof createLogger>;
  /** Send a JSON response with security headers and request ID. */
  json: (status: number, payload: unknown) => void;
  /** Send a JSON error response. */
  error: (status: number, error: string, code: string) => void;
  /** Parse the request body as JSON. */
  readJson: () => Promise<unknown>;
  /** Parse the request body as raw Buffer. */
  readRaw: () => Promise<Buffer>;
}

export type NexusRouteHandler = (ctx: NexusRequestContext) => Promise<void> | void;

// ── Server Factory ────────────────────────────────────────────────────────

export interface NexusServerHandle extends ServerHandle {
  /** The underlying Node.js HTTP server. */
  server: ReturnType<typeof createServer>;
}

export function createNexusServer(
  router: (ctx: NexusRequestContext) => Promise<void> | void,
  options: ServerOptions = {},
): NexusServerHandle {
  const serviceName = options.serviceName ?? "nexus-app";
  const getNow = options.now ?? (() => new Date().toISOString());
  const logger = createLogger(serviceName);
  const startedAt = Date.now();

  // Track open connections for graceful shutdown
  const connections = new Set<unknown>();

  const server = createServer(async (req: IncomingMessage, res: ServerResponse) => {
    const requestId = randomUUID();
    const start = Date.now();

    // Wrap res.end (not needed for tracking, kept for future extension)
    const originalEnd = res.end.bind(res);
    res.end = (...args: unknown[]) => {
      // @ts-expect-error: dynamic override
      return originalEnd(...args);
    };

    const ctx: NexusRequestContext = {
      req,
      res,
      requestId,
      logger,
      json: (status: number, payload: unknown) => {
        sendJson(res, status, payload, requestId);
        logger.info("request", {
          requestId,
          method: req.method,
          path: req.url,
          status,
          durationMs: Date.now() - start,
        });
      },
      error: (status: number, error: string, code: string) => {
        sendJson(res, status, { error, code, requestId }, requestId);
        logger.warn("request error", {
          requestId,
          method: req.method,
          path: req.url,
          status,
          error,
          code,
          durationMs: Date.now() - start,
        });
      },
      readJson: async () => {
        const chunks: Buffer[] = [];
        for await (const chunk of req) chunks.push(chunk as Buffer);
        const raw = Buffer.concat(chunks).toString("utf8");
        return raw ? JSON.parse(raw) : {};
      },
      readRaw: async () => {
        const chunks: Buffer[] = [];
        for await (const chunk of req) chunks.push(chunk as Buffer);
        return Buffer.concat(chunks);
      },
    };

    try {
      // ── GET /health (built-in) ─────────────────────────────────────
      if (req.method === "GET" && req.url === "/health") {
        ctx.json(200, {
          service: serviceName,
          status: "ok",
          version: "0.1.0",
          uptimeSeconds: Math.floor((Date.now() - startedAt) / 1000),
          timestamp: getNow(),
        });
        return;
      }

      // Delegate to the app's router
      await router(ctx);
    } catch (err) {
      const message = err instanceof Error ? err.message : String(err);
      logger.error("unhandled error", {
        requestId,
        method: req.method,
        path: req.url,
        error: message,
        durationMs: Date.now() - start,
      });

      if (!res.headersSent) {
        ctx.error(500, "Internal server error", "INTERNAL_ERROR");
      }
    }
  });

  // Track connections for graceful shutdown
  server.on("connection", (socket: unknown) => {
    connections.add(socket);
    (socket as { on(event: string, cb: () => void): void }).on("close", () => {
      connections.delete(socket);
    });
  });

  async function close(): Promise<void> {
    logger.info("closing server", { connections: connections.size });

    // Destroy all open connections
    for (const socket of connections) {
      (socket as { destroy(): void }).destroy();
    }
    connections.clear();

    return new Promise<void>((resolve, reject) => {
      server.close((err) => {
        if (err) {
          logger.error("server close error", { error: err.message });
          reject(err);
        } else {
          logger.info("server closed");
          resolve();
        }
      });
    });
  }

  return { server, close };
}
