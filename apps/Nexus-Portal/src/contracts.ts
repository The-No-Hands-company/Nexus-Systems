export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: Record<string, unknown>;
};

export function buildSystemsApiRegistrationPayload(
  baseUrl: string,
): SystemsApiRegistrationPayload {
  return {
    id: "nexus-portal",
    name: "Nexus-Portal",
    description: "Portal",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["portal"],
    metadata: {
      version: "v1",
      defaultPort: 3146,
    },
  };
}
