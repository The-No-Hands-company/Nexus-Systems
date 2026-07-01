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
    id: "nexus-cron",
    name: "Nexus-Cron",
    description: "Scheduled tasks and cron jobs",
    mode: "orchestrated",
    exposed: false,
    health: "healthy",
    upstreamUrl: baseUrl,
    capabilities: ["scheduled-tasks", "cron-jobs", "automation"],
    metadata: {
      version: "v1",
      defaultPort: 3131,
    },
  };
}
