import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}

function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}

function heartbeatMs(): number {
  return Math.max(5000, Number(process.env.NEXUS_PROJECT_CLOUD_HEARTBEAT_INTERVAL_MS || "30000"));
}

function enabled(): boolean {
  return (process.env.NEXUS_PROJECT_ENABLE_CLOUD_INTEGRATION || "true").trim().toLowerCase() !== "false";
}

export async function registerWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)),
  });
  if (!response.ok) throw new Error(`Nexus-Project registration failed: ${response.status}`);
}

export async function heartbeatWithCloud(baseUrl: string): Promise<void> {
  const response = await fetch(`${cloudBaseUrl()}/api/v1/tools/nexus-project/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!response.ok) throw new Error(`Nexus-Project heartbeat failed: ${response.status}`);
}

export function startHeartbeat(baseUrl: string): () => void {
  if (!enabled()) return () => {};
  registerWithCloud(baseUrl).catch((error) => {
    console.warn(`[nexus-project] Cloud registration failed: ${(error as Error).message}`);
  });
  const timer = setInterval(() => {
    heartbeatWithCloud(baseUrl).catch((error) => {
      console.warn(`[nexus-project] Cloud heartbeat failed: ${(error as Error).message}`);
    });
  }, heartbeatMs());
  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
