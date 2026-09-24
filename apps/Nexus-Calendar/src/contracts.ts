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
  /**
   * The canonical in-shell route. Declared here because the payload below was
   * already sending it — an excess property on the returned object literal that
   * the type did not mention, which nothing caught because this app's tsc was
   * never actually runnable.
   */
  publicUrl: string;
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
    publicUrl: "/calendar",
    capabilities: ["calendar", "events", "scheduling"],
    requiresAuth: true,
    metadata: {
      version: "v1",
      defaultPort: 3068,
    },
  };
}
