import type {
  PhantomComplianceSummary,
  SystemsApiRouteEntry,
  SystemsApiStatusResponse,
  SystemsApiToolHeartbeat,
  SystemsApiToolRegistration,
} from "./contracts";

export const exampleToolRegistration: SystemsApiToolRegistration = {
  id: "nexus-auth",
  displayName: "Nexus Auth",
  description: "Identity and service authentication provider",
  mode: "orchestrated",
  upstreamUrl: "http://nexus-auth.internal:4310",
  capabilityTags: ["identity", "service-auth", "token-validation"],
};

export const exampleToolHeartbeat: SystemsApiToolHeartbeat = {
  toolId: "nexus-auth",
  status: "healthy",
  upstreamUrl: "http://nexus-auth.internal:4310",
};

export const exampleStatusResponse: SystemsApiStatusResponse = {
  version: "v1",
  mode: "orchestrated",
  totalTools: 7,
  exposedTools: 4,
  healthyTools: 6,
  integrationStatus: "healthy",
  lastUpdatedAt: "2026-04-27T00:00:00.000Z",
};

export const exampleRouteEntry: SystemsApiRouteEntry = {
  toolId: "nexus-auth",
  domain: "auth.nexus.local",
  upstreamUrl: "http://nexus-auth.internal:4310",
  securityTag: "transitional",
};

export const exampleComplianceSummary: PhantomComplianceSummary = {
  claimedSecuredCount: 2,
  compliantCount: 1,
  failingCount: 1,
  status: "failing",
};
