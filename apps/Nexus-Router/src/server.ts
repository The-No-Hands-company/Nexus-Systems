import { IncomingMessage, ServerResponse } from "http";
import { logger } from "./lib/logger";
import { Config } from "./lib/config";
import { ipRouter } from "./middleware/ip-router";
import { authGate } from "./middleware/auth-gate";
import { requestEnricher } from "./middleware/request-enricher";
import { apiRouter } from "./middleware/api-router";
import { aiProviderRouter } from "./middleware/ai-provider-router";
import { telemetryEmitter } from "./middleware/telemetry-emitter";

interface RequestContext {
  requestId: string;
  startTime: number;
  clientIp: string;
  region?: string;
  userId?: string;
  orgId?: string;
  phantomDid?: string;
  traceId?: string;
  metadata: Record<string, unknown>;
}

export async function startRouter(
  req: IncomingMessage,
  res: ServerResponse,
  config: Config
): Promise<void> {
  const requestId = crypto.randomUUID();
  const startTime = Date.now();
  const clientIp = getClientIp(req);

  const context: RequestContext = {
    requestId,
    startTime,
    clientIp,
    metadata: {},
  };

  // Set response headers
  res.setHeader("X-Request-ID", requestId);
  res.setHeader("X-Router-Version", "0.1.0");

  // Pipeline: IP Router → Auth Gate → Request Enricher → API Router → AI Provider → Telemetry

  try {
    // 1. Health check
    if (req.url === "/health" && req.method === "GET") {
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(
        JSON.stringify({
          status: "healthy",
          uptime_sec: Math.floor(process.uptime()),
          request_id: requestId,
        })
      );
      return;
    }

    // 2. Router status
    if (req.url === "/api/v1/router/status" && req.method === "GET") {
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(
        JSON.stringify({
          version: "0.1.0",
          region: context.region || "unknown",
          routes_loaded: Object.keys(config.routes || {}).length,
          ai_providers_available: config.ai_providers?.routing_rules
            ?.map((r) => r.model)
            .filter((m, i, a) => a.indexOf(m) === i) || ["unknown"],
          telemetry_pipeline: "connected",
        })
      );
      return;
    }

    // 3. IP/Geo routing
    await ipRouter(req, res, context, config);
    if (res.writableEnded) return;

    // 4. Authentication gate
    await authGate(req, res, context, config);
    if (res.writableEnded) return;

    // 5. Request enrichment (add trace, metadata, etc)
    await requestEnricher(req, res, context, config);
    if (res.writableEnded) return;

    // 6. API route decision
    await apiRouter(req, res, context, config);
    if (res.writableEnded) return;

    // 7. AI provider selection (if applicable)
    await aiProviderRouter(req, res, context, config);
    if (res.writableEnded) return;

    // 8. If no middleware handled it, 404
    res.writeHead(404, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ error: "Not found", request_id: requestId }));
  } catch (err) {
    logger.error(
      {
        request_id: requestId,
        error: err instanceof Error ? err.message : String(err),
        stack: err instanceof Error ? err.stack : undefined,
      },
      "Router error"
    );
    res.writeHead(500, { "Content-Type": "application/json" });
    res.end(JSON.stringify({ error: "Internal server error", request_id: requestId }));
  } finally {
    // 9. Telemetry emission
    const duration = Date.now() - startTime;
    await telemetryEmitter(req, res, context, config, duration);
  }
}

function getClientIp(req: IncomingMessage): string {
  const forwarded = req.headers["x-forwarded-for"];
  if (typeof forwarded === "string") {
    return forwarded.split(",")[0].trim();
  }
  return req.socket.remoteAddress || "unknown";
}
