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

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-api",
    name: "Nexus API",
    description:
      "Unified API gateway with declarative route configuration, transparent request proxying, auth passthrough, and centralized routing for the ecosystem",
    mode: "orchestrated",
    exposed: true,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["gateway", "routing", "proxy", "auth-passthrough"],
    metadata: {
      apiVersion: "v1",
      supportsProxy: true,
      supportsRouteManagement: true,
      defaultPort: 3036,
    },
  };
}
