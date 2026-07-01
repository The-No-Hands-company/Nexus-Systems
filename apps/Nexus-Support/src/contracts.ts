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
    id: "nexus-support",
    name: "Nexus-Support",
    description: "Nexus Support app",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["support"],
    metadata: {
      version: "v1",
      defaultPort: 3179,
    },
  };
}
