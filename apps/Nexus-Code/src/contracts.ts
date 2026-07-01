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
    id: "nexus-code",
    name: "Nexus-Code",
    description: "Code snippets and repositories",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["code-snippets", "repositories", "version-control"],
    metadata: {
      version: "v1",
      defaultPort: 3126,
    },
  };
}
