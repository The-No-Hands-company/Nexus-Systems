import { IncomingMessage, ServerResponse } from "http";
import { Config } from "../lib/config";
import { logger } from "../lib/logger";

interface RequestContext {
  requestId: string;
  metadata: Record<string, unknown>;
}

/**
 * API Router: Query service registry and route to upstream
 */
export async function apiRouter(
  req: IncomingMessage,
  res: ServerResponse,
  context: RequestContext,
  config: Config
): Promise<void> {
  try {
    const url = req.url || "/";
    const routes = config.routes || {};

    // Find matching route
    let selectedRoute: any = null;
    let matchedPath = "";

    for (const [pattern, route] of Object.entries(routes)) {
      if (url.startsWith(pattern)) {
        selectedRoute = route;
        matchedPath = pattern;
        break;
      }
    }

    if (!selectedRoute) {
      // No route found, continue to next middleware
      return;
    }

    context.metadata.route = matchedPath;

    // Select upstream
    const upstreams = selectedRoute.upstreams || [];
    if (upstreams.length === 0) {
      logger.warn(
        { request_id: context.requestId, route: matchedPath },
        "No upstreams configured for route"
      );
      res.writeHead(503, { "Content-Type": "application/json" });
      res.end(
        JSON.stringify({
          error: "Service unavailable",
          request_id: context.requestId,
        })
      );
      return;
    }

    // Simple round-robin load balancing
    const upstream = selectUpstream(upstreams);
    context.metadata.upstream_name = upstream.name;

    logger.debug(
      {
        request_id: context.requestId,
        route: matchedPath,
        upstream: upstream.name,
      },
      "API router: selected upstream"
    );

    // TODO: Proxy request to upstream
    // For now, just forward to service registry
    if (matchedPath === "/api/v1/tools") {
      req.headers["x-routed-by"] = "nexus-router";
      // Continue to telemetry
      return;
    }
  } catch (err) {
    logger.error(
      {
        request_id: context.requestId,
        error: err instanceof Error ? err.message : String(err),
      },
      "API router error"
    );
  }
}

function selectUpstream(upstreams: any[]): any {
  // Simple round-robin
  const index = Math.floor(Math.random() * upstreams.length);
  return upstreams[index];
}
