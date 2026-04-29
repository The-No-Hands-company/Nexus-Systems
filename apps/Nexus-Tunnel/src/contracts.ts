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
    tunnelVersion: string;
    supportsPublicExposure: boolean;
    supportsHealthAwareRouting: boolean;
    requiresGuardianApproval: boolean;
  };
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-tunnel",
    name: "Nexus Tunnel",
    description: "Secure exposure management and intelligent routing layer",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["routing", "exposure-control", "tunnel", "health-aware", "guardian-aware"],
    metadata: {
      tunnelVersion: "v1",
      supportsPublicExposure: true,
      supportsHealthAwareRouting: true,
      requiresGuardianApproval: true,
    },
  };
}
