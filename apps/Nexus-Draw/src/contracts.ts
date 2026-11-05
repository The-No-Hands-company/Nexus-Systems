export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  publicUrl: string;
  delivery: "proxied-app";
  capabilities: string[];
  metadata: Record<string, unknown>;
};

export function drawPublicUrl(): string {
  return (process.env.NEXUS_DRAW_PUBLIC_URL || "https://draw.tnhc.dev").trim().replace(/\/$/, "");
}

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-draw",
    name: "Nexus Draw",
    description: "Real-time collaborative whiteboard and diagramming tool",
    mode: "orchestrated",
    exposed: true,
    health: "healthy",
    upstreamUrl: baseUrl,
    publicUrl: drawPublicUrl(),
    delivery: "proxied-app",
    capabilities: ["whiteboard", "diagramming", "collaboration"],
    metadata: {
      version: "v1",
      defaultPort: 3075,
    },
  };
}
