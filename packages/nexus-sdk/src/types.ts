export interface NexusServiceConfig {
  id: string;
  name: string;
  description: string;
  port: number;
  baseUrl: string;
  capabilities: string[];
  cloudUrl: string;
  cloudApiKey: string | undefined;
  guardianUrl?: string;
  authUrl?: string;
  monitorUrl?: string;
  tunnelUrl?: string;
  searchUrl?: string;
  enableCloudIntegration: boolean;
  enableMonitorIntegration: boolean;
  enableGuardianIntegration: boolean;
  heartbeatIntervalMs: number;
}

export interface SystemsApiRegistrationPayload {
  id: string;
  name: string;
  description: string;
  mode: "orchestrated" | "standalone";
  exposed: boolean;
  health: "healthy" | "degraded" | "offline";
  upstreamUrl: string;
  capabilities: string[];
  metadata: Record<string, unknown>;
}

export interface MonitorHeartbeat {
  kind: "heartbeat";
  source: string;
  state: "healthy" | "degraded" | "offline";
  timestamp: string;
  endpoint?: string;
}

export interface MonitorMetric {
  kind: "metric";
  source: string;
  name: string;
  value: number;
  tags?: Record<string, string>;
  timestamp: string;
}

export interface GuardianEvaluation {
  scope: string;
  subjectId: string;
  context: Record<string, string>;
}

export interface SearchDocument {
  title: string;
  content: string;
  source: string;
  url?: string;
  metadata?: Record<string, string>;
}
