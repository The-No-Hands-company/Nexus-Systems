import { buildSystemsApiRegistrationPayload } from "./contracts";
import {
  startNexusTunnelCloudRegistrationHeartbeat,
  startNexusTunnelCloudReconciliationLoop,
  requestGuardianApprovalForExposure,
} from "./cloud";
import {
  registerRoute,
  getRoute,
  setRouteEnabled,
  listRoutes,
  recordExposure,
  getExposure,
  listExposures,
  recordDecision,
  listDecisions,
  getPolicy,
} from "./state";

function jsonResponse(payload: unknown, init?: ResponseInit): Response {
  return new Response(JSON.stringify(payload, null, 2), {
    ...init,
    headers: {
      "content-type": "application/json; charset=utf-8",
      ...(init?.headers || {}),
    },
  });
}

async function parseBody(request: Request): Promise<Record<string, unknown>> {
  try {
    const parsed = await request.json();
    return typeof parsed === "object" && parsed !== null ? (parsed as Record<string, unknown>) : {};
  } catch {
    return {};
  }
}

function buildUpstreamUrl(baseTargetUrl: string, suffixPath: string, search: string): string {
  const base = new URL(baseTargetUrl);
  const normalizedBasePath = base.pathname.endsWith("/") ? base.pathname.slice(0, -1) : base.pathname;
  const normalizedSuffixPath = suffixPath.startsWith("/") ? suffixPath : `/${suffixPath}`;
  base.pathname = `${normalizedBasePath}${normalizedSuffixPath}`;
  base.search = search;
  return base.toString();
}

async function proxyToRoute(request: Request, upstreamUrl: string): Promise<Response> {
  const headers = new Headers(request.headers);
  headers.delete("host");
  headers.delete("content-length");

  const method = request.method.toUpperCase();
  const hasBody = method !== "GET" && method !== "HEAD";
  const body = hasBody ? await request.arrayBuffer() : undefined;

  let upstream: Response;
  try {
    upstream = await fetch(upstreamUrl, {
      method,
      headers,
      body,
      redirect: "manual",
    });
  } catch (error) {
    return jsonResponse(
      {
        error: "Upstream route unavailable",
        upstreamUrl,
        detail: (error as Error).message,
      },
      { status: 502 },
    );
  }

  const responseHeaders = new Headers(upstream.headers);
  responseHeaders.delete("content-length");

  return new Response(upstream.body, {
    status: upstream.status,
    headers: responseHeaders,
  });
}

