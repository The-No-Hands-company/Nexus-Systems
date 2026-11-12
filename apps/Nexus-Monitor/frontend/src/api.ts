export interface ServiceStatus {
  status: "healthy" | "degraded" | "unhealthy" | "unknown";
  lastHeartbeat: string;
}

export interface DashboardOverview {
  servicesTotal: number;
  servicesHealthy: number;
  servicesDegraded: number;
  servicesUnhealthy: number;
  eventsReceived: number;
  heartbeatsReceived: number;
  metricsStored: number;
  activeAlerts: number;
  alertRulesDefined: number;
}

export interface ActiveAlert {
  ruleId: string;
  firedAt: string;
  lastValue: number;
  acknowledged: boolean;
  rule: AlertRule;
}

export interface DashboardData {
  timestamp: string;
  uptimeSeconds: number;
  overview: DashboardOverview;
  recentAlerts: ActiveAlert[];
  serviceStatuses: Record<string, ServiceStatus>;
}

export interface MetricPoint {
  name: string;
  value: number;
  tags: Record<string, string>;
  timestamp: string;
  source: string;
}

export interface AlertRule {
  id: string;
  metric: string;
  condition: "gt" | "lt" | "gte" | "lte";
  threshold: number;
  severity: "info" | "warning" | "critical";
  cooldownSeconds: number;
  createdAt: string;
}

export async function fetchDashboard(): Promise<DashboardData> {
  const r = await fetch("/api/v1/monitor/dashboard");
  if (!r.ok) throw new Error(`dashboard ${r.status}`);
  return r.json() as Promise<DashboardData>;
}

export async function fetchMetrics(limit = 100): Promise<MetricPoint[]> {
  const r = await fetch(`/api/v1/monitor/metrics?limit=${limit}`);
  if (!r.ok) throw new Error(`metrics ${r.status}`);
  return r.json() as Promise<MetricPoint[]>;
}

export async function fetchAlertRules(): Promise<AlertRule[]> {
  const r = await fetch("/api/v1/monitor/alerts/rules");
  if (!r.ok) throw new Error(`rules ${r.status}`);
  return r.json() as Promise<AlertRule[]>;
}

export async function acknowledgeAlert(alertId: string): Promise<void> {
  await fetch(`/api/v1/monitor/alerts/${alertId}/acknowledge`, { method: "POST" });
}

export function formatUptime(seconds: number): string {
  const d = Math.floor(seconds / 86400);
  const h = Math.floor((seconds % 86400) / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  if (d > 0) return `${d}d ${h}h`;
  if (h > 0) return `${h}h ${m}m`;
  return `${m}m ${seconds % 60}s`;
}

export function timeAgo(iso: string): string {
  const diff = (Date.now() - new Date(iso).getTime()) / 1000;
  if (diff < 60) return `${Math.floor(diff)}s ago`;
  if (diff < 3600) return `${Math.floor(diff / 60)}m ago`;
  if (diff < 86400) return `${Math.floor(diff / 3600)}h ago`;
  return `${Math.floor(diff / 86400)}d ago`;
}
