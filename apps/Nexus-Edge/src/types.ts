export type EdgeThreatSeverity = "low" | "medium" | "high" | "critical";
export type EdgeResponseAction = "log" | "isolate" | "throttle" | "block" | "alert";
export type AnomalyType = "behavioral" | "performance" | "security" | "resource" | "governance";
export type EdgeIngressTarget = "direct" | "tunnel";
export type EdgeTrustState = "pending" | "verified" | "trusted" | "quarantined" | "revoked" | "expired";

export interface EdgeThreat {
  threatId: string;
  toolId: string;
  severity: EdgeThreatSeverity;
  description: string;
  detectedAt: string;
  resolvedAt?: string;
  resolution?: string;
}

export interface EdgeAnomaly {
  anomalyId: string;
  toolId: string;
  anomalyType: AnomalyType;
  confidence: number;
  details?: string;
  detectedAt: string;
  confirmed: boolean;
}

export interface EdgeResponse {
  responseId: string;
  threatId: string;
  action: EdgeResponseAction;
  executedAt: string;
  executedBy: string;
  result?: string;
}

export interface EdgePolicy {
  policyId: string;
  description: string;
  enableThreatDetection: boolean;
  enableAnomalyDetection: boolean;
  autoRespondToThreats: boolean;
  anomalyConfidenceThreshold: number;
  rateLimitEnabled: boolean;
  defaultRateLimitRps: number;
  healthCheckEnabled: boolean;
  healthCheckIntervalMs: number;
}

export interface AIGuardrailProfile {
  profileId: string;
  maxConcurrentRequests: number;
  maxResponseLatencyMs: number;
  behaviorCheckIntervalMs: number;
  suspicionThreshold: number;
}

export interface EdgeRoute {
  host: string;
  upstreamUrl: string;
  targetType: EdgeIngressTarget;
  trustState: EdgeTrustState;
  enabled: boolean;
  healthStatus: "healthy" | "degraded" | "unreachable";
  lastHealthCheck: string;
  createdAt: string;
  updatedAt: string;
  geoRoutingEnabled?: boolean;
  regionalUpstreams?: Record<string, string>;
  countryToRegion?: Record<string, string>;
}

export interface RateLimitRule {
  id: string;
  host: string;
  maxRequestsPerSecond: number;
  enabled: boolean;
  description: string;
}

export interface RateLimitBucket {
  host: string;
  clientIp: string;
  tokens: number;
  lastRefill: number;
}

export interface TrafficMetric {
  host: string;
  method: string;
  path: string;
  statusCode: number;
  latencyMs: number;
  clientIp: string;
  timestamp: string;
}

export interface TrafficSummary {
  host: string;
  totalRequests: number;
  avgLatencyMs: number;
  statusCodes: Record<number, number>;
  lastRequestAt: string;
}

export interface GeoRouteConfig {
  host: string;
  defaultRegion: string;
  regions: Record<string, string>;
  countryToRegion?: Record<string, string>;
  enabled: boolean;
  updatedAt: string;
}

export interface GeoRoutingResolution {
  host: string;
  clientRegion: string;
  resolvedRegion: string;
  upstreamUrl: string;
  matchedRule: string;
}
