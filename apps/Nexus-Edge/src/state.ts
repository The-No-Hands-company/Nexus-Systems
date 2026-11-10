import type {
  EdgeThreat,
  EdgeThreatSeverity,
  EdgeAnomaly,
  AnomalyType,
  EdgeResponse,
  EdgeResponseAction,
  EdgePolicy,
  AIGuardrailProfile,
  EdgeRoute,
  EdgeIngressTarget,
  EdgeTrustState,
  GeoRouteConfig,
  GeoRoutingResolution,
} from "./types";
import { mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

let threatCounter = 0;
let anomalyCounter = 0;
let responseCounter = 0;

const threats = new Map<string, EdgeThreat>();
const anomalies = new Map<string, EdgeAnomaly>();
const responses: EdgeResponse[] = [];
const routes = new Map<string, EdgeRoute>();
const geoRoutes = new Map<string, GeoRouteConfig>();

const stateFilePath = resolve(
  process.env.NEXUS_EDGE_STATE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "edge-state.json"),
);

const defaultPolicy: EdgePolicy = {
  policyId: "default",
  description: "Default edge policy — enable threat and anomaly detection with auto-response, rate limiting, and health checks",
  enableThreatDetection: true,
  enableAnomalyDetection: true,
  autoRespondToThreats: true,
  anomalyConfidenceThreshold: 0.75,
  rateLimitEnabled: true,
  defaultRateLimitRps: 100,
  healthCheckEnabled: true,
  healthCheckIntervalMs: 30000,
};

const defaultAIGuardrail: AIGuardrailProfile = {
  profileId: "default",
  maxConcurrentRequests: 100,
  maxResponseLatencyMs: 5000,
  behaviorCheckIntervalMs: 60000,
  suspicionThreshold: 0.8,
};

function generateId(prefix: string): string {
  return `${prefix}-${Date.now()}-${++threatCounter}`;
}

function normalizeHost(host: string): string {
  return host.trim().toLowerCase().replace(/:\d+$/, "");
}

export function upsertRoute(
  host: string,
  upstreamUrl: string,
  targetType: EdgeIngressTarget = "direct",
  trustState: EdgeTrustState = "verified",
): EdgeRoute {
  const normalizedHost = normalizeHost(host);
  const now = new Date().toISOString();
  const existing = routes.get(normalizedHost);

  const route: EdgeRoute = {
    host: normalizedHost,
    upstreamUrl,
    targetType,
    trustState,
    enabled: existing?.enabled ?? true,
    healthStatus: existing?.healthStatus ?? "healthy",
    lastHealthCheck: existing?.lastHealthCheck ?? now,
    createdAt: existing?.createdAt ?? now,
    updatedAt: now,
  };

  routes.set(normalizedHost, route);
  return route;
}

export function getRoute(host: string): EdgeRoute | undefined {
  return routes.get(normalizeHost(host));
}

export function listRoutes(): EdgeRoute[] {
  return Array.from(routes.values());
}

export function upsertGeoRoute(config: {
  host: string;
  defaultRegion: string;
  regions: Record<string, string>;
  countryToRegion?: Record<string, string>;
  enabled?: boolean;
}): GeoRouteConfig {
  const host = normalizeHost(config.host);
  const existing = geoRoutes.get(host);
  const now = new Date().toISOString();
  const entry: GeoRouteConfig = {
    host,
    defaultRegion: config.defaultRegion || existing?.defaultRegion || "default",
    regions: { ...(existing?.regions || {}), ...config.regions },
    countryToRegion: { ...(existing?.countryToRegion || {}), ...(config.countryToRegion || {}) },
    enabled: config.enabled ?? existing?.enabled ?? true,
    updatedAt: now,
  };
  geoRoutes.set(host, entry);
  persistState();
  return entry;
}

export function getGeoRoute(host: string): GeoRouteConfig | undefined {
  return geoRoutes.get(normalizeHost(host));
}

export function listGeoRoutes(): GeoRouteConfig[] {
  return Array.from(geoRoutes.values());
}

export function deleteGeoRoute(host: string): boolean {
  const result = geoRoutes.delete(normalizeHost(host));
  if (result) persistState();
  return result;
}

export function resolveGeoRouting(
  host: string,
  clientHint?: { region?: string; country?: string },
): GeoRoutingResolution | null {
  const config = geoRoutes.get(normalizeHost(host));
  if (!config || !config.enabled) return null;

  let targetRegion = config.defaultRegion;
  let matchedRule = "default_fallback";

  const directRegion = clientHint?.region?.toLowerCase().trim();
  const countryCode = clientHint?.country?.toUpperCase().trim();

  if (directRegion && config.regions[directRegion]) {
    targetRegion = directRegion;
    matchedRule = `direct_region:${directRegion}`;
  } else if (countryCode && config.countryToRegion && config.countryToRegion[countryCode]) {
    const mapped = config.countryToRegion[countryCode].toLowerCase();
    if (config.regions[mapped]) {
      targetRegion = mapped;
      matchedRule = `country_mapping:${countryCode}->${mapped}`;
    }
  }

  const upstreamUrl = config.regions[targetRegion] || config.regions[config.defaultRegion];
  if (!upstreamUrl) return null;

  return {
    host: config.host,
    clientRegion: directRegion || countryCode || "unknown",
    resolvedRegion: targetRegion,
    upstreamUrl,
    matchedRule,
  };
}

