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
  const raw = Number(process.env.NEXUS_GUARDIAN_CLOUD_HEARTBEAT_INTERVAL_MS || "30000");
  return Number.isFinite(raw) ? Math.max(5000, raw) : 30000;
}

function cloudIntegrationEnabled(): boolean {
  const flag = (process.env.NEXUS_GUARDIAN_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase();
  return flag !== "false" && flag !== "0" && flag !== "off";
}

export async function registerNexusGuardianWithCloud(baseUrl: string): Promise<void> {
  const payload = buildSystemsApiRegistrationPayload(baseUrl);
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(payload),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Guardian registration failed with status ${response.status}`);
  }
}

export async function heartbeatNexusGuardianWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent("nexus-guardian")}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });

  if (!response.ok) {
    throw new Error(`Nexus-Guardian heartbeat failed with status ${response.status}`);
  }
}

export function startNexusGuardianCloudRegistrationHeartbeat(baseUrl: string): () => void {
  if (!cloudIntegrationEnabled()) {
    return () => {};
  }

  registerNexusGuardianWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-guardian] Cloud registration failed: ${(error as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatNexusGuardianWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-guardian] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatIntervalMs());

  if (typeof timer.unref === "function") {
    timer.unref();
  }

  return () => clearInterval(timer);
}
