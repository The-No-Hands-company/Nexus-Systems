export type TunnelExposureMode = "public" | "internal" | "protected";

export interface TunnelRoute {
  routeId: string;
  toolId: string;
  targetUrl: string;
  exposureMode: TunnelExposureMode;
  enabled: boolean;
  createdAt: string;
  updatedAt: string;
}

export interface TunnelExposure {
  toolId: string;
  exposureMode: TunnelExposureMode;
  guardianApproved: boolean;
  approvalReason?: string;
  denialReason?: string;
  requestedAt: string;
  approvedAt?: string;
}

export interface TunnelDecision {
  decisionId: string;
  toolId: string;
  exposureMode: TunnelExposureMode;
  approved: boolean;
  reason?: string;
  decidedAt: string;
  decidedBy: string;
}

export interface RoutingPolicy {
  policyId: string;
  description: string;
  maxPublicExposures: number;
  requireGuardianApproval: boolean;
  healthCheckIntervalMs: number;
}
