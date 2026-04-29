export const systemsApiV1Endpoints = {
  registerTool: "/api/v1/tools",
  heartbeat: "/api/v1/tools/:toolId/heartbeat",
  status: "/api/v1/status",
  routes: "/api/v1/routes",
  phantomCompliance: "/api/v1/compliance/phantom",
  phantomComplianceSummary: "/api/v1/compliance/phantom/summary",
} as const;

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
