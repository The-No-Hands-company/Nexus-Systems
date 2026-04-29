export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: {
    guardianVersion: string;
    supportsPolicyEnforcement: boolean;
    supportsThreatDetection: boolean;
    supportsAuditTrail: boolean;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-guardian",
    name: "Nexus Guardian",
    description: "Central safety, security, and system health command center",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["policy-enforcement", "threat-detection", "guardian", "audit-trail"],
    metadata: {
      guardianVersion: "v1",
      supportsPolicyEnforcement: true,
      supportsThreatDetection: true,
      supportsAuditTrail: true,
    },
  };
}
