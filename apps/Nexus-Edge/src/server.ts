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
  upsertGeoRoute,
  getGeoRoute,
  listGeoRoutes,
  deleteGeoRoute,
  resolveGeoRouting,
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
import { PhantomApp } from "../../../packages/phantom-sdk/src/integration";

const phantomApp = new PhantomApp("nexus-edge");
phantomApp.start()
  .then(() => phantomApp.registerWithCloud(process.env.NEXUS_CLOUD_URL ?? "http://localhost:8787"))
  .catch((err: unknown) => {
    console.error("[nexus-edge] Phantom startup failed:", err instanceof Error ? err.message : err);
  });

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

// The proxy strips address headers and supplies an opaque client tag instead.
// (Name kept for its callers; the value is a tag, not an address.)
function getClientKey(request: Request): string {
  return request.headers.get("x-nexus-client-tag") || "unknown";
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
    // Loopback unless told otherwise — Bun.serve binds every interface by
    // default; infrastructure services should only be reachable locally.
    hostname: process.env.NEXUS_BIND_HOST || "127.0.0.1",
    fetch: async (request) => {
      const url = new URL(request.url);
      const path = url.pathname;
      const startTime = Date.now();

      if (request.method === "GET" && (path === "/" || path === "/health")) {
        const accept = request.headers.get("accept") || "";
        if (accept.includes("text/html") && path === "/") {
          const html = `<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>Nexus Edge — Traffic Routing & Ingress Daemon</title>
  <style>
    :root { --bg: #090a0f; --card: #12151f; --line: #1e2436; --text: #e2e8f0; --muted: #798399; --accent: #38bdf8; }
    body { margin: 0; background: var(--bg); color: var(--text); font-family: ui-sans-serif, system-ui, -apple-system, sans-serif; display: flex; align-items: center; justify-content: center; min-height: 100vh; padding: 20px; }
    .card { background: var(--card); border: 1px solid var(--line); border-radius: 16px; padding: 36px; max-width: 580px; width: 100%; box-shadow: 0 20px 40px rgba(0,0,0,0.4); }
    .badge { display: inline-flex; align-items: center; gap: 8px; font-family: ui-monospace, monospace; font-size: 11px; text-transform: uppercase; letter-spacing: 0.1em; color: var(--accent); background: rgba(56,189,248,0.1); border: 1px solid rgba(56,189,248,0.3); border-radius: 999px; padding: 4px 12px; margin-bottom: 20px; }
    .dot { width: 8px; height: 8px; border-radius: 50%; background: var(--accent); }
    h1 { margin: 0 0 10px; font-size: 24px; font-weight: 700; color: #fff; }
    p { margin: 0 0 24px; color: var(--muted); font-size: 14px; line-height: 1.6; }
    .meta { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; margin-bottom: 24px; font-size: 13px; font-family: ui-monospace, monospace; }
    .meta-box { background: rgba(255,255,255,0.02); border: 1px solid var(--line); border-radius: 8px; padding: 12px; }
    .meta-box .k { color: var(--muted); font-size: 11px; margin-bottom: 4px; text-transform: uppercase; }
    .meta-box .v { color: #fff; font-weight: 600; }
    .btn { display: inline-flex; align-items: center; justify-content: center; gap: 8px; width: 100%; background: #0284c7; color: #fff; text-decoration: none; padding: 12px 20px; border-radius: 8px; font-weight: 600; font-size: 14px; transition: background 0.2s; }
    .btn:hover { background: #0369a1; }
  </style>
</head>
<body>
  <div class="card">
    <div class="badge"><span class="dot"></span> Platform Service Active</div>
    <h1>Nexus Edge</h1>
    <p>Edge termination, regional ingress routing, traffic anomaly detection, and distributed cloud gateway for the Nexus Systems ecosystem.</p>
    <div class="meta">
      <div class="meta-box"><div class="k">Role</div><div class="v">Edge Routing Daemon</div></div>
      <div class="meta-box"><div class="k">Status</div><div class="v">Operational</div></div>
      <div class="meta-box"><div class="k">Health Check</div><div class="v"><a href="/health" style="color:var(--accent);text-decoration:none;">/health → 200 OK</a></div></div>
      <div class="meta-box"><div class="k">API Protocol</div><div class="v">Systems API v1</div></div>
    </div>
    <a href="https://app.tnhc.dev/" class="btn">Return to Platform Dashboard →</a>
  </div>
</body>
</html>`;
          return new Response(html, {
            headers: { "content-type": "text/html; charset=utf-8" },
          });
        }

        return jsonResponse({ status: "ok", service: "nexus-edge", role: "platform_edge_daemon", timestamp: new Date().toISOString(), phantom: phantomApp.status() });
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

      // ── Regional Geo-Routing ──
      if (request.method === "GET" && path === "/api/v1/edge/geo-routing") {
        return jsonResponse({ status: "ok", geoRoutes: listGeoRoutes() });
      }

      if (request.method === "POST" && path === "/api/v1/edge/geo-routing") {
        const body = await parseBody(request);
        const host = typeof body.host === "string" ? normalizeHost(body.host) : "";
        const defaultRegion = typeof body.defaultRegion === "string" ? body.defaultRegion.trim() : "default";
        const regions = typeof body.regions === "object" && body.regions !== null ? (body.regions as Record<string, string>) : {};
        const countryToRegion = typeof body.countryToRegion === "object" && body.countryToRegion !== null ? (body.countryToRegion as Record<string, string>) : undefined;
        const enabled = typeof body.enabled === "boolean" ? body.enabled : true;

        if (!host || Object.keys(regions).length === 0) {
          return jsonResponse({ error: "host and at least one region target are required" }, { status: 400 });
        }

        const geoRoute = upsertGeoRoute({ host, defaultRegion, regions, countryToRegion, enabled });
        return jsonResponse({ status: "ok", geoRoute }, { status: 201 });
      }

      const geoDeleteMatch = path.match(/^\/api\/v1\/edge\/geo-routing\/([^/]+)$/);
      if (request.method === "DELETE" && geoDeleteMatch) {
        const [, encodedHost] = geoDeleteMatch;
        const deleted = deleteGeoRoute(decodeURIComponent(encodedHost));
        if (!deleted) return jsonResponse({ error: "Geo route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", deleted: true });
      }

      if (request.method === "POST" && path === "/api/v1/edge/geo-routing/resolve") {
        const body = await parseBody(request);
        const host = typeof body.host === "string" ? normalizeHost(body.host) : "";
        const region = typeof body.region === "string" ? body.region : undefined;
        const country = typeof body.country === "string" ? body.country : undefined;
        if (!host) return jsonResponse({ error: "host is required" }, { status: 400 });

        const resolution = resolveGeoRouting(host, { region, country });
        if (!resolution) {
          return jsonResponse({ error: `No active geo-routing configured for host '${host}'` }, { status: 404 });
        }
        return jsonResponse({ status: "ok", resolution });
      }

      // ── Ingress Proxy ──
      const requestHost = normalizeHost(
        request.headers.get("x-forwarded-host") || request.headers.get("host") || "",
      );

      if (!path.startsWith("/api/v1/edge") && requestHost) {
        const route = getRoute(requestHost);

        if (!route) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 404, latencyMs: Date.now() - startTime, clientIp: getClientKey(request) });
          return jsonResponse({ error: `No ingress route configured for host '${requestHost}'` }, { status: 404 });
        }
        if (!route.enabled) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 503, latencyMs: Date.now() - startTime, clientIp: getClientKey(request) });
          return jsonResponse({ error: "Ingress route disabled" }, { status: 503 });
        }
        if (DENY_TRUST_STATES.has(route.trustState)) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 403, latencyMs: Date.now() - startTime, clientIp: getClientKey(request) });
          return jsonResponse({ error: `Ingress denied due to trust state '${route.trustState}'` }, { status: 403 });
        }
        if (route.healthStatus === "unreachable") {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 502, latencyMs: Date.now() - startTime, clientIp: getClientKey(request) });
          return jsonResponse({ error: "Upstream unreachable" }, { status: 502 });
        }

        const clientIp = getClientKey(request);
        const rateLimit = checkAndRecordRateLimit(requestHost, clientIp);
        if (!rateLimit.allowed) {
          recordMetric({ host: requestHost, method: request.method, path, statusCode: 429, latencyMs: Date.now() - startTime, clientIp });
          return jsonResponse(
            { error: "Rate limit exceeded", retryAfterMs: rateLimit.retryAfterMs },
            { status: 429, headers: { "retry-after": String(Math.ceil(rateLimit.retryAfterMs / 1000)) } },
          );
        }

        const clientRegion =
          request.headers.get("x-nexus-region") ||
          request.headers.get("cf-ipcountry") ||
          request.headers.get("x-geo-country") ||
          url.searchParams.get("region") ||
          undefined;
        const clientCountry =
          request.headers.get("cf-ipcountry") ||
          request.headers.get("x-geo-country") ||
          undefined;

        const geoResolution = resolveGeoRouting(requestHost, {
          region: clientRegion,
          country: clientCountry,
        });
        const targetBaseUrl = geoResolution?.upstreamUrl || route.upstreamUrl;

        const upstreamUrl = buildUpstreamUrl(targetBaseUrl, path, url.search);
        const proxyStart = Date.now();
        const response = await proxyRequest(request, upstreamUrl);
        const proxyLatency = Date.now() - proxyStart;

        if (geoResolution) {
          response.headers.set("x-nexus-edge-region", geoResolution.resolvedRegion);
          response.headers.set("x-nexus-edge-routed-by", geoResolution.matchedRule);
        }

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
