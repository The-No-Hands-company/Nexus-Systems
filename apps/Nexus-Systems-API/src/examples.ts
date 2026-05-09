import type {
  BatchServiceRegistration,
  PhantomComplianceSummary,
  SystemsApiRouteEntry,
  SystemsApiStatusResponse,
  SystemsApiToolHeartbeat,
  SystemsApiToolRegistration,
} from "./contracts";
import { batchServiceRegistry } from "./contracts";

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

// Batch service example registrations (pinned 2026-05-08, items 1–9)
export const batchServiceRegistrations: Record<string, BatchServiceRegistration> = {
  "nexus-monitor": {
    id: "nexus-monitor",
    displayName: "Nexus Monitor",
    description: "Ecosystem health monitoring — ingest, heartbeat, and alert surface",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-monitor"].port}`,
    capabilityTags: ["monitoring", "health-ingest", "alerting"],
    role: batchServiceRegistry["nexus-monitor"].role,
  },
  "nexus-compliance": {
    id: "nexus-compliance",
    displayName: "Nexus Compliance",
    description: "Compliance event ingestion and status reporting",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-compliance"].port}`,
    capabilityTags: ["compliance", "event-ingestion", "audit"],
    role: batchServiceRegistry["nexus-compliance"].role,
  },
  "nexus-ai-hub": {
    id: "nexus-ai-hub",
    displayName: "Nexus AI Hub",
    description: "AI provider routing and aggregation layer",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-ai-hub"].port}`,
    capabilityTags: ["ai-routing", "provider-aggregation", "model-dispatch"],
    role: batchServiceRegistry["nexus-ai-hub"].role,
  },
  "nexus-files": {
    id: "nexus-files",
    displayName: "Nexus Files",
    description: "File storage and metadata management",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-files"].port}`,
    capabilityTags: ["file-storage", "metadata", "object-store"],
    role: batchServiceRegistry["nexus-files"].role,
  },
  "nexus-search": {
    id: "nexus-search",
    displayName: "Nexus Search",
    description: "Full-text and semantic search across ecosystem content",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-search"].port}`,
    capabilityTags: ["search", "full-text", "semantic"],
    role: batchServiceRegistry["nexus-search"].role,
  },
  "nexus-ide": {
    id: "nexus-ide",
    displayName: "Nexus IDE",
    description: "Browser-based IDE session management",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-ide"].port}`,
    capabilityTags: ["ide", "session-management", "code-editing"],
    role: batchServiceRegistry["nexus-ide"].role,
  },
  "nexus-api": {
    id: "nexus-api",
    displayName: "Nexus API",
    description: "API gateway and request aggregator for ecosystem services",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-api"].port}`,
    capabilityTags: ["gateway", "routing", "aggregation"],
    role: batchServiceRegistry["nexus-api"].role,
  },
  "nexus-testing": {
    id: "nexus-testing",
    displayName: "Nexus Testing",
    description: "Test orchestration — job submission, run tracking, and result surface",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-testing"].port}`,
    capabilityTags: ["test-runner", "job-orchestration", "contract-testing"],
    role: batchServiceRegistry["nexus-testing"].role,
  },
  "nexus-forge": {
    id: "nexus-forge",
    displayName: "Nexus Forge",
    description: "Self-hosted federated code forge (Git/SVN/Mercurial/Pijul)",
    mode: "standalone",
    upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-forge"].port}`,
    capabilityTags: [
      "repository-management",
      "federation-discovery",
      "pull-requests",
      "issues",
      "multi-vcs",
    ],
    role: batchServiceRegistry["nexus-forge"].role,
  },
};

// Example heartbeat for a batch service
export const exampleBatchHeartbeat: SystemsApiToolHeartbeat = {
  toolId: "nexus-monitor",
  status: "healthy",
  upstreamUrl: `http://localhost:${batchServiceRegistry["nexus-monitor"].port}`,
};
