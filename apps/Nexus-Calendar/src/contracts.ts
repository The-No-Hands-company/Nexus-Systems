export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  /** Calendar events are user data — always behind SSO. */
  requiresAuth: boolean;
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
    capabilities: ["calendar", "events", "scheduling"],
    requiresAuth: true,
    metadata: {
      version: "v1",
      defaultPort: 3068,
    },
  };
}
