export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  path: string;
  publicUrl: string;
  delivery: "proxied-app";
  metadata: Record<string, unknown>;
};

export function buildSystemsApiRegistrationPayload(baseUrl: string): SystemsApiRegistrationPayload {
  return {
    id: "nexus-calendar",
    name: "Nexus-Calendar",
    description: "Shared calendars with events, reminders, and month/week views",
    mode: "orchestrated",
    exposed: true,
    health: "healthy",
    upstreamUrl: baseUrl,
    path: "/calendar",
    publicUrl: "https://calendar.tnhc.dev",
    delivery: "proxied-app",
    capabilities: ["calendar", "events", "scheduling"],
    metadata: {
      version: "v1",
      defaultPort: 3068,
    },
  };
}
