import { buildSystemsApiRegistrationPayload } from "./contracts";

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
  const raw = Number(process.env.NEXUS_EDGE_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function cloudIntegrationEnabled(): boolean {
  const flag = (process.env.NEXUS_EDGE_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase();
  return flag !== "false" && flag !== "0" && flag !== "off";
}

export async function registerNexusEdgeWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(payload),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Edge registration failed with status ${response.status}`);
  }
}

export async function heartbeatNexusEdgeWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-edge")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Edge heartbeat failed with status ${response.status}`);
  }
}

/**
 * Query Cloud Guardian module for threat response recommendation.
 * Guardian scope: "agent", subject: toolId (for AI behavior guardrail decisions)
 */
export async function requestGuardianThreatResponse(
  toolId: string,
  threatDescription: string,
): Promise<{ recommended: string; reason?: string }> {
  const cloudUrl = cloudBaseUrl();

  try {
    const response = await fetch(`${cloudUrl}/api/v1/guardian/agent/${encodeURIComponent(toolId)}`, {
      method: "GET",
      headers: cloudHeaders(),
    });

    if (response.ok) {
      const data = (await response.json()) as { decision?: { verdict?: string; reason?: string } };
      const verdict = data.decision?.verdict;

      if (verdict === "approve") {
        return { recommended: "log", reason: "Guardian approved operation" };
      } else if (verdict === "deny") {
        return { recommended: "block", reason: "Guardian denied operation" };
      } else if (verdict === "suspend") {
        return { recommended: "isolate", reason: "Guardian recommends isolation" };
      }
    }
  } catch (error) {
    console.warn(`[nexus-edge] Guardian query failed: ${(error as Error).message}`);
  }

  // Default: log and alert on error
  return { recommended: "alert", reason: "Guardian unavailable — recommend alert" };
}

export function startNexusEdgeCloudRegistrationHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) {
    return () => {};
  }

  registerNexusEdgeWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-edge] Cloud registration failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusEdgeWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-edge] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}
