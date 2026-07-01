import type {
  NexusServiceConfig,
  SystemsApiRegistrationPayload,
  MonitorHeartbeat,
  MonitorMetric,
  GuardianEvaluation,
  SearchDocument,
} from "./types";

function jsonHeaders(apiKey?: string): Record<string, string> {
  return {
    "content-type": "application/json",
    accept: "application/json",
    ...(apiKey ? { "x-api-key": apiKey } : {}),
  };
}

export class NexusClient {
  private config: NexusServiceConfig;
  private heartbeatTimer: ReturnType<typeof setInterval> | undefined;
  private monitorTimer: ReturnType<typeof setInterval> | undefined;

  constructor(config: NexusServiceConfig) {
    this.config = config;
  }

  // ── Cloud Integration ──

  buildRegistrationPayload(): SystemsApiRegistrationPayload {
    return {
      id: this.config.id,
      name: this.config.name,
      description: this.config.description,
      mode: "orchestrated",
      exposed: false,
      health: "healthy",
      upstreamUrl: this.config.baseUrl,
      capabilities: this.config.capabilities,
      metadata: {
        version: "v1",
        defaultPort: this.config.port,
      },
    };
  }

  async registerWithCloud(): Promise<boolean> {
    if (!this.config.enableCloudIntegration) return false;
    try {
      const r = await fetch(`${this.config.cloudUrl}/api/v1/tools`, {
        method: "POST",
        headers: jsonHeaders(this.config.cloudApiKey),
        body: JSON.stringify(this.buildRegistrationPayload()),
      });
      return r.ok;
    } catch (e) {
      console.warn(`[${this.config.id}] Cloud registration failed: ${(e as Error).message}`);
      return false;
    }
  }

  async heartbeatWithCloud(): Promise<boolean> {
    if (!this.config.enableCloudIntegration) return false;
    try {
      const r = await fetch(`${this.config.cloudUrl}/api/v1/tools/${encodeURIComponent(this.config.id)}/heartbeat`, {
        method: "POST",
        headers: jsonHeaders(this.config.cloudApiKey),
        body: JSON.stringify({ health: "healthy", upstreamUrl: this.config.baseUrl }),
      });
      return r.ok;
    } catch {
      return false;
    }
  }

  startCloudHeartbeat(): () => void {
    if (!this.config.enableCloudIntegration) return () => {};

    this.registerWithCloud();

    this.heartbeatTimer = setInterval(() => {
      this.heartbeatWithCloud();
    }, this.config.heartbeatIntervalMs);

    if (typeof this.heartbeatTimer.unref === "function") this.heartbeatTimer.unref();
    return () => { if (this.heartbeatTimer) clearInterval(this.heartbeatTimer); };
  }

  // ── Monitor Integration ──

  async sendHeartbeatToMonitor(health?: string): Promise<boolean> {
    if (!this.config.enableMonitorIntegration || !this.config.monitorUrl) return false;
    try {
      const payload: MonitorHeartbeat = {
        kind: "heartbeat",
        source: this.config.id,
        state: (health as MonitorHeartbeat["state"]) || "healthy",
        timestamp: new Date().toISOString(),
        endpoint: this.config.baseUrl,
      };
      const r = await fetch(`${this.config.monitorUrl}/api/v1/monitor/ingest`, {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify(payload),
      });
      return r.ok;
    } catch {
      return false;
    }
  }

  async sendMetricToMonitor(name: string, value: number, tags?: Record<string, string>): Promise<boolean> {
    if (!this.config.enableMonitorIntegration || !this.config.monitorUrl) return false;
    try {
      const payload: MonitorMetric = {
        kind: "metric",
        source: this.config.id,
        name,
        value,
        tags: tags || {},
        timestamp: new Date().toISOString(),
      };
      const r = await fetch(`${this.config.monitorUrl}/api/v1/monitor/ingest`, {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify(payload),
      });
      return r.ok;
    } catch {
      return false;
    }
  }

