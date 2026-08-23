import { buildSystemsApiRegistrationPayload } from "./contracts";
import {
  startNexusEdgeCloudRegistrationHeartbeat,
  requestGuardianThreatResponse,
  registerRouteWithTunnel,
  pushHealthToGuardian,
} from "./cloud";
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
  updateRouteHealth,
} from "./state";
import {
  upsertRateLimitRule,
  listRateLimitRules,
  deleteRateLimitRule,
  checkAndRecordRateLimit,
} from "./edge-rate-limiter";
import { recordMetric, getMetrics, getMetricsForHost, getTrafficSummary } from "./traffic-metrics";
import type { EdgeThreatSeverity, AnomalyType, EdgeResponseAction, EdgeIngressTarget, EdgeTrustState } from "./types";
import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";

const VALID_SEVERITIES: EdgeThreatSeverity[] = ["low", "medium", "high", "critical"];
const VALID_ANOMALY_TYPES: AnomalyType[] = ["behavioral", "performance", "security", "resource", "governance"];
const VALID_RESPONSES: EdgeResponseAction[] = ["log", "isolate", "throttle", "block", "alert"];
const VALID_INGRESS_TARGETS: EdgeIngressTarget[] = ["direct", "tunnel"];
const VALID_TRUST_STATES: EdgeTrustState[] = ["pending", "verified", "trusted", "quarantined", "revoked", "expired"];
const DENY_TRUST_STATES = new Set<EdgeTrustState>(["quarantined", "revoked", "expired"]);

