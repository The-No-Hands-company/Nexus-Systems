import { buildSystemsApiRegistrationPayload } from "./contracts";
import { startNexusEdgeCloudRegistrationHeartbeat, requestGuardianThreatResponse } from "./cloud";
import {
  recordThreat,
  resolveThreat,
  getThreat,
  listThreats,
  recordAnomaly,
  confirmAnomaly,
  getAnomaly,
  listAnomalies,
  recordResponse,
  listResponses,
  getPolicy,
  getAIGuardrailProfile,
  upsertRoute,
  getRoute,
  listRoutes,
  setRouteEnabled,
  setRouteTrustState,
} from "./state";
import type { EdgeThreatSeverity, AnomalyType, EdgeResponseAction, EdgeIngressTarget, EdgeTrustState } from "./types";

const VALID_SEVERITIES: EdgeThreatSeverity[] = ["low", "medium", "high", "critical"];
const VALID_ANOMALY_TYPES: AnomalyType[] = ["behavioral", "performance", "security", "resource", "governance"];
const VALID_RESPONSES: EdgeResponseAction[] = ["log", "isolate", "throttle", "block", "alert"];
const VALID_INGRESS_TARGETS: EdgeIngressTarget[] = ["direct", "tunnel"];
const VALID_TRUST_STATES: EdgeTrustState[] = ["pending", "verified", "trusted", "quarantined", "revoked", "expired"];
const DENY_TRUST_STATES = new Set<EdgeTrustState>(["quarantined", "revoked", "expired"]);

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

function isValidSeverity(value: string): value is EdgeThreatSeverity {
  return (VALID_SEVERITIES as string[]).includes(value);
}

function isValidAnomalyType(value: string): value is AnomalyType {
  return (VALID_ANOMALY_TYPES as string[]).includes(value);
}

function isValidResponse(value: string): value is EdgeResponseAction {
  return (VALID_RESPONSES as string[]).includes(value);
}

function isValidIngressTarget(value: string): value is EdgeIngressTarget {
  return (VALID_INGRESS_TARGETS as string[]).includes(value);
}

function isValidTrustState(value: string): value is EdgeTrustState {
  return (VALID_TRUST_STATES as string[]).includes(value);
}

function normalizeHost(host: string): string {
  return host.trim().toLowerCase().replace(/:\d+$/, "");
}

function buildUpstreamUrl(baseTargetUrl: string, path: string, search: string): string {
  const base = new URL(baseTargetUrl);
  base.pathname = path;
  base.search = search;
  return base.toString();
}

async function proxyRequest(request: Request, upstreamUrl: string): Promise<Response> {
  const method = request.method.toUpperCase();
  const headers = new Headers(request.headers);
  headers.delete("host");
  headers.delete("content-length");
  const body = method === "GET" || method === "HEAD" ? undefined : await request.arrayBuffer();

  let upstreamResponse: Response;
  try {
    upstreamResponse = await fetch(upstreamUrl, {
      method,
      headers,
      body,
      redirect: "manual",
    });
  } catch (error) {
    return jsonResponse(
      {
        error: "Ingress upstream unavailable",
        upstreamUrl,
        detail: (error as Error).message,
      },
      { status: 502 },
    );
  }

  const proxyHeaders = new Headers(upstreamResponse.headers);
  proxyHeaders.delete("content-length");
  return new Response(upstreamResponse.body, {
    status: upstreamResponse.status,
    headers: proxyHeaders,
  });
}