  startMonitorHeartbeat(intervalMs = 30000): () => void {
    if (!this.config.enableMonitorIntegration || !this.config.monitorUrl) return () => {};

    this.sendHeartbeatToMonitor();
    this.sendMetricToMonitor("startup", 1);

    this.monitorTimer = setInterval(() => {
      this.sendHeartbeatToMonitor();
      this.sendMetricToMonitor("uptime_seconds", Math.floor(process.uptime()), { unit: "seconds" });
    }, intervalMs);

    if (typeof this.monitorTimer.unref === "function") this.monitorTimer.unref();
    return () => { if (this.monitorTimer) clearInterval(this.monitorTimer); };
  }

  // ── Guardian Integration ──

  async evaluateWithGuardian(evaluation: GuardianEvaluation): Promise<{
    verdict: string;
    reason: string;
    decisionId?: string;
    rateLimited?: boolean;
  } | null> {
    if (!this.config.enableGuardianIntegration || !this.config.guardianUrl) return null;
    try {
      const r = await fetch(`${this.config.guardianUrl}/api/v1/guardian/evaluate`, {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify(evaluation),
      });
      if (!r.ok) return null;
      const data = await r.json() as {
        evaluation: { verdict: string; reason: string };
        decision: { id: string };
        rateLimit: { throttled: boolean } | null;
      };
      return {
        verdict: data.evaluation.verdict,
        reason: data.evaluation.reason,
        decisionId: data.decision?.id,
        rateLimited: data.rateLimit?.throttled || false,
      };
    } catch {
      return null;
    }
  }

  async respondToThreat(toolId: string, severity: string, description: string): Promise<{
    recommendedAction: string;
    reason: string;
  } | null> {
    if (!this.config.enableGuardianIntegration || !this.config.guardianUrl) return null;
    try {
      const r = await fetch(`${this.config.guardianUrl}/api/v1/guardian/threat/respond`, {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ toolId, threatType: "security", severity, description }),
      });
      if (!r.ok) return null;
      const data = await r.json() as { response: { recommendedAction: string; reason: string } };
      return data.response;
    } catch {
      return null;
    }
  }

  // ── Auth Integration ──

  async validateToken(token: string): Promise<{ userId: string; username: string | undefined } | null> {
    if (!this.config.authUrl) return null;
    try {
      const r = await fetch(`${this.config.authUrl}/api/v1/auth/trust`, {
        headers: { authorization: `Bearer ${token}`, accept: "application/json" },
        signal: AbortSignal.timeout(3000),
      });
      if (!r.ok) return null;
      const data = await r.json() as { trusted: boolean; userId?: string; user?: { username: string } };
      return data.trusted ? { userId: data.userId || "", username: data.user?.username } : null;
    } catch {
      return null;
    }
  }

  // ── Tunnel Integration ──

  async registerRouteWithTunnel(host: string, upstreamUrl: string): Promise<boolean> {
    if (!this.config.tunnelUrl) return false;
    try {
      const r = await fetch(`${this.config.tunnelUrl}/api/v1/tunnel/routes`, {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ toolId: host, targetUrl: upstreamUrl, exposureMode: "internal" }),
      });
      return r.ok;
    } catch {
      return false;
    }
  }

  // ── Search Integration ──

  async indexInSearch(doc: SearchDocument): Promise<boolean> {
    if (!this.config.searchUrl) return false;
    try {
      const r = await fetch(`${this.config.searchUrl}/api/v1/search/index`, {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify(doc),
      });
      return r.ok;
    } catch {
      return false;
    }
  }

  async batchIndexInSearch(docs: SearchDocument[]): Promise<number> {
    if (!this.config.searchUrl) return 0;
    try {
      const r = await fetch(`${this.config.searchUrl}/api/v1/search/index/batch`, {
        method: "POST",
        headers: jsonHeaders(),
        body: JSON.stringify({ documents: docs }),
      });
      if (!r.ok) return 0;
      const data = await r.json() as { indexed: number };
      return data.indexed;
    } catch {
      return 0;
    }
  }

  // ── Cleanup ──

  stop(): void {
    if (this.heartbeatTimer) clearInterval(this.heartbeatTimer);
    if (this.monitorTimer) clearInterval(this.monitorTimer);
  }
}
