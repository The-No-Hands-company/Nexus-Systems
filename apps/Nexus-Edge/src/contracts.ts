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
    edgeVersion: string;
    supportsThreatDetection: boolean;
    supportsAnomalyDetection: boolean;
    supportsAIGuardrails: boolean;
    supportsAutoResponse: boolean;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-edge",
    name: "Nexus Edge",
    description: "Gateway orchestration with AI behavior guardrails and real-time threat response",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["threat-detection", "edge-enforcement", "ai-guardrails", "anomaly-detection", "auto-response"],
    metadata: {
      edgeVersion: "v1",
      supportsThreatDetection: true,
      supportsAnomalyDetection: true,
      supportsAIGuardrails: true,
      supportsAutoResponse: true,
    },
  };
}
