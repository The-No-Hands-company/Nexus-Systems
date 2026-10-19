import { buildSystemsApiRegistrationPayload } from "./contracts";

function cloudBaseUrl(): string {
  return (process.env.NEXUS_CLOUD_URL || "http://localhost:8787").trim().replace(/\/$/, "");
}
function cloudHeaders(): Record<string, string> {
  return {
    "content-type": "application/json",
    ...(process.env.NEXUS_CLOUD_API_KEY ? { "x-api-key": process.env.NEXUS_CLOUD_API_KEY } : {}),
  };
}
function hbMs(): number {
  return Math.max(5000, Number(process.env.NEXUS_API_CLOUD_HEARTBEAT_INTERVAL_MS || "30000"));
}
function enabled(): boolean {
  const configured = process.env.NEXUS_API_ENABLE_CLOUD_INTEGRATION?.trim().toLowerCase();
  if (configured !== undefined) return configured !== "false";
  return process.env.NODE_ENV !== "test";
}

export async function registerNexusApiWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl)),
  });
  if (!r.ok) throw new Error(`Nexus-API reg failed: ${r.status}`);
}
export async function heartbeatNexusApiWithCloud(baseUrl: string): Promise<void> {
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools/nexus-api/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health: "healthy", upstreamUrl: baseUrl }),
  });
  if (!r.ok) throw new Error(`Nexus-API hb failed: ${r.status}`);
}
export function startNexusApiHeartbeat(baseUrl: string): () => void {
  if (!enabled()) return () => {};
  registerNexusApiWithCloud(baseUrl).catch((e) =>
    console.warn(`[nexus-api] Cloud reg failed: ${(e as Error).message}`),
  );
  const t = setInterval(() => {
    heartbeatNexusApiWithCloud(baseUrl).catch((e) =>
      console.warn(`[nexus-api] HB failed: ${(e as Error).message}`),
    );
  }, hbMs()) as unknown as { unref?: () => void };
  if (typeof t.unref === "function") t.unref();
  return () => clearInterval(t as unknown as number);
}
