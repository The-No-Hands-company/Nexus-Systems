export type GuardianDecisionVerdict = "approve" | "deny" | "suspend" | "quarantine";

export type GuardianScope = "service" | "user" | "agent" | "network" | "resource";

export interface GuardianDecision {
  id: string;
  scope: GuardianScope;
  subjectId: string;
  verdict: GuardianDecisionVerdict;
  reason?: string;
  issuedAt: string;
  issuedBy: string;
}

export interface GuardianAuditEntry {
  eventId: string;
  decisionId: string;
  scope: GuardianScope;
  subjectId: string;
  verdict: GuardianDecisionVerdict;
  timestamp: string;
}

export interface GuardianPolicy {
  policyId: string;
  description: string;
  version: number;
  defaultVerdict: GuardianDecisionVerdict;
  enabled: boolean;
  publicExposureDenyRules: string[];
  updatedAt: string;
}

export interface GuardianServiceCapabilities {
  policyEnforcement: boolean;
  threatDetection: boolean;
  auditTrail: boolean;
  supportedScopes: GuardianScope[];
  supportedVerdicts: GuardianDecisionVerdict[];
}
