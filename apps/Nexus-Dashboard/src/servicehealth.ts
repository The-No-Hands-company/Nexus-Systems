/**
 * Aggregated health status for every service this node runs.
 *
 * Checks each loopback endpoint in parallel with a short timeout. A service
 * that answers anything at all is "up" (even a 404 proves the process is
 * alive); only connection refused / timeout means "down".
 */

type ServiceCheck = {
  name: string;
  url: string;
  /** What this service does, shown as context in the UI. */
  description: string;
};

const SERVICES: ServiceCheck[] = [
  { name: "auth", url: "http://127.0.0.1:4310/health", description: "Identity provider" },
  { name: "cloud", url: "http://127.0.0.1:8787/health", description: "Control plane" },
  { name: "api", url: "http://127.0.0.1:3150/api/health/live", description: "Unified API" },
  { name: "chat-gateway", url: "http://127.0.0.1:8180/api/v1/health", description: "nexus-chat REST" },
  { name: "terminal", url: "http://127.0.0.1:3110/health", description: "Shell service" },
  { name: "dashboard", url: "http://127.0.0.1:3132/health", description: "Ecosystem front door" },
  { name: "draw", url: "http://127.0.0.1:3075/health", description: "Draw API" },
  { name: "mailapi", url: "http://127.0.0.1:3140/", description: "Mail store" },
];

export type ServiceStatus = {
  name: string;
  description: string;
  healthy: boolean;
  latencyMs: number | null;
  detail?: string;
};

export async function checkAllServices(): Promise<ServiceStatus[]> {
  const results = await Promise.allSettled(
    SERVICES.map(async (svc) => {
      const start = Date.now();
      const res = await fetch(svc.url, { signal: AbortSignal.timeout(3000) });
      return {
        name: svc.name,
        description: svc.description,
        healthy: res.status < 500,
        latencyMs: Date.now() - start,
      };
    }),
  );

  return SERVICES.map((svc, i) => {
    const result = results[i];
    if (result.status === "fulfilled") return result.value;
    return {
      name: svc.name,
      description: svc.description,
      healthy: false,
      latencyMs: null,
      detail: String(result.reason?.cause?.code ?? result.reason?.message ?? "unreachable").slice(0, 80),
    };
  });
}
