import { IncomingMessage, ServerResponse } from "http";
import { Config } from "../lib/config";
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
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  config: Config,
  duration: number
): Promise<void> {
  try {
    const telemetryConfig = config.telemetry;
    if (!telemetryConfig) {
      return;
    }

    // Emit log
    if (telemetryConfig.logs?.enabled) {
      emitLog(req, res, context, duration, telemetryConfig);
    }

    // Emit metrics
    if (telemetryConfig.metrics?.enabled) {
      emitMetrics(req, res, context, duration, telemetryConfig);
    }

    // Emit trace
    if (telemetryConfig.tracing?.enabled) {
      emitTrace(req, res, context, duration, telemetryConfig);
    }
  } catch (err) {
    logger.warn(
      {
        request_id: context.requestId,
        error: err instanceof Error ? err.message : String(err),
      },
      "Telemetry emitter error (continuing)"
    );
  }
}

function emitLog(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  duration: number,
  telemetryConfig: any
): void {
  const logEntry = {
    timestamp: new Date().toISOString(),
    request_id: context.requestId,
    method: req.method,
    url: req.url,
    status_code: res.statusCode,
    duration_ms: duration,
    ...context.metadata,
  };

  logger.info(logEntry, "API request");

  // TODO: Send to ElasticSearch / Datadog / etc
  if (telemetryConfig.logs?.elasticsearch?.url) {
    // sendToElasticsearch(logEntry, telemetryConfig.logs.elasticsearch.url);
  }
}

function emitMetrics(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  duration: number,
  telemetryConfig: any
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
    "Metric emitted"
  );
}

function emitTrace(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  duration: number,
  telemetryConfig: any
): void {
  // TODO: Send trace to Jaeger / OpenTelemetry collector
  const traceId = context.metadata.trace_id as string;
  logger.debug(
    {
      request_id: context.requestId,
      trace_id: traceId,
      duration_ms: duration,
    },
    "Trace emitted"
  );
}
