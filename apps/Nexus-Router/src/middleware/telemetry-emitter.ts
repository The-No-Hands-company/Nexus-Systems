import type { IncomingMessage, ServerResponse } from "node:http";
import type { Config } from "../lib/config";
import { logger } from "../lib/logger";

interface RequestContext {
  requestId: string;
  startTime: number;
  metadata: Record<string, unknown>;
}

/**
 * Telemetry Emitter: Send logs, metrics, and traces to observability pipeline
 */
export async function telemetryEmitter(
  _req: IncomingMessage,
  _res: ServerResponse,
  context: RequestContext,
  config: Config,
  duration: number,
): Promise<void> {
  try {
    const _telemetryConfig = config.telemetry;
    if (!_telemetryConfig) {
      return;
    }

    // Emit log
    if (_telemetryConfig.logs?.enabled) {
      emitLog(_req, _res, context, duration, _telemetryConfig);
    }

    // Emit metrics
    if (_telemetryConfig.metrics?.enabled) {
      emitMetrics(_req, _res, context, duration, _telemetryConfig);
    }

    // Emit trace
    if (_telemetryConfig.tracing?.enabled) {
      emitTrace(_req, _res, context, duration, _telemetryConfig);
    }
  } catch (err) {
    logger.warn(
      {
        request_id: context.requestId,
        error: err instanceof Error ? err.message : String(err),
      },
      "Telemetry emitter error (continuing)",
    );
  }
}

function emitLog(
  _req: IncomingMessage,
  _res: ServerResponse,
  context: RequestContext,
  duration: number,
  _telemetryConfig: any,
): void {
  const logEntry = {
    timestamp: new Date().toISOString(),
    request_id: context.requestId,
    method: _req.method,
    url: _req.url,
    status_code: _res.statusCode,
    duration_ms: duration,
    ...context.metadata,
  };

  logger.info(logEntry, "API request");

  // TODO: Send to ElasticSearch / Datadog / etc
  if (_telemetryConfig.logs?.elasticsearch?.url) {
    // sendToElasticsearch(logEntry, _telemetryConfig.logs.elasticsearch.url);
  }
}

function emitMetrics(
  _req: IncomingMessage,
  _res: ServerResponse,
  context: RequestContext,
  duration: number,
  _telemetryConfig: any,
): void {
  // TODO: Emit Prometheus metrics
  // - nexus_router_requests_total
  // - nexus_router_request_duration_seconds
  // - nexus_router_auth_failures_total
  // - etc

  logger.debug(
    {
      request_id: context.requestId,
      metric_type: "request_latency",
      duration_ms: duration,
    },
    "Metric emitted",
  );
}

function emitTrace(
  _req: IncomingMessage,
  _res: ServerResponse,
  context: RequestContext,
  duration: number,
  _telemetryConfig: any,
): void {
  // TODO: Send trace to Jaeger / OpenTelemetry collector
  const traceId = context.metadata.trace_id as string;
  logger.debug(
    {
      request_id: context.requestId,
      trace_id: traceId,
      duration_ms: duration,
    },
    "Trace emitted",
  );
}
