import { NexusClient, createConfig } from "../../../packages/nexus-sdk/src/index";
import { buildSystemsApiRegistrationPayload } from "./contracts";
import {
  startNexusTunnelCloudRegistrationHeartbeat,
  startNexusTunnelCloudReconciliationLoop,
  requestGuardianApprovalForExposure,
  pushHealthToGuardian,
  checkGuardianRateLimit,
} from "./cloud";
import {
  registerRoute,
  getRoute,
  setRouteEnabled,
  updateRouteHealth,
  listRoutes,
  getEnabledRoutes,
  recordExposure,
  getExposure,
  listExposures,
  recordDecision,
  listDecisions,
  getPolicy,
  upsertTlsCert,
  getTlsCert,
  listTlsCerts,
  getTlsCertsForTool,
  upsertExposureLifecycle,
  getExposureLifecycle,
  listExposureLifecycles,
  upsertReachabilityRecord,
  listReachabilityRecords,
} from "./state";
import {
  checkRouteHealth,
  getHealthHistory,
  getLatestHealth,
  getAllLatestHealth,
  startRouteHealthChecks,
  clearHealthData,
} from "./health-checker";
import {
  checkReachability,
  getReachability,
  listReachability,
  startReachabilityChecks,
  clearReachability,
} from "./reachability";
import type {
  TunnelExposureMode,
  TlsCertStatus,
} from "./types";

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
      { error: "Upstream route unavailable", upstreamUrl, detail: (error as Error).message },
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

  const policy = getPolicy();

  const stopHealthChecks = startRouteHealthChecks(
    getEnabledRoutes(),
    policy.healthCheckIntervalMs,
    (route, check) => {
      updateRouteHealth(route.toolId, check.status, route.consecutiveFailures + 1);
      setRouteEnabled(route.toolId, false);
      pushHealthToGuardian(route.toolId, route.toolId, route.targetUrl, check.status).catch(() => {});
    },
    policy.unhealthyThreshold,
    policy.autoDisableUnhealthy,
  );

  const stopReachability = startReachabilityChecks(
    getEnabledRoutes().map((r) => r.toolId),
    (toolId) => getRoute(toolId)?.targetUrl,
    policy.reachabilityCheckIntervalMs,
  );

  const persistInterval = setInterval(() => {
    const enabledRoutes = getEnabledRoutes();
    for (const route of enabledRoutes) {
      checkReachability(route.toolId, route.targetUrl).then((record) => {
        upsertReachabilityRecord(record);
      }).catch(() => {});
    }
  }, policy.reachabilityCheckIntervalMs);

  if (typeof persistInterval.unref === "function") {
    persistInterval.unref();
  }

  
