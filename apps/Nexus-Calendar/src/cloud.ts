import { buildSystemsApiRegistrationPayload, calendarToolId } from "./contracts";

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

function hbMs(): number {
  return Math.max(5000, Number(process.env["NEXUS_CALENDAR_CLOUD_HEARTBEAT_INTERVAL_MS"] || "30000"));
}

function enabled(): boolean {
  return (process.env["NEXUS_CALENDAR_ENABLE_CLOUD_INTEGRATION"] || "true").trim().toLowerCase() !== "false";
}

function healthTimeoutMs(): number {
  const value = Number(process.env.NEXUS_CALENDAR_HEALTH_TIMEOUT_MS || "2000");
  return Number.isFinite(value) && value > 0 ? Math.min(10000, Math.max(1, Math.floor(value))) : 2000;
}

async function observedHealth(baseUrl: string): Promise<"healthy" | "degraded"> {
  try {
    const response = await fetch(`${baseUrl.replace(/\/$/, "")}/health`, {
      headers: { accept: "application/json" },
      signal: AbortSignal.timeout(healthTimeoutMs()),
      redirect: "error",
    });
    if (!response.ok) return "degraded";
    const payload: unknown = await response.json();
    if (typeof payload !== "object" || payload === null || Array.isArray(payload)) return "degraded";
    const health = payload as Record<string, unknown>;
    return health.service === calendarToolId() && health.status === "ok" && health.ready === true
      ? "healthy" : "degraded";
  } catch {
    return "degraded";
  }
}

export async function registerWithCloud(baseUrl: string): Promise<void> {
  if (!enabled()) return;
  const health = await observedHealth(baseUrl);
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify(buildSystemsApiRegistrationPayload(baseUrl, health)),
  });
  if (!r.ok) throw new Error(`Nexus-Calendar registration failed: ${r.status}`);
}

export async function heartbeatWithCloud(baseUrl: string): Promise<void> {
  if (!enabled()) return;
  const health = await observedHealth(baseUrl);
  const r = await fetch(`${cloudBaseUrl()}/api/v1/tools/${encodeURIComponent(calendarToolId())}/heartbeat`, {
    method: "POST",
    headers: cloudHeaders(),
    body: JSON.stringify({ health, upstreamUrl: baseUrl }),
  });
  if (!r.ok) throw new Error(`Nexus-Calendar heartbeat failed: ${r.status}`);
}

export function startHeartbeat(baseUrl: string): () => void {
  if (!enabled()) return () => {};

  registerWithCloud(baseUrl).catch((e) => {
    console.warn(`[nexus-calendar] Cloud registration failed: ${(e as Error).message}`);
  });

  const timer = setInterval(() => {
    heartbeatWithCloud(baseUrl).catch((e) => {
      console.warn(`[nexus-calendar] Cloud heartbeat failed: ${(e as Error).message}`);
    });
  }, hbMs());

  if (typeof timer.unref === "function") timer.unref();
  return () => clearInterval(timer);
}
