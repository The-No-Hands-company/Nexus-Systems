import type { NexusServiceConfig } from "./types";

export function createConfig(options: {
  id: string;
  name: string;
  description: string;
  port: number;
  capabilities: string[];
}): NexusServiceConfig {
  const envPrefix = options.id.toUpperCase().replace(/-/g, "_");
  const baseUrl = process.env[`NEXUS_${envPrefix}_BASE_URL`] || `http://localhost:${options.port}`;

  return {
    id: options.id,
    name: options.name,
    description: options.description,
    port: options.port,
    baseUrl,
    capabilities: options.capabilities,
    // Where registration and heartbeats go. NEXUS_CLOUD_URL is Cloud's public
    // address (for discovery); sending heartbeats there routes them out and back
    // through the proxy's sign-in gate, which refuses them. The internal
    // address wins when set. See tests/config.test.ts.
    cloudUrl: (process.env.NEXUS_CLOUD_INTERNAL_URL || process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/+$/, ""),
    cloudApiKey: process.env.NEXUS_CLOUD_API_KEY,
    guardianUrl: process.env.NEXUS_GUARDIAN_URL || "http://localhost:4320",
    authUrl: process.env.NEXUS_AUTH_URL || "http://localhost:4310",
    monitorUrl: process.env.NEXUS_MONITOR_URL || "http://localhost:3030",
    tunnelUrl: process.env.NEXUS_TUNNEL_URL || "http://localhost:4330",
    searchUrl: process.env.NEXUS_SEARCH_URL || "http://localhost:3034",
    enableCloudIntegration: (process.env[`NEXUS_${envPrefix}_ENABLE_CLOUD_INTEGRATION`] || "true").trim().toLowerCase() !== "false",
    enableMonitorIntegration: (process.env[`NEXUS_${envPrefix}_ENABLE_MONITOR_INTEGRATION`] || "true").trim().toLowerCase() !== "false",
    enableGuardianIntegration: (process.env[`NEXUS_${envPrefix}_ENABLE_GUARDIAN_INTEGRATION`] || "true").trim().toLowerCase() !== "false",
    heartbeatIntervalMs: Math.max(5000, Number(process.env[`NEXUS_${envPrefix}_CLOUD_HEARTBEAT_INTERVAL_MS`] || "30000")),
  };
}
