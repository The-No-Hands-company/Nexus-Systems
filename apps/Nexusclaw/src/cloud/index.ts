import type { AnyClawConfig } from "../config/index.js";

export interface NexusCloudRegistrationResult {
  id?: string;
  status?: string;
  message?: string;
  [key: string]: unknown;
}

export function buildNexusCloudManifest(config: AnyClawConfig, runtime: { gatewayUrl: string }) {
  const apiEndpoint = runtime.gatewayUrl.replace(/\/+$/, "");
  return {
    id: config.cloud.service_id,
    name: config.cloud.name,
    description: config.cloud.description,
    version: config.cloud.version,
    endpoint: apiEndpoint,
    health: {
      live: `${apiEndpoint}/api/health/live`,
      ready: `${apiEndpoint}/api/health/ready`,
      status: `${apiEndpoint}/api/status`,
    },
    capabilities: config.cloud.capabilities,
    metadata: {
      gatewayUrl: apiEndpoint,
      cloudAutoRegister: config.cloud.auto_register,
    },
  };
}

export function validateNexusCloudManifest(manifest: Record<string, unknown>): string[] {
  const errors: string[] = [];
  if (!manifest.id || typeof manifest.id !== "string") {
    errors.push("Missing or invalid manifest id");
  }
  if (!manifest.name || typeof manifest.name !== "string") {
    errors.push("Missing or invalid manifest name");
  }
  if (!manifest.endpoint || typeof manifest.endpoint !== "string") {
    errors.push("Missing or invalid manifest endpoint");
  }
  if (!manifest.health || typeof manifest.health !== "object") {
    errors.push("Missing or invalid health object");
  } else {
    const health = manifest.health as Record<string, unknown>;
    if (!health.live || typeof health.live !== "string") {
      errors.push("Missing or invalid health.live URL");
    }
    if (!health.ready || typeof health.ready !== "string") {
      errors.push("Missing or invalid health.ready URL");
    }
    if (!health.status || typeof health.status !== "string") {
      errors.push("Missing or invalid health.status URL");
    }
  }
  if (!Array.isArray(manifest.capabilities) || manifest.capabilities.some((c) => typeof c !== "string")) {
    errors.push("Missing or invalid capabilities array");
  }
  return errors;
}

export async function deregisterFromNexusCloud(
  config: AnyClawConfig,
  runtime: { gatewayUrl: string },
): Promise<NexusCloudRegistrationResult | null> {
  if (!config.cloud.enabled) return null;
  if (!config.cloud.endpoint) {
    throw new Error("Nexus Cloud deregistration is enabled, but cloud.endpoint is not configured.");
  }

  const toolId = config.cloud.service_id;
  const baseUrl = config.cloud.endpoint.endsWith("/api/v1/tools")
    ? config.cloud.endpoint.replace(/\/api\/v1\/tools\/?$/, "")
    : config.cloud.endpoint.replace(/\/+$/, "");
  const deregisterUrl = `${baseUrl}/api/v1/tools/${encodeURIComponent(toolId)}`;

  const headers: Record<string, string> = {
    "content-type": "application/json",
  };
  if (config.cloud.api_key) {
    headers.authorization = `Bearer ${config.cloud.api_key}`;
  }

  const response = await fetch(deregisterUrl, {
    method: "DELETE",
    headers,
  });

  if (!response.ok) {
    const body = await response.text();
    throw new Error(`Nexus Cloud deregistration failed: ${response.status} ${response.statusText} ${body}`);
  }

  return { status: "deregistered", id: toolId };
}

/**
 * Send a heartbeat PATCH to keep the Nexus Cloud registration alive.
 * Returns `null` when cloud is disabled or not configured.
 */
export async function sendNexusCloudHeartbeat(
  config: AnyClawConfig,
  runtime: { gatewayUrl: string },
): Promise<NexusCloudRegistrationResult | null> {
  if (!config.cloud.enabled) return null;
  if (!config.cloud.endpoint) return null;

  const toolId = config.cloud.service_id;
  const baseUrl = config.cloud.endpoint.endsWith("/api/v1/tools")
    ? config.cloud.endpoint.replace(/\/api\/v1\/tools\/?$/, "")
    : config.cloud.endpoint.replace(/\/+$/, "");
  const heartbeatUrl = `${baseUrl}/api/v1/tools/${encodeURIComponent(toolId)}/heartbeat`;

  const headers: Record<string, string> = { "content-type": "application/json" };
  if (config.cloud.api_key) {
    headers.authorization = `Bearer ${config.cloud.api_key}`;
  }

  const manifest = buildNexusCloudManifest(config, runtime);
  const response = await fetch(heartbeatUrl, {
    method: "PATCH",
    headers,
    body: JSON.stringify({ endpoint: manifest.endpoint, health: manifest.health }),
  });

  if (!response.ok) {
    const body = await response.text();
    throw new Error(`Nexus Cloud heartbeat failed: ${response.status} ${response.statusText} ${body}`);
  }

  return (await response.json()) as NexusCloudRegistrationResult;
}

/**
 * Start a periodic heartbeat to Nexus Cloud.
 * Returns a `stop()` function that cancels the interval.
 */
export function startNexusCloudHeartbeat(
  config: AnyClawConfig,
  runtime: { gatewayUrl: string },
  intervalMs = config.cloud.heartbeat_interval_seconds * 1000,
  onError?: (err: Error) => void,
): () => void {
  const timer = setInterval(async () => {
    try {
      await sendNexusCloudHeartbeat(config, runtime);
    } catch (err) {
      onError?.(err as Error);
    }
  }, intervalMs);
  timer.unref(); // don't keep process alive for heartbeat alone
  return () => clearInterval(timer);
}

export async function registerWithNexusCloud(
  config: AnyClawConfig,
  runtime: { gatewayUrl: string },
): Promise<NexusCloudRegistrationResult | null> {
  if (!config.cloud.enabled) return null;
  if (!config.cloud.endpoint) {
    throw new Error("Nexus Cloud registration is enabled, but cloud.endpoint is not configured.");
  }

  const registrationUrl = config.cloud.endpoint.endsWith("/api/v1/tools")
    ? config.cloud.endpoint
    : `${config.cloud.endpoint.replace(/\/+$/, "")}/api/v1/tools`;

  const manifest = buildNexusCloudManifest(config, runtime);
  const headers: Record<string, string> = {
    "content-type": "application/json",
  };

  if (config.cloud.api_key) {
    headers.authorization = `Bearer ${config.cloud.api_key}`;
  }

  const response = await fetch(registrationUrl, {
    method: "POST",
    headers,
    body: JSON.stringify(manifest),
  });

  if (!response.ok) {
    const body = await response.text();
    throw new Error(`Nexus Cloud registration failed: ${response.status} ${response.statusText} ${body}`);
  }

  return (await response.json()) as NexusCloudRegistrationResult;
}
