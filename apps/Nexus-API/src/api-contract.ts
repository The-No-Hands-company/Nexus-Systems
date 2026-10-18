export type ApiContractRoute = {
  method: "GET" | "POST" | "DELETE";
  path: string;
  access: "public" | "operator";
  description: string;
};

export type NexusApiContract = {
  protocol: "nexus-api/1";
  service: {
    id: "nexus-api";
    name: "Nexus API";
    version: string;
    baseUrl: string;
    auth: "Bearer token or hosted session";
  };
  routes: readonly ApiContractRoute[];
};

const ROUTES: readonly ApiContractRoute[] = [
  { method: "GET", path: "/health", access: "public", description: "Liveness and gateway health" },
  { method: "GET", path: "/api/v1/contract", access: "public", description: "Versioned Nexus API contract" },
  { method: "GET", path: "/api/v1/gateway/status", access: "public", description: "Gateway counters and uptime" },
  { method: "GET", path: "/api/v1/gateway/routes", access: "operator", description: "List configured proxy routes" },
  { method: "POST", path: "/api/v1/gateway/routes", access: "operator", description: "Create or replace a proxy route" },
  { method: "DELETE", path: "/api/v1/gateway/routes/:method/:path", access: "operator", description: "Remove a proxy route" },
];

export function buildApiContract(baseUrl: string): NexusApiContract {
  return {
    protocol: "nexus-api/1",
    service: {
      id: "nexus-api",
      name: "Nexus API",
      version: "v2",
      baseUrl,
      auth: "Bearer token or hosted session",
    },
    routes: ROUTES,
  };
}
