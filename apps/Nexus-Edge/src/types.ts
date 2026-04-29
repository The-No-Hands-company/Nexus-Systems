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
  createdAt: string;
  updatedAt: string;
}
