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
    id: "nexus-context",
    name: "Nexus-Context",
    description: "Context and state management",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["context-management", "state-management", "session"],
    metadata: {
      version: "v1",
      defaultPort: 3129,
    },
  };
}