function jsonResponse(payload: unknown, init?: ResponseInit): Response {
  return new Response(JSON.stringify(payload, null, 2), {
    ...init,
    headers: { "content-type": "application/json; charset=utf-8" },
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

function isValidSeverity(v: string): v is EdgeThreatSeverity { return (VALID_SEVERITIES as string[]).includes(v); }
function isValidAnomalyType(v: string): v is AnomalyType { return (VALID_ANOMALY_TYPES as string[]).includes(v); }
function isValidResponse(v: string): v is EdgeResponseAction { return (VALID_RESPONSES as string[]).includes(v); }
function isValidIngressTarget(v: string): v is EdgeIngressTarget { return (VALID_INGRESS_TARGETS as string[]).includes(v); }
function isValidTrustState(v: string): v is EdgeTrustState { return (VALID_TRUST_STATES as string[]).includes(v); }

function normalizeHost(host: string): string {
  return host.trim().toLowerCase().replace(/:\d+$/, "");
}

function getClientIp(request: Request): string {
  return request.headers.get("x-forwarded-for")?.split(",")[0]?.trim() ||
    request.headers.get("x-real-ip") || "127.0.0.1";
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
    upstreamResponse = await fetch(upstreamUrl, { method, headers, body, redirect: "manual" });
  } catch (error) {
    return jsonResponse({ error: "Ingress upstream unavailable", upstreamUrl, detail: (error as Error).message }, { status: 502 });
  }

  const proxyHeaders = new Headers(upstreamResponse.headers);
  proxyHeaders.delete("content-length");
  return new Response(upstreamResponse.body, { status: upstreamResponse.status, headers: proxyHeaders });
}

export function createEdgeServer() {
  const port = Number(process.env.PORT || "4340");
  const baseUrl = process.env.NEXUS_EDGE_BASE_URL || `http://localhost:${port}`;
  const cloudIntegrationEnabled =
    (process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";

  const policy = getPolicy();

  if (policy.healthCheckEnabled) {
    setInterval(async () => {
      for (const route of listRoutes()) {
        if (!route.enabled) continue;
        try {
          const start = Date.now();
          const res = await fetch(`${route.upstreamUrl.replace(/\/$/, "")}/health`, {
            signal: AbortSignal.timeout(5000),
          });
          const latency = Date.now() - start;
          updateRouteHealth(route.host, res.ok ? "healthy" : "degraded");
          pushHealthToGuardian(route).catch(() => {});
        } catch {
          updateRouteHealth(route.host, "unreachable");
        }
      }
    }, policy.healthCheckIntervalMs);
  }

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-edge",
  name: "Nexus Edge",
  description: "Gateway orchestration with threat detection",
  port: 4340,
  capabilities: ["threat-detection","edge-enforcement","ai-guardrails","anomaly-detection","auto-response","rate-limiting","traffic-metrics"],
}));

const stopNexusHeartbeat = nexusClient.startCloudHeartbeat();
const stopNexusMonitor = nexusClient.startMonitorHeartbeat();
const server = Bun.serve({
    port,
    fetch: async (request) => {
      const url = new URL(request.url);
      const path = url.pathname;
      const startTime = Date.now();

      if (request.method === "GET" && path === "/health") {
        return jsonResponse({ status: "ok", service: "nexus-edge", timestamp: new Date().toISOString() });
      }

      if (request.method === "GET" && path === "/api/v1/edge/readiness") {
        return jsonResponse({
          status: "ready",
          routes: listRoutes().length,
          threats: listThreats().length,
          rateLimitRules: listRateLimitRules().length,
          cloudIntegration: { enabled: cloudIntegrationEnabled },
        });
      }

      // ── Routes ──
      if (request.method === "GET" && path === "/api/v1/edge/routes") {
        return jsonResponse({ status: "ok", routes: listRoutes() });
      }

      if (request.method === "POST" && path === "/api/v1/edge/routes") {
        const body = await parseBody(request);
        const host = typeof body.host === "string" ? normalizeHost(body.host) : "";
        const upstreamUrl = typeof body.upstreamUrl === "string" ? body.upstreamUrl.trim() : "";
        const targetTypeRaw = typeof body.targetType === "string" ? body.targetType : "direct";
        const trustStateRaw = typeof body.trustState === "string" ? body.trustState : "verified";

        if (!host || !upstreamUrl) return jsonResponse({ error: "host and upstreamUrl are required" }, { status: 400 });
        if (!isValidIngressTarget(targetTypeRaw)) return jsonResponse({ error: `Invalid targetType '${targetTypeRaw}'` }, { status: 400 });
        if (!isValidTrustState(trustStateRaw)) return jsonResponse({ error: `Invalid trustState '${trustStateRaw}'` }, { status: 400 });
        try { new URL(upstreamUrl); } catch { return jsonResponse({ error: "upstreamUrl must be a valid URL" }, { status: 400 }); }

        const route = upsertRoute(host, upstreamUrl, targetTypeRaw, trustStateRaw);
        registerRouteWithTunnel(host, upstreamUrl).catch(() => {});
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
        if (!isValidTrustState(rawTrustState)) return jsonResponse({ error: `Invalid trustState '${rawTrustState}'` }, { status: 400 });
        const route = setRouteTrustState(decodeURIComponent(encodedHost), rawTrustState);
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      // ── Threats ──
      if (request.method === "GET" && path === "/api/v1/edge/threats") {
        return jsonResponse({ status: "ok", threats: listThreats() });
      }

      const getThreatMatch = path.match(/^\/api\/v1\/edge\/threats\/([^/]+)$/);
      if (request.method === "GET" && getThreatMatch) {
        const [, threatId] = getThreatMatch;
        const threat = getThreat(decodeURIComponent(threatId));
        if (!threat) return jsonResponse({ error: "Threat not found" }, { status: 404 });
        return jsonResponse({ status: "ok", threat });
      }

      const resolveMatch = path.match(/^\/api\/v1\/edge\/threats\/([^/]+)\/resolve$/);
      if (request.method === "POST" && resolveMatch) {
        const [, threatId] = resolveMatch;
        const body = await parseBody(request);
        const resolution = typeof body.resolution === "string" ? body.resolution : "Threat resolved";
        const updated = resolveThreat(decodeURIComponent(threatId), resolution);
        if (!updated) return jsonResponse({ error: "Threat not found" }, { status: 404 });
        return jsonResponse({ status: "ok", threat: updated });
      }

      // ── Detect ──
      const detectMatch = path.match(/^\/api\/v1\/edge\/detect\/([^/]+)$/);
      if (request.method === "POST" && detectMatch) {
        const [, toolId] = detectMatch;
        const decodedToolId = decodeURIComponent(toolId);
        const body = await parseBody(request);
        const detectionType = typeof body.type === "string" ? body.type : "threat";

        if (detectionType === "threat") {
          const severity = typeof body.severity === "string" && isValidSeverity(body.severity) ? body.severity : "medium";
          const description = typeof body.description === "string" ? body.description : "Detected threat";
          const threat = recordThreat(decodedToolId, severity, description);

          const guardianRec = await requestGuardianThreatResponse(decodedToolId, severity, description);
          if (isValidResponse(guardianRec.recommended)) {
            recordResponse(threat.threatId, guardianRec.recommended as EdgeResponseAction, "edge-guardian", guardianRec.reason);
          }
          return jsonResponse({ status: "ok", threat, guardianResponse: guardianRec }, { status: 201 });
        }

        if (detectionType === "anomaly") {
          const anomalyType = typeof body.anomalyType === "string" && isValidAnomalyType(body.anomalyType) ? body.anomalyType : "behavioral";
          const confidence = typeof body.confidence === "number" ? body.confidence : 0.5;
          const details = typeof body.details === "string" ? body.details : undefined;
          const anomaly = recordAnomaly(decodedToolId, anomalyType, confidence, details);
          return jsonResponse({ status: "ok", anomaly }, { status: 201 });
        }

        return jsonResponse({ error: "type must be 'threat' or 'anomaly'" }, { status: 400 });
      }

      // ── Anomalies ──
      if (request.method === "GET" && path === "/api/v1/edge/anomalies") {
        return jsonResponse({ status: "ok", anomalies: listAnomalies() });
      }

      const confirmMatch = path.match(/^\/api\/v1\/edge\/anomalies\/([^/]+)\/confirm$/);
      if (request.method === "POST" && confirmMatch) {
        const [, anomalyId] = confirmMatch;
        const confirmed = confirmAnomaly(decodeURIComponent(anomalyId));
        if (!confirmed) return jsonResponse({ error: "Anomaly not found" }, { status: 404 });
        return jsonResponse({ status: "ok", anomaly: confirmed });
      }

      // ── Responses ──
      if (request.method === "GET" && path === "/api/v1/edge/responses") {
        return jsonResponse({ status: "ok", responses: listResponses() });
      }

      // ── Policy / Guardrails ──
      if (request.method === "GET" && path === "/api/v1/edge/policy") {
        return jsonResponse({ status: "ok", policy: getPolicy() });
      }
      if (request.method === "GET" && path === "/api/v1/edge/guardrails") {
        return jsonResponse({ status: "ok", aiGuardrailProfile: getAIGuardrailProfile() });
      }
      if (request.method === "GET" && path === "/api/v1/edge/contracts/registration") {
        return jsonResponse({ status: "ok", payload: buildSystemsApiRegistrationPayload(baseUrl) });
      }

      // ── Rate Limits ──
      if (request.method === "GET" && path === "/api/v1/edge/rate-limits") {
        return jsonResponse({ status: "ok", rateLimits: listRateLimitRules() });
      }

      if (request.method === "POST" && path === "/api/v1/edge/rate-limits") {
        const body = await parseBody(request);
        const host = typeof body.host === "string" ? normalizeHost(body.host) : "";
        if (!host) return jsonResponse({ error: "host is required" }, { status: 400 });

        const rule = upsertRateLimitRule({
          id: typeof body.id === "string" ? body.id : undefined,
          host,
          maxRequestsPerSecond: typeof body.maxRequestsPerSecond === "number" ? body.maxRequestsPerSecond : 10,
          description: typeof body.description === "string" ? body.description : "",
        });
        return jsonResponse({ status: "ok", rule }, { status: 201 });
      }

      const rlDeleteMatch = path.match(/^\/api\/v1\/edge\/rate-limits\/([^/]+)$/);
      if (request.method === "DELETE" && rlDeleteMatch) {
        const [, ruleId] = rlDeleteMatch;
        const deleted = deleteRateLimitRule(decodeURIComponent(ruleId));
        if (!deleted) return jsonResponse({ error: "Rate limit rule not found" }, { status: 404 });
        return jsonResponse({ status: "ok", deleted: true });
      }

      // ── Traffic Metrics ──
      if (request.method === "GET" && path === "/api/v1/edge/metrics") {
        return jsonResponse({ status: "ok", summary: getTrafficSummary(), recentRequests: getMetrics(20) });
      }

      const metricsHostMatch = path.match(/^\/api\/v1\/edge\/metrics\/([^/]+)$/);
      if (request.method === "GET" && metricsHostMatch) {
        const [, encodedHost] = metricsHostMatch;
        return jsonResponse({ status: "ok", metrics: getMetricsForHost(decodeURIComponent(encodedHost)) });
      }

      // ── Ingress Proxy ──
      const requestHost = normalizeHost(
        request.headers.get("x-forwarded-host") || request.headers.get("host") || "",
      );

      if (!path.startsWith("/api/v1/edge") && requestHost) {
        const route = getRoute(requestHost);

        if (!route) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 404, latencyMs: Date.now() - startTime, clientIp: getClientIp(request) });
          return jsonResponse({ error: `No ingress route configured for host '${requestHost}'` }, { status: 404 });
        }
        if (!route.enabled) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 503, latencyMs: Date.now() - startTime, clientIp: getClientIp(request) });
          return jsonResponse({ error: "Ingress route disabled" }, { status: 503 });
        }
        if (DENY_TRUST_STATES.has(route.trustState)) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 403, latencyMs: Date.now() - startTime, clientIp: getClientIp(request) });
          return jsonResponse({ error: `Ingress denied due to trust state '${route.trustState}'` }, { status: 403 });
        }
        if (route.healthStatus === "unreachable") {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 502, latencyMs: Date.now() - startTime, clientIp: getClientIp(request) });
          return jsonResponse({ error: "Upstream unreachable" }, { status: 502 });
        }

        const clientIp = getClientIp(request);
        const rateLimit = checkAndRecordRateLimit(requestHost, clientIp);
        if (!rateLimit.allowed) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 429, latencyMs: Date.now() - startTime, clientIp });
          return jsonResponse(
            { error: "Rate limit exceeded", retryAfterMs: rateLimit.retryAfterMs },
            { status: 429, headers: { "retry-after": String(Math.ceil(rateLimit.retryAfterMs / 1000)) } },
          );
        }

        const upstreamUrl = buildUpstreamUrl(route.upstreamUrl, path, url.search);
        const proxyStart = Date.now();
        const response = await proxyRequest(request, upstreamUrl);
        const proxyLatency = Date.now() - proxyStart;

        recordMetric({ host: requestHost, method: request.method, path, statusCode: response.status, latencyMs: proxyLatency, clientIp });
        return response;
      }

      return jsonResponse({ error: "Not found" }, { status: 404 });
    },
  });

  console.log(`[nexus-edge] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusEdgeCloudRegistrationHeartbeat(baseUrl);

  return {
    server,
    stop: () => {
      stopHeartbeat();
      stopNexusHeartbeat();
      stopNexusMonitor();
      server.stop();
    },
  };
}