const nexusClient = new NexusClient(createConfig({
  id: "nexus-tunnel",
  name: "Nexus Tunnel",
  description: "Secure exposure management and routing",
  port: 4330,
  capabilities: ["routing","exposure-control","tunnel","health-aware","reachability","tls-management"],
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
            health: "/api/v1/tunnel/health",
            healthHistory: "/api/v1/tunnel/health/:toolId",
            reachability: "/api/v1/tunnel/reachability",
            reachabilityCheck: "/api/v1/tunnel/reachability/:toolId",
            tlsCerts: "/api/v1/tunnel/tls",
            lifecycle: "/api/v1/tunnel/lifecycle",
          },
          capabilities: {
            routeCount: listRoutes().length,
            exposureCount: listExposures().length,
            tlsCertCount: listTlsCerts().length,
            lifecycleCount: listExposureLifecycles().length,
          },
          cloudIntegration: {
            enabled: cloudIntegrationEnabled,
            cloudUrl: process.env.NEXUS_CLOUD_URL || "http://localhost:8787",
          },
          guardianIntegration: {
            enabled: (process.env.NEXUS_TUNNEL_ENABLE_GUARDIAN_INTEGRATION || "true").trim().toLowerCase() !== "false",
            guardianUrl: process.env.NEXUS_GUARDIAN_URL || "http://localhost:4320",
          },
        });
      }

      // ── Routes ──
      if (request.method === "GET" && path === "/api/v1/tunnel/routes") {
        return jsonResponse({ status: "ok", routes: listRoutes() });
      }

      if (request.method === "POST" && path === "/api/v1/tunnel/routes") {
        const body = await parseBody(request);
        const toolId = typeof body.toolId === "string" ? body.toolId : "";
        const targetUrl = typeof body.targetUrl === "string" ? body.targetUrl : "";
        const exposureMode =
          typeof body.exposureMode === "string" && ["public", "internal", "protected"].includes(body.exposureMode)
            ? (body.exposureMode as TunnelExposureMode)
            : "internal";

        if (!toolId || !targetUrl) {
          return jsonResponse({ error: "toolId and targetUrl are required" }, { status: 400 });
        }

        const route = registerRoute(toolId, targetUrl, exposureMode);
        return jsonResponse({ status: "ok", route }, { status: 201 });
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

      // ── Route by ID ──
      const getRouteMatch = path.match(/^\/api\/v1\/tunnel\/routes\/([^/]+)$/);
      if (request.method === "GET" && getRouteMatch) {
        const [, toolId] = getRouteMatch;
        const route = getRoute(decodeURIComponent(toolId));
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      // ── Route enable/disable ──
      const enableRouteMatch = path.match(/^\/api\/v1\/tunnel\/routes\/([^/]+)\/enable$/);
      if (request.method === "POST" && enableRouteMatch) {
        const [, toolId] = enableRouteMatch;
        const route = setRouteEnabled(decodeURIComponent(toolId), true);
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      const disableRouteMatch = path.match(/^\/api\/v1\/tunnel\/routes\/([^/]+)\/disable$/);
      if (request.method === "POST" && disableRouteMatch) {
        const [, toolId] = disableRouteMatch;
        const route = setRouteEnabled(decodeURIComponent(toolId), false);
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });
        return jsonResponse({ status: "ok", route });
      }

      // ── Health ──
      if (request.method === "GET" && path === "/api/v1/tunnel/health") {
        const enabledRoutes = getEnabledRoutes();
        const results = await Promise.all(
          enabledRoutes.map((r) => checkRouteHealth(r)),
        );
        return jsonResponse({ status: "ok", health: results });
      }

      if (request.method === "GET" && path === "/api/v1/tunnel/health/latest") {
        return jsonResponse({ status: "ok", health: getAllLatestHealth() });
      }

      const healthHistoryMatch = path.match(/^\/api\/v1\/tunnel\/health\/([^/]+)$/);
      if (request.method === "GET" && healthHistoryMatch) {
        const [, toolId] = healthHistoryMatch;
        const history = getHealthHistory(decodeURIComponent(toolId));
        const latest = getLatestHealth(decodeURIComponent(toolId));
        return jsonResponse({ status: "ok", toolId: decodeURIComponent(toolId), latest, history });
      }

      // ── Reachability ──
      if (request.method === "GET" && path === "/api/v1/tunnel/reachability") {
        return jsonResponse({ status: "ok", reachability: listReachabilityRecords() });
      }

      const reachMatch = path.match(/^\/api\/v1\/tunnel\/reachability\/([^/]+)$/);
      if (request.method === "GET" && reachMatch) {
        const [, toolId] = reachMatch;
        const record = await checkReachability(
          decodeURIComponent(toolId),
          getRoute(decodeURIComponent(toolId))?.targetUrl || "",
        );
        upsertReachabilityRecord(record);
        return jsonResponse({ status: "ok", reachability: record });
      }

      const reachCheckMatch = path.match(/^\/api\/v1\/tunnel\/reachability\/([^/]+)\/check$/);
      if (request.method === "POST" && reachCheckMatch) {
        const [, toolId] = reachCheckMatch;
        const route = getRoute(decodeURIComponent(toolId));
        if (!route) return jsonResponse({ error: "Route not found" }, { status: 404 });

        const record = await checkReachability(route.toolId, route.targetUrl);
        upsertReachabilityRecord(record);
        return jsonResponse({ status: "ok", reachability: record });
      }

      // ── TLS Certs ──
      if (request.method === "GET" && path === "/api/v1/tunnel/tls") {
        return jsonResponse({ status: "ok", certs: listTlsCerts() });
      }

      if (request.method === "POST" && path === "/api/v1/tunnel/tls") {
        const body = await parseBody(request);
        const toolId = typeof body.toolId === "string" ? body.toolId.trim() : "";
        const domain = typeof body.domain === "string" ? body.domain.trim() : "";
        const status: TlsCertStatus = typeof body.status === "string" &&
          ["requested", "issued", "renewing", "expired", "revoked"].includes(body.status)
          ? body.status as TlsCertStatus
          : "requested";
        const issuer = typeof body.issuer === "string" ? body.issuer : "nexus-tunnel";
        const expiresAt = typeof body.expiresAt === "string" ? body.expiresAt
          : new Date(Date.now() + 90 * 24 * 60 * 60 * 1000).toISOString();
        const autoRenew = typeof body.autoRenew === "boolean" ? body.autoRenew : true;

        if (!toolId || !domain) {
          return jsonResponse({ error: "toolId and domain are required" }, { status: 400 });
        }

        const cert = upsertTlsCert({
          certId: typeof body.certId === "string" ? body.certId : undefined,
          toolId,
          domain,
          status,
          issuer,
          expiresAt,
          autoRenew,
        });

        return jsonResponse({ status: "ok", cert }, { status: 201 });
      }

      const getCertMatch = path.match(/^\/api\/v1\/tunnel\/tls\/([^/]+)$/);
      if (request.method === "GET" && getCertMatch) {
        const [, certId] = getCertMatch;
        const cert = getTlsCert(decodeURIComponent(certId));
        if (!cert) return jsonResponse({ error: "Certificate not found" }, { status: 404 });
        return jsonResponse({ status: "ok", cert });
      }

      const toolCertsMatch = path.match(/^\/api\/v1\/tunnel\/tls\/tool\/([^/]+)$/);
      if (request.method === "GET" && toolCertsMatch) {
        const [, toolId] = toolCertsMatch;
        return jsonResponse({ status: "ok", certs: getTlsCertsForTool(decodeURIComponent(toolId)) });
      }

      // ── Exposure Lifecycle ──
      if (request.method === "GET" && path === "/api/v1/tunnel/lifecycle") {
        return jsonResponse({ status: "ok", lifecycles: listExposureLifecycles() });
      }

      const lifecycleMatch = path.match(/^\/api\/v1\/tunnel\/lifecycle\/([^/]+)$/);
      if (request.method === "POST" && lifecycleMatch) {
        const [, toolId] = lifecycleMatch;
        const body = await parseBody(request);
        const exposureMode: TunnelExposureMode =
          typeof body.exposureMode === "string" && ["public", "internal", "protected"].includes(body.exposureMode)
            ? body.exposureMode as TunnelExposureMode
            : "public";
        const maxRenewals = typeof body.maxRenewals === "number" ? body.maxRenewals : 12;

        const lifecycle = upsertExposureLifecycle(decodeURIComponent(toolId), exposureMode, maxRenewals);
        return jsonResponse({ status: "ok", lifecycle }, { status: 201 });
      }

      if (request.method === "GET" && lifecycleMatch) {
        const [, toolId] = lifecycleMatch;
        const lifecycle = getExposureLifecycle(decodeURIComponent(toolId));
        if (!lifecycle) return jsonResponse({ error: "Lifecycle not found" }, { status: 404 });
        return jsonResponse({ status: "ok", lifecycle });
      }

      // ── Exposure decision (Guardian integration) ──
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

      if (request.method === "POST" && decideMatch) {
        const [, toolId] = decideMatch;
        const decodedToolId = decodeURIComponent(toolId);
        const body = await parseBody(request);

        const exposureMode: TunnelExposureMode =
          typeof body.exposureMode === "string" && ["public", "internal", "protected"].includes(body.exposureMode)
            ? body.exposureMode as TunnelExposureMode
            : "internal";

        const requireApproval = exposureMode === "public";
        let guardianApproved = !requireApproval;
        let reason = "";

        if (requireApproval) {
          const guardianResult = await requestGuardianApprovalForExposure(decodedToolId, exposureMode);
          guardianApproved = guardianResult.approved;
          reason = guardianResult.reason || "Guardian decision pending";
        }

        const publicUrl = exposureMode === "public"
          ? `https://${decodedToolId}.nexus.local`
          : undefined;

        const exposure = recordExposure(decodedToolId, exposureMode, guardianApproved, reason, publicUrl);
        recordDecision(decodedToolId, exposureMode, guardianApproved, "tunnel",
          reason || `Exposure ${exposureMode} decided`);

        if (guardianApproved && exposureMode === "public") {
          upsertExposureLifecycle(decodedToolId, exposureMode);
        }

        return jsonResponse(
          { status: guardianApproved ? "approved" : "denied", exposure },
          { status: guardianApproved ? 200 : 403 },
        );
      }

      // ── Legacy expose alias ──
      const legacyMatch = path.match(/^\/api\/v1\/tunnel\/expose\/([^/]+)$/);
      if (request.method === "POST" && legacyMatch) {
        const [, toolId] = legacyMatch;
        const decodedToolId = decodeURIComponent(toolId);
        const body = await parseBody(request);

        const exposureMode: TunnelExposureMode =
          typeof body.exposureMode === "string" && ["public", "internal", "protected"].includes(body.exposureMode)
            ? body.exposureMode as TunnelExposureMode
            : "internal";

        const requireApproval = exposureMode === "public";
        let guardianApproved = !requireApproval;
        let reason = "";

        if (requireApproval) {
          const guardianResult = await requestGuardianApprovalForExposure(decodedToolId, exposureMode);
          guardianApproved = guardianResult.approved;
          reason = guardianResult.reason || "Guardian decision pending";
        }

        const publicUrl = exposureMode === "public"
          ? `https://${decodedToolId}.nexus.local`
          : undefined;

        const exposure = recordExposure(decodedToolId, exposureMode, guardianApproved, reason, publicUrl);
        recordDecision(decodedToolId, exposureMode, guardianApproved, "tunnel",
          reason || `Exposure ${exposureMode} decided`);

        return jsonResponse(
          { status: guardianApproved ? "approved" : "denied", exposure },
          { status: guardianApproved ? 200 : 403 },
        );
      }

      // ── Proxy /t/:toolId/* ──
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

        if (route.healthStatus === "unreachable") {
          return jsonResponse({ error: "Route is unreachable" }, { status: 503 });
        }

        if (route.exposureMode === "public") {
          const exposure = getExposure(toolId);
          if (!exposure?.guardianApproved) {
            return jsonResponse({ error: "Public route requires Guardian approval" }, { status: 403 });
          }

          const rateLimit = await checkGuardianRateLimit(toolId, "exposure");
          if (rateLimit?.throttled) {
            return jsonResponse(
              { error: "Rate limit exceeded — retry after window reset", rateLimit },
              { status: 429 },
            );
          }
        }

        const suffixPath = rawSuffix ? `/${rawSuffix}` : "/";
        const upstreamUrl = buildUpstreamUrl(route.targetUrl, suffixPath, url.search);

        checkRouteHealth(route).then((check) => {
          if (check.status === "unreachable") {
            updateRouteHealth(route.toolId, check.status, route.consecutiveFailures + 1);
            if (route.consecutiveFailures + 1 >= policy.maxConsecutiveFailures) {
              setRouteEnabled(route.toolId, false);
            }
            pushHealthToGuardian(route.toolId, route.toolId, route.targetUrl, "unreachable").catch(() => {});
          } else {
            updateRouteHealth(route.toolId, check.status, 0);
          }
        }).catch(() => {});

        return await proxyToRoute(request, upstreamUrl);
      }

      return jsonResponse({ error: "Not found" }, { status: 404 });
    },
  });

  console.log(`[nexus-tunnel] Listening on port ${server.port}`);

  const stopHeartbeat = startNexusTunnelCloudRegistrationHeartbeat(baseUrl);
  const stopReconciliation = startNexusTunnelCloudReconciliationLoop();

  return {
    server,
    stop: () => {
      stopHeartbeat();
      stopReconciliation();
      stopHealthChecks();
      stopReachability();
      clearInterval(persistInterval);
      clearHealthData();
      clearReachability();
      server.stop();
    },
  };
}
