import { IncomingMessage, ServerResponse } from "http";
import { Config } from "../lib/config";
import { logger } from "../lib/logger";

interface RequestContext {
  requestId: string;
  metadata: Record<string, unknown>;
}

/**
 * Request Enricher: Add context, traces, and metadata
 */
export async function requestEnricher(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  config: Config
): Promise<void> {
  try {
    // Generate trace ID if not present
    const traceId =
      (req.headers["x-trace-id"] as string) || crypto.randomUUID();
    context.metadata.trace_id = traceId;
    req.headers["x-trace-id"] = traceId;

    // Add request metadata
    context.metadata.method = req.method;
    context.metadata.url = req.url;
    context.metadata.user_agent = req.headers["user-agent"];
    context.metadata.timestamp = new Date().toISOString();

    // Pass through via headers
    res.setHeader("X-Trace-ID", traceId);

    logger.debug(
      { request_id: context.requestId, trace_id: traceId },
      "Request enriched with trace context"
    );
  } catch (err) {
    logger.warn(
      {
        request_id: context.requestId,
        error: err instanceof Error ? err.message : String(err),
      },
      "Request enricher error (continuing)"
    );
  }
}