export function createEdgeServer() {
  const port = Number(process.env.PORT || "4340");
  const baseUrl = process.env.NEXUS_EDGE_BASE_URL || `http://localhost:${port}`;
  const cloudIntegrationEnabled =
    (process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";

  const server = Bun.serve({
    port,
    fetch: async (request) => {
      const url = new URL(request.url);
      const path = url.pathname;

      if (request.method === "GET" && path === "/health") {
        return jsonResponse({
          status: "ok",
          service: "nexus-edge",
          timestamp: new Date().toISOString(),
        });
      }

      if (request.method === "GET" && path === "/api/v1/edge/readiness") {
        return jsonResponse({
          status: "ready",
          contracts: {
            threats: "/api/v1/edge/threats",
            anomalies: "/api/v1/edge/anomalies",
            responses: "/api/v1/edge/responses",
            policy: "/api/v1/edge/policy",
            guardrails: "/api/v1/edge/guardrails",
            detect: "/api/v1/edge/detect/:toolId",
            ingressRoutes: "/api/v1/edge/routes",
            ingressProxy: "host-header based routing for non-API paths",
          },
          cloudIntegration: {
            enabled: cloudIntegrationEnabled,
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
        });
      }

      if (request.method === "GET" && path === "/api/v1/edge/routes") {
        return jsonResponse({ status: "ok", routes: listRoutes() });
      }

      if (request.method === "POST" && path === "/api/v1/edge/routes") {
        const body = await parseBody(request);
        const host = typeof body.host === "string" ? normalizeHost(body.host) : "";
        const upstreamUrl = typeof body.upstreamUrl === "string" ? body.upstreamUrl.trim() : "";
        const targetTypeRaw = typeof body.targetType === "string" ? body.targetType : "direct";
        const trustStateRaw = typeof body.trustState === "string" ? body.trustState : "verified";

        if (!host || !upstreamUrl) {
          return jsonResponse({ error: "host and upstreamUrl are required" }, { status: 400 });
        }
        if (!isValidIngressTarget(targetTypeRaw)) {
          return jsonResponse(
            { error: `Invalid targetType '${targetTypeRaw}'. Valid values: ${VALID_INGRESS_TARGETS.join(", ")}` },
            { status: 400 },
          );
        }
        if (!isValidTrustState(trustStateRaw)) {
          return jsonResponse(
            { error: `Invalid trustState '${trustStateRaw}'. Valid values: ${VALID_TRUST_STATES.join(", ")}` },
            { status: 400 },
          );
        }

        try {
          new URL(upstreamUrl);
        } catch {
          return jsonResponse({ error: "upstreamUrl must be a valid URL" }, { status: 400 });
        }

        const route = upsertRoute(host, upstreamUrl, targetTypeRaw, trustStateRaw);
        return jsonResponse({ status: "ok", route }, { status: 201 });
      }

      const getRouteMatch = path.match(/^\/api\/v1\/edge\/routes\/([^/]+)$/);
      if (request.method === "GET" && getRouteMatch) {
        const [, encodedHost] = getRouteMatch;
        const route = getRoute(decodeURIComponent(encodedHost));
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      const enableRouteMatch = path.match(/^\/api\/v1\/edge\/routes\/([^/]+)\/(enable|disable)$/);
      if (request.method === "POST" && enableRouteMatch) {
        const [, encodedHost, action] = enableRouteMatch;
        const route = setRouteEnabled(decodeURIComponent(encodedHost), action === "enable");
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      const trustRouteMatch = path.match(/^\/api\/v1\/edge\/routes\/([^/]+)\/trust\/([^/]+)$/);
      if (request.method === "POST" && trustRouteMatch) {
        const [, encodedHost, rawTrustState] = trustRouteMatch;
        if (!isValidTrustState(rawTrustState)) {
          return jsonResponse(
            { error: `Invalid trustState '${rawTrustState}'. Valid values: ${VALID_TRUST_STATES.join(", ")}` },
            { status: 400 },
          );
        }
        const route = setRouteTrustState(decodeURIComponent(encodedHost), rawTrustState);
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      if (request.method === "GET" && path === "/api/v1/edge/threats") {
        return jsonResponse({ status: "ok", threats: listThreats() });
      }

      if (request.method === "GET" && path === "/api/v1/edge/anomalies") {
        return jsonResponse({ status: "ok", anomalies: listAnomalies() });
      }

      if (request.method === "GET" && path === "/api/v1/edge/responses") {
        return jsonResponse({ status: "ok", responses: listResponses() });
      }

      if (request.method === "GET" && path === "/api/v1/edge/policy") {
        return jsonResponse({ status: "ok", policy: getPolicy() });
      }

      if (request.method === "GET" && path === "/api/v1/edge/guardrails") {
        return jsonResponse({ status: "ok", aiGuardrailProfile: getAIGuardrailProfile() });
      }

      if (request.method === "GET" && path === "/api/v1/edge/contracts/registration") {
        return jsonResponse({ status: "ok", payload: buildSystemsApiRegistrationPayload(baseUrl) });
      }

      // GET /api/v1/edge/threats/:threatId
      const getThreatMatch = path.match(/^\/api\/v1\/edge\/threats\/([^/]+)$/);
      if (request.method === "GET" && getThreatMatch) {
        const [, threatId] = getThreatMatch;
        const threat = getThreat(decodeURIComponent(threatId));
        if (!threat) {
          return jsonResponse({ error: "Threat not found" }, { status: 404 });
        }
        return jsonResponse({ status: "ok", threat });
      }

      // POST /api/v1/edge/detect/:toolId — record threat or anomaly
      const detectMatch = path.match(/^\/api\/v1\/edge\/detect\/([^/]+)$/);
      if (request.method === "POST" && detectMatch) {
        const [, toolId] = detectMatch;
        const decodedToolId = decodeURIComponent(toolId);
        const body = await parseBody(request);

        const detectionType = typeof body.type === "string" ? body.type : "threat";

        if (detectionType === "threat") {
          const rawSeverity = typeof body.severity === "string" ? body.severity : "medium";
          if (!isValidSeverity(rawSeverity)) {
            return jsonResponse(
              { error: `Invalid severity '${rawSeverity}'. Valid values: ${VALID_SEVERITIES.join(", ")}` },
              { status: 400 },
            );
          }

          const description = typeof body.description === "string" ? body.description : "Detected threat";
          const threat = recordThreat(decodedToolId, rawSeverity, description);

          // Query Guardian for threat response recommendation
          const guardianRec = await requestGuardianThreatResponse(decodedToolId, description);
          if (isValidResponse(guardianRec.recommended as string)) {
            recordResponse(threat.threatId, guardianRec.recommended as EdgeResponseAction, "edge-guardian", guardianRec.reason);
          }

          return jsonResponse({ status: "ok", threat }, { status: 201 });
        } else if (detectionType === "anomaly") {
          const rawType = typeof body.anomalyType === "string" ? body.anomalyType : "behavioral";
          if (!isValidAnomalyType(rawType)) {
            return jsonResponse(
              { error: `Invalid anomalyType '${rawType}'. Valid values: ${VALID_ANOMALY_TYPES.join(", ")}` },
              { status: 400 },
            );
          }

          const confidence = typeof body.confidence === "number" ? body.confidence : 0.5;
          const details = typeof body.details === "string" ? body.details : undefined;
          const anomaly = recordAnomaly(decodedToolId, rawType, confidence, details);

          return jsonResponse({ status: "ok", anomaly }, { status: 201 });
        }

        return jsonResponse({ error: "type must be 'threat' or 'anomaly'" }, { status: 400 });
      }

      // POST /api/v1/edge/threats/:threatId/resolve
      const resolveMatch = path.match(/^\/api\/v1\/edge\/threats\/([^/]+)\/resolve$/);
      if (request.method === "POST" && resolveMatch) {
        const [, threatId] = resolveMatch;
        const body = await parseBody(request);
        const resolution = typeof body.resolution === "string" ? body.resolution : "Threat resolved";

        const updated = resolveThreat(decodeURIComponent(threatId), resolution);
        if (!updated) {
          return jsonResponse({ error: "Threat not found" }, { status: 404 });
        }

        return jsonResponse({ status: "ok", threat: updated });
      }

      // POST /api/v1/edge/anomalies/:anomalyId/confirm
      const confirmMatch = path.match(/^\/api\/v1\/edge\/anomalies\/([^/]+)\/confirm$/);
      if (request.method === "POST" && confirmMatch) {
        const [, anomalyId] = confirmMatch;
        const confirmed = confirmAnomaly(decodeURIComponent(anomalyId));
        if (!confirmed) {
          return jsonResponse({ error: "Anomaly not found" }, { status: 404 });
        }

        return jsonResponse({ status: "ok", anomaly: confirmed });
      }

      // Host-header ingress proxy for any non-API route.
      const requestHost = normalizeHost(request.headers.get("x-forwarded-host") || request.headers.get("host") || "");
      if (!path.startsWith("/api/v1/edge") && requestHost) {
        const route = getRoute(requestHost);
        if (!route) {
          return jsonResponse({ error: `No ingress route configured for host '${requestHost}'` }, { status: 404 });
        }
        if (!route.enabled) {
          return jsonResponse({ error: "Ingress route disabled" }, { status: 503 });
        }
        if (DENY_TRUST_STATES.has(route.trustState)) {
          return jsonResponse(
            { error: `Ingress denied due to trust state '${route.trustState}'` },
            { status: 403 },
          );
        }

        const upstreamUrl = buildUpstreamUrl(route.upstreamUrl, path, url.search);
        return await proxyRequest(request, upstreamUrl);
      }

      return jsonResponse({ error: "Not found" }, { status: 404 });
    },
  });

  console.log(`[nexus-edge] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusEdgeCloudRegistrationHeartbeat(baseUrl);

  return { server, stop: () => { stopHeartbeat(); server.stop(); } };
}