export function createTunnelServer() {
  const port = Number(process.env.PORT || "4330");
  const baseUrl = process.env.NEXUS_TUNNEL_BASE_URL || `http://localhost:${port}`;
  const cloudIntegrationEnabled =
    (process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";

  const server = Bun.serve({
    port,
    fetch: async (request) => {
      const url = new URL(request.url);
      const path = url.pathname;

      if (request.method === "GET" && path === "/health") {
        return jsonResponse({
          status: "ok",
          service: "nexus-tunnel",
          timestamp: new Date().toISOString(),
        });
      }

      if (request.method === "GET" && path === "/api/v1/tunnel/readiness") {
        return jsonResponse({
          status: "ready",
          contracts: {
            routes: "/api/v1/tunnel/routes",
            routeEnable: "/api/v1/tunnel/routes/:toolId/enable",
            routeDisable: "/api/v1/tunnel/routes/:toolId/disable",
            exposures: "/api/v1/tunnel/exposures",
            policy: "/api/v1/tunnel/policy",
            decide: "/api/v1/tunnel/decide/:toolId",
            expose: "/api/v1/tunnel/expose/:toolId",
            proxy: "/t/:toolId/*",
          },
          cloudIntegration: {
            enabled: cloudIntegrationEnabled,
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
        });
      }

      if (request.method === "GET" && path === "/api/v1/tunnel/routes") {
        return jsonResponse({ status: "ok", routes: listRoutes() });
      }

      if (request.method === "GET" && path === "/api/v1/tunnel/exposures") {
        return jsonResponse({ status: "ok", exposures: listExposures() });
      }

      if (request.method === "GET" && path === "/api/v1/tunnel/decisions") {
        return jsonResponse({ status: "ok", decisions: listDecisions() });
      }

      if (request.method === "GET" && path === "/api/v1/tunnel/policy") {
        return jsonResponse({ status: "ok", policy: getPolicy() });
      }

      if (request.method === "GET" && path === "/api/v1/tunnel/contracts/registration") {
        return jsonResponse({ status: "ok", payload: buildSystemsApiRegistrationPayload(baseUrl) });
      }

      // GET /api/v1/tunnel/routes/:toolId
      const getRouteMatch = path.match(/^\/api\/v1\/tunnel\/routes\/([^/]+)$/);
      if (request.method === "GET" && getRouteMatch) {
        const [, toolId] = getRouteMatch;
        const route = getRoute(decodeURIComponent(toolId));
        if (!route) {
          return jsonResponse({ error: "Route not found" }, { status: 404 });
        }
        return jsonResponse({ status: "ok", route });
      }

      // POST /api/v1/tunnel/routes
      if (request.method === "POST" && path === "/api/v1/tunnel/routes") {
        const body = await parseBody(request);
        const toolId = typeof body.toolId === "string" ? body.toolId : "";
        const targetUrl = typeof body.targetUrl === "string" ? body.targetUrl : "";
        const exposureMode =
          typeof body.exposureMode === "string" && ["public", "internal", "protected"].includes(body.exposureMode)
            ? (body.exposureMode as "public" | "internal" | "protected")
            : "internal";

        if (!toolId || !targetUrl) {
          return jsonResponse({ error: "toolId and targetUrl are required" }, { status: 400 });
        }

        const route = registerRoute(toolId, targetUrl, exposureMode);
        return jsonResponse({ status: "ok", route }, { status: 201 });
      }

      // POST /api/v1/tunnel/routes/:toolId/enable
      const enableRouteMatch = path.match(/^\/api\/v1\/tunnel\/routes\/([^/]+)\/enable$/);
      if (request.method === "POST" && enableRouteMatch) {
        const [, toolId] = enableRouteMatch;
        const route = setRouteEnabled(decodeURIComponent(toolId), true);
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      // POST /api/v1/tunnel/routes/:toolId/disable
      const disableRouteMatch = path.match(/^\/api\/v1\/tunnel\/routes\/([^/]+)\/disable$/);
      if (request.method === "POST" && disableRouteMatch) {
        const [, toolId] = disableRouteMatch;
        const route = setRouteEnabled(decodeURIComponent(toolId), false);
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      // GET /api/v1/tunnel/decide/:toolId — check Guardian approval status
      const decideMatch = path.match(/^\/api\/v1\/tunnel\/decide\/([^/]+)$/);
      if (request.method === "GET" && decideMatch) {
        const [, toolId] = decideMatch;
        const decodedToolId = decodeURIComponent(toolId);
        const exposure = getExposure(decodedToolId);

        if (!exposure) {
          return jsonResponse({ error: "No exposure decision found for tool" }, { status: 404 });
        }

        return jsonResponse({
          status: "ok",
          toolId: decodedToolId,
          approved: exposure.guardianApproved,
          exposureMode: exposure.exposureMode,
          reason: exposure.guardianApproved ? exposure.approvalReason : exposure.denialReason,
        });
      }

      // POST /api/v1/tunnel/decide/:toolId — request exposure with Guardian check
      const exposureMatch = path.match(/^\/api\/v1\/tunnel\/decide\/([^/]+)$/);
      if (request.method === "POST" && exposureMatch) {
        const [, toolId] = exposureMatch;
        const decodedToolId = decodeURIComponent(toolId);
        const body = await parseBody(request);

        const exposureMode =
          typeof body.exposureMode === "string" && ["public", "internal", "protected"].includes(body.exposureMode)
            ? (body.exposureMode as "public" | "internal" | "protected")
            : "internal";

        // Query Guardian for approval (only required for public exposure)
        const requireApproval = exposureMode === "public";
        let guardianApproved = !requireApproval; // Internal/protected don't need Guardian approval
        let reason = "";

        if (requireApproval) {
          const guardianResult = await requestGuardianApprovalForExposure(decodedToolId, exposureMode);
          guardianApproved = guardianResult.approved;
          reason = guardianResult.reason || "Guardian decision pending";
        }

        const exposure = recordExposure(decodedToolId, exposureMode, guardianApproved, reason);
        recordDecision(
          decodedToolId,
          exposureMode,
          guardianApproved,
          "tunnel",
          reason || `Exposure ${exposureMode} decided`,
        );

        return jsonResponse(
          {
            status: guardianApproved ? "approved" : "denied",
            exposure,
          },
          { status: guardianApproved ? 200 : 403 },
        );
      }

      // POST /api/v1/tunnel/expose/:toolId (legacy alias for decide)
      const legacyMatch = path.match(/^\/api\/v1\/tunnel\/expose\/([^/]+)$/);
      if (request.method === "POST" && legacyMatch) {
        const [, toolId] = legacyMatch;
        const decodedToolId = decodeURIComponent(toolId);
        const body = await parseBody(request);

        const exposureMode =
          typeof body.exposureMode === "string" && ["public", "internal", "protected"].includes(body.exposureMode)
            ? (body.exposureMode as "public" | "internal" | "protected")
            : "internal";

        const requireApproval = exposureMode === "public";
        let guardianApproved = !requireApproval;
        let reason = "";

        if (requireApproval) {
          const guardianResult = await requestGuardianApprovalForExposure(decodedToolId, exposureMode);
          guardianApproved = guardianResult.approved;
          reason = guardianResult.reason || "Guardian decision pending";
        }

        const exposure = recordExposure(decodedToolId, exposureMode, guardianApproved, reason);
        recordDecision(
          decodedToolId,
          exposureMode,
          guardianApproved,
          "tunnel",
          reason || `Exposure ${exposureMode} decided`,
        );

        return jsonResponse(
          {
            status: guardianApproved ? "approved" : "denied",
            exposure,
          },
          { status: guardianApproved ? 200 : 403 },
        );
      }

      // Proxy any method through an approved/active route:
      // /t/:toolId or /t/:toolId/*
      const proxyMatch = path.match(/^\/t\/([^/]+)(?:\/(.*))?$/);
      if (proxyMatch) {
        const [, rawToolId, rawSuffix] = proxyMatch;
        const toolId = decodeURIComponent(rawToolId);
        const route = getRoute(toolId);

        if (!route) {
          return jsonResponse({ error: "Route not found" }, { status: 404 });
        }
        if (!route.enabled) {
          return jsonResponse({ error: "Route is disabled" }, { status: 503 });
        }

        if (route.exposureMode === "public") {
          const exposure = getExposure(toolId);
          if (!exposure?.guardianApproved) {
            return jsonResponse({ error: "Public route requires Guardian approval" }, { status: 403 });
          }
        }

        const suffixPath = rawSuffix ? `/${rawSuffix}` : "/";
        const upstreamUrl = buildUpstreamUrl(route.targetUrl, suffixPath, url.search);
        return await proxyToRoute(request, upstreamUrl);
      }

      return jsonResponse({ error: "Not found" }, { status: 404 });
    },
  });

  console.log(`[nexus-tunnel] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusTunnelCloudRegistrationHeartbeat(baseUrl);
  const stopReconciliation = startNexusTunnelCloudReconciliationLoop();

  return { server, stop: () => { stopHeartbeat(); stopReconciliation(); server.stop(); } };
}
