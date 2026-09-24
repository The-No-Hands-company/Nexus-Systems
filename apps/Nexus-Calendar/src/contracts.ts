export type SystemsApiRegistrationPayload = {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  /**
   * No `requiresAuth`: whether an app sits behind the SSO gate is Cloud's
   * operator-only switch, and Cloud ignores what an app says about itself.
   * Calendar's events stay private because every read is owner-scoped.
   */
  /** The canonical in-shell route. */
  path: string;
  /** The direct HTTPS origin for bookmarks and standalone access. */
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
