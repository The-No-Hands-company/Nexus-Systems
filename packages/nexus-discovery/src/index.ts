/**
 * Nexus Discovery — service-to-service resolution via Nexus-Cloud topology.
 *
 * Every app imports this instead of hardcoding other apps' URLs.
 *
 * Usage:
 *   import { NexusDiscovery } from "@nexus/discovery";
 *   const discovery = new NexusDiscovery({ cloudUrl: "http://localhost:8787" });
 *
 *   const svc = await discovery.resolve("nexus-photos");
 *   // → { id: "nexus-photos", url: "http://localhost:3096", health: "healthy" }
 *
 *   const data = await discovery.call("nexus-photos", "/api/v1/photos/albums");
 */

export interface ServiceInfo {
  id: string;
  url: string;
  health: "healthy" | "degraded" | "offline" | "unknown";
  capabilities: string[];
  lastSeen: number;
}

export interface DiscoveryConfig {
  cloudUrl: string;
  apiKey?: string | undefined;
  ttlMs?: number | undefined;
  retryMs?: number | undefined;
}

interface TopologyResponse {
  topology: {
    tools: Array<{
      id: string;
      name: string;
      upstreamUrl?: string;
      health?: string;
      capabilities?: string[];
    }>;
  };
}

export class NexusDiscovery {
  private cloudUrl: string;
  private apiKey: string | undefined;
  private ttlMs: number;
  private retryMs: number;
  private cache: Map<string, ServiceInfo> = new Map();
  private lastFetch = 0;
  private fetchPromise: Promise<void> | null = null;

  constructor(config: DiscoveryConfig) {
    this.cloudUrl = config.cloudUrl.replace(/\/$/, "");
    this.apiKey = config.apiKey;
    this.ttlMs = config.ttlMs ?? 30_000;
    this.retryMs = config.retryMs ?? 5_000;
  }

  /** Resolve a service by its registered ID (e.g. "nexus-photos"). */
  async resolve(id: string): Promise<ServiceInfo | null> {
    await this.ensureCache();
    return this.cache.get(id) ?? null;
  }

  /** List all registered services. */
  async list(): Promise<ServiceInfo[]> {
    await this.ensureCache();
    return [...this.cache.values()].sort((a, b) => a.id.localeCompare(b.id));
  }

  /** Convenience: resolve a service then call its API. */
  async call(
    serviceId: string,
    path: string,
    init?: RequestInit,
  ): Promise<Response | null> {
    const svc = await this.resolve(serviceId);
    if (!svc) return null;
    const url = `${svc.url.replace(/\/$/, "")}${path}`;
    return fetch(url, init);
  }

  /** Force refresh the topology cache. */
  async refresh(): Promise<void> {
    this.lastFetch = 0;
    await this.fetchTopology();
  }

  // ── Internal ────────────────────────────────────────────────

  private async ensureCache(): Promise<void> {
    const age = Date.now() - this.lastFetch;
    if (age < this.ttlMs && this.cache.size > 0) return;

    // Deduplicate concurrent fetch calls
    if (this.fetchPromise) {
      await this.fetchPromise;
      return;
    }

    this.fetchPromise = this.fetchTopology();
    await this.fetchPromise;
    this.fetchPromise = null;
  }

  private async fetchTopology(): Promise<void> {
    try {
      const headers: Record<string, string> = {
        accept: "application/json",
      };
      if (this.apiKey) {
        headers["x-api-key"] = this.apiKey;
      }

      const res = await fetch(`${this.cloudUrl}/api/v1/topology`, { headers });
      if (!res.ok) throw new Error(`Cloud returned ${res.status}`);

      const data = await res.json() as TopologyResponse;
      const tools: Array<{
        id: string;
        name: string;
        upstreamUrl?: string;
        health?: string;
        capabilities?: string[];
      }> = data.topology?.tools ?? [];

      // Also try /api/v1/tools as fallback (different response shape)
      let toolList = tools;
      if (toolList.length === 0) {
        const fallback = await fetch(`${this.cloudUrl}/api/v1/tools`, { headers });
        if (fallback.ok) {
          const fb = await fallback.json() as { tools?: typeof toolList };
          toolList = fb.tools ?? [];
        }
      }

      this.cache.clear();
      for (const tool of toolList) {
        const url = tool.upstreamUrl ?? `${this.cloudUrl}`;
        this.cache.set(tool.id, {
          id: tool.id,
          url,
          health: (tool.health as ServiceInfo["health"]) ?? "unknown",
          capabilities: tool.capabilities ?? [],
          lastSeen: Date.now(),
        });
      }

      this.lastFetch = Date.now();
    } catch (err) {
      // If Cloud is unreachable, keep using cached data
      if (this.cache.size === 0) {
        console.warn(`[discovery] Cloud unreachable, no cache: ${err instanceof Error ? err.message : err}`);
      }
      // Retry sooner next time
      this.lastFetch = Date.now() - this.ttlMs + this.retryMs;
    }
  }
}
