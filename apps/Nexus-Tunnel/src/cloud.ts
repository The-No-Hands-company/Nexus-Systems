import { buildSystemsApiRegistrationPayload } from "./contracts";
import { listRoutes, registerRoute, setRouteEnabled } from "./state";

type CloudTool = {
  id: string;
  upstreamUrl?: string;
  health?: string;
  exposed?: boolean;
  registrationStatus?: string;
};

function cloudBaseUrl(): string {
  const raw = (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim();
  return raw.replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function heartbeatIntervalMs(): number {
  const raw = Number(process.env.NEXUS_TUNNEL_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function reconcileIntervalMs(): number {
  const raw = Number(process.env.NEXUS_TUNNEL_CLOUD_RECONCILE_INTERVAL_MS || "15000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 15000;
}

function cloudIntegrationEnabled(): boolean {
  const flag = (process.env.NEXUS_TUNNEL_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase();
  return flag !== "false" && flag !== "0" && flag !== "off";
}

export async function registerNexusTunnelWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(payload),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Tunnel registration failed with status ${response.status}`);
  }
}

export async function heartbeatNexusTunnelWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-tunnel")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Tunnel heartbeat failed with status ${response.status}`);
  }
}

async function listToolsFromCloud(): Promise<CloudTool[]> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "GET",
    headers: cloudHeaders(),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Tunnel reconcile tools failed with status ${response.status}`);
  }

  const data = (await response.json()) as { tools?: CloudTool[] };
  return Array.isArray(data.tools) ? data.tools : [];
}

async function evaluateToolTrust(toolId: string): Promise<boolean> {
  try {
    const response = await fetch(`${cloudBaseUrl()}/api/v1/guardian/service/${encodeURIComponent(toolId)}`, {
      method: "GET",
      headers: cloudHeaders(),
    });

    if (response.status === 404) return true;
    if (!response.ok) return true;

    const data = (await response.json()) as { decision?: { verdict?: string } };
    const verdict = data.decision?.verdict;
    return verdict !== "deny" && verdict !== "suspend" && verdict !== "quarantine";
  } catch {
    // Treat Cloud trust lookup errors as non-authoritative and avoid hard disabling.
    return true;
  }
}

function isToolHealthyForRouting(tool: CloudTool): boolean {
  const health = (tool.health || "").toLowerCase();
  const status = (tool.registrationStatus || "").toLowerCase();
  if (status === "offline") return false;
  return health !== "offline" && health !== "degraded";
}

export async function reconcileNexusTunnelRoutesFromCloud(): Promise<void> {
  const tools = await listToolsFromCloud();
  const cloudToolIds = new Set<string>();

  for (const tool of tools) {
    if (!tool?.id || tool.id === "nexus-tunnel") continue;
    cloudToolIds.add(tool.id);

    if (!tool.upstreamUrl) {
      setRouteEnabled(tool.id, false);
      continue;
    }

    const exposureMode = tool.exposed ? "public" : "internal";
    registerRoute(tool.id, tool.upstreamUrl, exposureMode);

    const healthyForRouting = isToolHealthyForRouting(tool);
    const trustedForRouting = await evaluateToolTrust(tool.id);
    setRouteEnabled(tool.id, healthyForRouting && trustedForRouting);
  }

  for (const route of listRoutes()) {
    if (!cloudToolIds.has(route.toolId)) {
      setRouteEnabled(route.toolId, false);
    }
  }
}

/**
 * Query Cloud Guardian module for exposure approval decision.
 * Guardian scope: "service", subject: toolId
 */
export async function requestGuardianApprovalForExposure(
  toolId: string,
  exposureMode: string,
): Promise<{ approved: boolean; reason?: string }> {
  const cloudUrl = cloudBaseUrl();

  try {
    const response = await fetch(`${cloudUrl}/api/v1/guardian/service/${encodeURIComponent(toolId)}`, {
      method: "GET",
      headers: cloudHeaders(),
    });

    if (response.ok) {
      const data = (await response.json()) as { decision?: { verdict?: string; reason?: string } };
      const verdict = data.decision?.verdict;
      return {
        approved: verdict === "approve",
        reason: data.decision?.reason || `Guardian verdict: ${verdict}`,
      };
    }
  } catch (error) {
    console.warn(`[nexus-tunnel] Guardian query failed: ${(error as Error).message}`);
  }

  // Default to deny on error for security
  return { approved: false, reason: "Guardian unavailable — denying for security" };
}

export function startNexusTunnelCloudRegistrationHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) {
    return () => {};
  }

  registerNexusTunnelWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-tunnel] Cloud registration failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusTunnelWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-tunnel] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}

export function startNexusTunnelCloudReconciliationLoop(): () => void {
  if (!cloudIntegrationEnabled()) {
    return () => {};
  }

  reconcileNexusTunnelRoutesFromCloud().catch((error) => {
    console.warn(`[nexus-tunnel] Cloud reconcile failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    reconcileNexusTunnelRoutesFromCloud().catch((error) => {
      console.warn(`[nexus-tunnel] Cloud reconcile failed: ${(error as Error).message}`);
    });
  }, reconcileIntervalMs());

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}
