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
    id: "nexus-graphic",
    name: "Nexus-Graphic",
    description:
      "Full graphic design suite with vectors, photo editing, templates, and AI generation",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["graphic-design", "photo-editing", "ai-generation"],
    metadata: {
      version: "v1",
      defaultPort: 3080,
    },
  };
}
