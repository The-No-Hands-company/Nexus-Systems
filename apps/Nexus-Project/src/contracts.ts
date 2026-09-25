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
    id: "nexus-project",
    name: "Nexus-Project",
    description: "Project management for solo users and teams: boards, WBS, dependencies and a critical-path schedule",
    mode: "orchestrated",
    exposed: true,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["projects", "tasks", "scheduling"],
    path: "/project",
    publicUrl: "https://project.tnhc.dev",
    delivery: "proxied-app",
    metadata: { version: "v1", defaultPort: 3152 },
  };
}
