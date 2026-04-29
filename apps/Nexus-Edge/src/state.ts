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
} from "./types";

let threatCounter = 0;
let anomalyCounter = 0;
let responseCounter = 0;

const threats = new Map<string, EdgeThreat>();
const anomalies = new Map<string, EdgeAnomaly>();
const responses: EdgeResponse[] = [];
const routes = new Map<string, EdgeRoute>();

const defaultPolicy: EdgePolicy = {
  policyId: "default",
  description: "Default edge policy — enable threat and anomaly detection with auto-response",
  enableThreatDetection: true,
  enableAnomalyDetection: true,
  autoRespondToThreats: true,
  anomalyConfidenceThreshold: 0.75,
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

export function clearThreats(): void {
  threats.clear();
  anomalies.clear();
  responses.length = 0;
  routes.clear();
  threatCounter = 0;
  anomalyCounter = 0;
  responseCounter = 0;
}