export function setRouteEnabled(host: string, enabled: boolean): EdgeRoute | undefined {
  const route = routes.get(normalizeHost(host));
  if (!route) return undefined;
  route.enabled = enabled;
  route.updatedAt = new Date().toISOString();
  return route;
}

export function setRouteTrustState(host: string, trustState: EdgeTrustState): EdgeRoute | undefined {
  const route = routes.get(normalizeHost(host));
  if (!route) return undefined;
  route.trustState = trustState;
  route.updatedAt = new Date().toISOString();
  return route;
}

export function recordThreat(
  toolId: string,
  severity: EdgeThreatSeverity,
  description: string,
): EdgeThreat {
  const threatId = `threat-${Date.now()}-${++threatCounter}`;
  const detectedAt = new Date().toISOString();

  const threat: EdgeThreat = { threatId, toolId, severity, description, detectedAt };
  threats.set(threatId, threat);

  return threat;
}

export function resolveThreat(threatId: string, resolution: string): EdgeThreat | undefined {
  const threat = threats.get(threatId);
  if (threat) {
    threat.resolvedAt = new Date().toISOString();
    threat.resolution = resolution;
  }
  return threat;
}

export function getThreat(threatId: string): EdgeThreat | undefined {
  return threats.get(threatId);
}

export function listThreats(): EdgeThreat[] {
  return Array.from(threats.values());
}

export function recordAnomaly(
  toolId: string,
  anomalyType: AnomalyType,
  confidence: number,
  details?: string,
): EdgeAnomaly {
  const anomalyId = `anomaly-${Date.now()}-${++anomalyCounter}`;
  const detectedAt = new Date().toISOString();

  const anomaly: EdgeAnomaly = {
    anomalyId,
    toolId,
    anomalyType,
    confidence: Math.min(1, Math.max(0, confidence)),
    details,
    detectedAt,
    confirmed: false,
  };

  anomalies.set(anomalyId, anomaly);

  return anomaly;
}

export function confirmAnomaly(anomalyId: string): EdgeAnomaly | undefined {
  const anomaly = anomalies.get(anomalyId);
  if (anomaly) {
    anomaly.confirmed = true;
  }
  return anomaly;
}

export function getAnomaly(anomalyId: string): EdgeAnomaly | undefined {
  return anomalies.get(anomalyId);
}

export function listAnomalies(): EdgeAnomaly[] {
  return Array.from(anomalies.values());
}

export function recordResponse(
  threatId: string,
  action: EdgeResponseAction,
  executedBy = "edge",
  result?: string,
): EdgeResponse {
  const responseId = `response-${Date.now()}-${++responseCounter}`;
  const executedAt = new Date().toISOString();

  const response: EdgeResponse = { responseId, threatId, action, executedAt, executedBy, result };
  responses.push(response);

  return response;
}

export function listResponses(): EdgeResponse[] {
  return [...responses];
}

export function getPolicy(): EdgePolicy {
  return { ...defaultPolicy };
}

export function getAIGuardrailProfile(): AIGuardrailProfile {
  return { ...defaultAIGuardrail };
}

export function updateRouteHealth(host: string, healthStatus: EdgeRoute["healthStatus"]): EdgeRoute | undefined {
  const route = routes.get(normalizeHost(host));
  if (!route) return undefined;
  route.healthStatus = healthStatus;
  route.lastHealthCheck = new Date().toISOString();
  route.updatedAt = new Date().toISOString();
  persistState();
  return route;
}

export function clearThreats(): void {
  threats.clear();
  anomalies.clear();
  responses.length = 0;
  routes.clear();
  threatCounter = 0;
  anomalyCounter = 0;
  responseCounter = 0;
  persistState();
}

function persistState(): void {
  const payload = {
    version: 1,
    threats: Array.from(threats.values()),
    anomalies: Array.from(anomalies.values()),
    responses: [...responses],
    routes: Array.from(routes.values()),
    geoRoutes: Array.from(geoRoutes.values()),
    updatedAt: new Date().toISOString(),
  };

  const tempPath = `${stateFilePath}.tmp`;
  try {
    mkdirSync(dirname(stateFilePath), { recursive: true });
    writeFileSync(tempPath, JSON.stringify(payload, null, 2), "utf-8");
    renameSync(tempPath, stateFilePath);
  } catch {
    try { rmSync(tempPath, { force: true }); } catch {}
  }
}

function hydrateState(): void {
  try {
    const raw = readFileSync(stateFilePath, "utf-8");
    const parsed = JSON.parse(raw) as {
      routes?: EdgeRoute[];
      threats?: EdgeThreat[];
      geoRoutes?: GeoRouteConfig[];
    };
    for (const route of Array.isArray(parsed.routes) ? parsed.routes : []) {
      if (route && route.host) routes.set(route.host, route);
    }
    for (const threat of Array.isArray(parsed.threats) ? parsed.threats : []) {
      if (threat && threat.threatId) threats.set(threat.threatId, threat);
    }
    for (const geo of Array.isArray(parsed.geoRoutes) ? parsed.geoRoutes : []) {
      if (geo && geo.host) geoRoutes.set(geo.host, geo);
    }
  } catch {
    persistState();
  }
}

hydrateState();
