export const systemsApiV1Endpoints = {
  registerTool: "/api/v1/tools",
  heartbeat: "/api/v1/tools/:toolId/heartbeat",
  status: "/api/v1/status",
  routes: "/api/v1/routes",
  phantomCompliance: "/api/v1/compliance/phantom",
  phantomComplianceSummary: "/api/v1/compliance/phantom/summary",
} as const;

// Canonical batch service IDs and default ports (pinned 2026-05-08)
export const batchServiceRegistry = {
  "nexus-monitor": { port: 3030, role: "monitoring" },
  "nexus-compliance": { port: 3031, role: "compliance" },
  "nexus-ai-hub": { port: 3032, role: "ai-hub" },
  "nexus-files": { port: 3033, role: "file-storage" },
  "nexus-search": { port: 3034, role: "search" },
  "nexus-ide": { port: 3035, role: "ide" },
  "nexus-api": { port: 3036, role: "gateway" },
  "nexus-testing": { port: 3037, role: "test-runner" },
  "nexus-forge": { port: 8090, role: "code-forge" },
} as const;

export type BatchServiceId = keyof typeof batchServiceRegistry;

// Shape delta note: Nexus-Forge registers with { name, capabilities } instead of
// { displayName, capabilityTags }. The canonical SDK shape below is the v1 standard;
// Forge is a known delta tracked in Nexus-Forge/docs/BATCH_ALIGNMENT_NOTE.md.
export type BatchServiceRegistration = SystemsApiToolRegistration & {
  role: string;
};

export type SystemsApiToolRegistration = {
  id: string;
  displayName: string;
  description: string;
  mode: "standalone" | "orchestrated";
  upstreamUrl: string;
  capabilityTags: string[];
};

export type SystemsApiToolHeartbeat = {
  toolId: string;
  status: "healthy" | "degraded" | "offline";
  upstreamUrl?: string;
};

export type SystemsApiStatusResponse = {
  version: "v1";
  mode: "standalone" | "orchestrated";
  totalTools: number;
  exposedTools: number;
  healthyTools: number;
  integrationStatus: "healthy" | "failing";
  lastUpdatedAt: string;
};

export type SystemsApiRouteEntry = {
  toolId: string;
  domain: string;
  upstreamUrl: string;
  securityTag: "phantom-hardened" | "transitional";
};

export type PhantomComplianceSummary = {
  claimedSecuredCount: number;
  compliantCount: number;
  failingCount: number;
  status: "healthy" | "failing";
};
