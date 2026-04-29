import type { TunnelRoute, TunnelExposure, TunnelDecision, RoutingPolicy } from "./types";
import { mkdirSync, readFileSync, renameSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

let routeCounter = 0;
let decisionCounter = 0;

const routes = new Map<string, TunnelRoute>();
const exposures = new Map<string, TunnelExposure>();
const decisions: TunnelDecision[] = [];

const stateFilePath = resolve(
  process.env.NEXUS_TUNNEL_STATE_PATH ||
    join(dirname(fileURLToPath(import.meta.url)), "..", "data", "tunnel-state.json"),
);

type PersistedTunnelState = {
  version: number;
  counters: {
    routeCounter: number;
    decisionCounter: number;
  };
  routes: TunnelRoute[];
  exposures: TunnelExposure[];
  decisions: TunnelDecision[];
  updatedAt: string;
};

const defaultPolicy: RoutingPolicy = {
  policyId: "default",
  description: "Default tunnel policy — require Guardian approval for public exposure",
  maxPublicExposures: 10,
  requireGuardianApproval: true,
  healthCheckIntervalMs: 30000,
};

function persistState(): void {
  const payload: PersistedTunnelState = {
    version: 1,
    counters: { routeCounter, decisionCounter },
    routes: Array.from(routes.values()),
    exposures: Array.from(exposures.values()),
    decisions: [...decisions],
    updatedAt: new Date().toISOString(),
  };

  try {
    mkdirSync(dirname(stateFilePath), { recursive: true });
    const tempPath = `${stateFilePath}.tmp`;
    writeFileSync(tempPath, JSON.stringify(payload, null, 2), "utf-8");
    renameSync(tempPath, stateFilePath);
  } catch (error) {
    try {
      rmSync(`${stateFilePath}.tmp`, { force: true });
    } catch {
      // best-effort cleanup only
    }
    console.warn(`[nexus-tunnel] Failed to persist state: ${(error as Error).message}`);
  }
}

function hydrateStateFromDisk(): void {
  try {
    const raw = readFileSync(stateFilePath, "utf-8");
    const parsed = JSON.parse(raw) as Partial<PersistedTunnelState>;

    routes.clear();
    exposures.clear();
    decisions.length = 0;

    for (const route of Array.isArray(parsed.routes) ? parsed.routes : []) {
      if (route && typeof route.toolId === "string") routes.set(route.toolId, route);
    }
    for (const exposure of Array.isArray(parsed.exposures) ? parsed.exposures : []) {
      if (exposure && typeof exposure.toolId === "string") exposures.set(exposure.toolId, exposure);
    }
    for (const decision of Array.isArray(parsed.decisions) ? parsed.decisions : []) {
      decisions.push(decision);
    }

    routeCounter = Math.max(0, Number(parsed.counters?.routeCounter || 0));
    decisionCounter = Math.max(0, Number(parsed.counters?.decisionCounter || 0));
  } catch {
    // First boot or malformed file: initialize an empty persisted state.
    persistState();
  }
}

function generateId(prefix: string): string {
  return `${prefix}-${Date.now()}-${++routeCounter}`;
}

export function registerRoute(
  toolId: string,
  targetUrl: string,
  exposureMode: "public" | "internal" | "protected" = "internal",
): TunnelRoute {
  const now = new Date().toISOString();
  const existing = routes.get(toolId);

  const route: TunnelRoute = {
    routeId: existing?.routeId ?? generateId("route"),
    toolId,
    targetUrl,
    exposureMode,
    enabled: true,
    createdAt: existing?.createdAt ?? now,
    updatedAt: now,
  };

  routes.set(toolId, route);
  persistState();
  return route;
}

export function getRoute(toolId: string): TunnelRoute | undefined {
  return routes.get(toolId);
}

export function setRouteEnabled(toolId: string, enabled: boolean): TunnelRoute | undefined {
  const route = routes.get(toolId);
  if (!route) return undefined;
  route.enabled = enabled;
  route.updatedAt = new Date().toISOString();
  persistState();
  return route;
}

export function listRoutes(): TunnelRoute[] {
  return Array.from(routes.values());
}

export function recordExposure(
  toolId: string,
  exposureMode: "public" | "internal" | "protected",
  guardianApproved: boolean,
  reason?: string,
): TunnelExposure {
  const now = new Date().toISOString();

  const exposure: TunnelExposure = {
    toolId,
    exposureMode,
    guardianApproved,
    requestedAt: now,
    approvedAt: guardianApproved ? now : undefined,
    approvalReason: guardianApproved ? reason : undefined,
    denialReason: !guardianApproved ? reason : undefined,
  };

  exposures.set(toolId, exposure);
  persistState();
  return exposure;
}

export function getExposure(toolId: string): TunnelExposure | undefined {
  return exposures.get(toolId);
}

export function listExposures(): TunnelExposure[] {
  return Array.from(exposures.values());
}

export function recordDecision(
  toolId: string,
  exposureMode: "public" | "internal" | "protected",
  approved: boolean,
  decidedBy = "tunnel",
  reason?: string,
): TunnelDecision {
  const decisionId = `dec-${Date.now()}-${++decisionCounter}`;
  const now = new Date().toISOString();

  const decision: TunnelDecision = {
    decisionId,
    toolId,
    exposureMode,
    approved,
    reason,
    decidedAt: now,
    decidedBy,
  };

  decisions.push(decision);
  persistState();
  return decision;
}

export function listDecisions(): TunnelDecision[] {
  return [...decisions];
}

export function getPolicy(): RoutingPolicy {
  return { ...defaultPolicy };
}

export function clearRoutes(): void {
  routes.clear();
  exposures.clear();
  decisions.length = 0;
  routeCounter = 0;
  decisionCounter = 0;
  persistState();
}

hydrateStateFromDisk();
