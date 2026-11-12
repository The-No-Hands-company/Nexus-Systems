import { useCallback, useEffect, useRef, useState } from "react";
import {
  fetchDashboard, fetchMetrics, fetchAlertRules,
  acknowledgeAlert,
  formatUptime, timeAgo,
  type DashboardData, type MetricPoint, type AlertRule,
} from "./api";

type Tab = "services" | "metrics" | "rules";

const STATUS_COLOR: Record<string, string> = {
  healthy: "var(--healthy)",
  degraded: "var(--degraded)",
  unhealthy: "var(--unhealthy)",
  unknown: "var(--muted)",
};

const SEVERITY_COLOR: Record<string, string> = {
  critical: "var(--unhealthy)",
  warning: "var(--degraded)",
  info: "var(--accent)",
};

function StatusDot({ status }: { status: string }) {
  const color = STATUS_COLOR[status] ?? "var(--muted)";
  const pulse = status === "unhealthy" || status === "degraded";
  return (
    <span
      style={{ display: "inline-block", width: 8, height: 8, borderRadius: "50%", background: color, flexShrink: 0, animation: pulse ? "pulse 1.5s ease-in-out infinite" : "none" }}
      aria-label={status}
    />
  );
}

function StatCard({ label, value, sub, accent }: { label: string; value: number | string; sub?: string; accent?: string }) {
  return (
    <div style={{ background: "var(--surface)", border: "1px solid var(--border)", borderRadius: 8, padding: "16px 18px" }}>
      <div style={{ fontSize: 11, fontFamily: "var(--mono)", letterSpacing: "0.08em", textTransform: "uppercase", color: "var(--muted)", marginBottom: 6 }}>{label}</div>
      <div style={{ fontSize: 28, fontWeight: 700, color: accent ?? "var(--fg)", fontVariantNumeric: "tabular-nums" }}>{value}</div>
      {sub && <div style={{ fontSize: 12, color: "var(--muted)", marginTop: 4 }}>{sub}</div>}
    </div>
  );
}

function ServiceCard({ name, status, lastHeartbeat }: { name: string; status: string; lastHeartbeat: string }) {
  const color = STATUS_COLOR[status] ?? "var(--muted)";
  return (
    <div style={{ background: "var(--surface)", border: `1px solid ${color}40`, borderRadius: 8, padding: "12px 14px", display: "flex", flexDirection: "column", gap: 6 }}>
      <div style={{ display: "flex", alignItems: "center", gap: 8 }}>
        <StatusDot status={status} />
        <span style={{ fontWeight: 600, fontSize: 13, flex: 1, overflow: "hidden", textOverflow: "ellipsis", whiteSpace: "nowrap" }}>{name}</span>
        <span style={{ fontSize: 11, fontFamily: "var(--mono)", color: color, fontWeight: 600, textTransform: "uppercase" }}>{status}</span>
      </div>
      <div style={{ fontSize: 11, color: "var(--muted)" }}>heartbeat {timeAgo(lastHeartbeat)}</div>
    </div>
  );
}

function AlertRow({ alert, onAck }: { alert: DashboardData["recentAlerts"][0]; onAck: (id: string) => void }) {
  const color = SEVERITY_COLOR[alert.rule.severity] ?? "var(--muted)";
  return (
    <div style={{ display: "flex", alignItems: "center", gap: 12, padding: "10px 14px", borderBottom: "1px solid var(--border)" }}>
      <span style={{ width: 8, height: 8, borderRadius: "50%", background: color, flexShrink: 0, display: "inline-block" }} />
      <div style={{ flex: 1, minWidth: 0 }}>
        <div style={{ fontSize: 13, fontWeight: 500 }}>
          {alert.rule.metric} {alert.rule.condition} {alert.rule.threshold}
        </div>
        <div style={{ fontSize: 11, color: "var(--muted)" }}>
          value: <span style={{ fontFamily: "var(--mono)" }}>{alert.lastValue}</span> · {timeAgo(alert.firedAt)} · {alert.rule.severity}
        </div>
      </div>
      {!alert.acknowledged && (
        <button
          onClick={() => onAck(alert.ruleId)}
          style={{ fontSize: 11, padding: "3px 10px", borderRadius: 5, border: "1px solid var(--border)", background: "transparent", color: "var(--muted)", cursor: "pointer" }}
        >
          Ack
        </button>
      )}
    </div>
  );
}

function MetricsTimeline({ points }: { points: MetricPoint[] }) {
  const byName = points.reduce<Record<string, MetricPoint[]>>((acc, p) => {
    (acc[p.name] ??= []).push(p);
    return acc;
  }, {});
  const names = Object.keys(byName).slice(0, 6);

  if (names.length === 0) {
    return (
      <div style={{ padding: 24, textAlign: "center", color: "var(--muted)", fontSize: 13 }}>
        No metrics ingested yet. Services can POST to <code style={{ fontFamily: "var(--mono)" }}>/api/v1/monitor/ingest</code> with <code style={{ fontFamily: "var(--mono)" }}>{`{ kind: "metric", name, value, source }`}</code>.
      </div>
    );
  }

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 16, padding: 16 }}>
      {names.map((name) => {
        const pts = (byName[name] ?? []).slice(-40);
        const vals = pts.map((p) => p.value);
        const min = Math.min(...vals);
        const max = Math.max(...vals) || 1;
        const range = max - min || 1;
        const latest = pts[pts.length - 1];
        return (
          <div key={name} style={{ background: "var(--surface)", border: "1px solid var(--border)", borderRadius: 8, padding: "12px 14px" }}>
            <div style={{ display: "flex", justifyContent: "space-between", marginBottom: 10, alignItems: "baseline" }}>
              <span style={{ fontFamily: "var(--mono)", fontSize: 12, fontWeight: 600 }}>{name}</span>
              <span style={{ fontFamily: "var(--mono)", fontSize: 18, fontWeight: 700, color: "var(--accent)" }}>
                {latest ? latest.value.toLocaleString(undefined, { maximumFractionDigits: 2 }) : "—"}
              </span>
            </div>
            <svg viewBox={`0 0 ${pts.length * 6} 40`} style={{ width: "100%", height: 40 }} preserveAspectRatio="none">
              <polyline
                points={pts.map((p, i) => `${i * 6 + 3},${40 - ((p.value - min) / range) * 36 + 2}`).join(" ")}
                fill="none"
                stroke="var(--accent)"
                strokeWidth="1.5"
                strokeLinejoin="round"
                strokeLinecap="round"
              />
            </svg>
            <div style={{ fontSize: 11, color: "var(--muted)", marginTop: 4 }}>
              {pts.length} points · source: {latest?.source ?? "—"} · last {latest ? timeAgo(latest.timestamp) : "—"}
            </div>
          </div>
        );
      })}
    </div>
  );
}

function AlertRulesPanel({ rules }: { rules: AlertRule[] }) {
  if (rules.length === 0) {
    return (
      <div style={{ padding: 24, textAlign: "center", color: "var(--muted)", fontSize: 13 }}>
        No alert rules defined. POST to <code style={{ fontFamily: "var(--mono)" }}>/api/v1/monitor/alerts/rules</code> to create one.
      </div>
    );
  }
  return (
    <table style={{ width: "100%", borderCollapse: "collapse", fontSize: 13 }}>
      <thead>
        <tr style={{ borderBottom: "1px solid var(--border)" }}>
          {["Metric", "Condition", "Threshold", "Severity", "Cooldown"].map((h) => (
            <th key={h} style={{ padding: "8px 14px", textAlign: "left", fontFamily: "var(--mono)", fontSize: 10, letterSpacing: "0.08em", textTransform: "uppercase", color: "var(--muted)", fontWeight: 600 }}>{h}</th>
          ))}
        </tr>
      </thead>
      <tbody>
        {rules.map((r) => (
          <tr key={r.id} style={{ borderBottom: "1px solid var(--border)" }}>
            <td style={{ padding: "9px 14px", fontFamily: "var(--mono)", fontSize: 12 }}>{r.metric}</td>
            <td style={{ padding: "9px 14px", fontFamily: "var(--mono)", fontSize: 12 }}>{r.condition}</td>
            <td style={{ padding: "9px 14px", fontFamily: "var(--mono)", fontSize: 12 }}>{r.threshold}</td>
            <td style={{ padding: "9px 14px" }}>
              <span style={{ fontSize: 11, fontFamily: "var(--mono)", fontWeight: 600, color: SEVERITY_COLOR[r.severity] ?? "var(--muted)", textTransform: "uppercase" }}>
                {r.severity}
              </span>
            </td>
            <td style={{ padding: "9px 14px", color: "var(--muted)", fontSize: 12 }}>{r.cooldownSeconds}s</td>
          </tr>
        ))}
      </tbody>
    </table>
  );
}

export default function App() {
  const [dash, setDash] = useState<DashboardData | null>(null);
  const [metrics, setMetrics] = useState<MetricPoint[]>([]);
  const [rules, setRules] = useState<AlertRule[]>([]);
  const [tab, setTab] = useState<Tab>("services");
  const [error, setError] = useState<string | null>(null);
  const [countdown, setCountdown] = useState(30);
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null);

  const load = useCallback(async () => {
    try {
      const [d, m, r] = await Promise.all([fetchDashboard(), fetchMetrics(100), fetchAlertRules()]);
      setDash(d);
      setMetrics(m);
      setRules(r);
      setError(null);
      setCountdown(30);
    } catch (e) {
      setError(String(e));
    }
  }, []);

  useEffect(() => {
    void load();
    intervalRef.current = setInterval(() => {
      setCountdown((c) => {
        if (c <= 1) { void load(); return 30; }
        return c - 1;
      });
    }, 1000);
    return () => { if (intervalRef.current) clearInterval(intervalRef.current); };
  }, [load]);

  const ack = async (id: string) => {
    await acknowledgeAlert(id);
    void load();
  };

  const o = dash?.overview;

  return (
    <>
      <style>{`
        @keyframes pulse {
          0%, 100% { opacity: 1; }
          50% { opacity: 0.4; }
        }
        button:hover { opacity: 0.8; }
      `}</style>

      <div style={{ maxWidth: 1200, margin: "0 auto", padding: "24px 16px" }}>

        {/* Header */}
        <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 28, flexWrap: "wrap", gap: 10 }}>
          <div>
            <div style={{ fontSize: 11, fontFamily: "var(--mono)", letterSpacing: "0.1em", textTransform: "uppercase", color: "var(--healthy)", marginBottom: 4 }}>
              Nexus Systems
            </div>
            <h1 style={{ fontSize: 24, fontWeight: 700, lineHeight: 1 }}>Monitor</h1>
            {dash && (
              <div style={{ fontSize: 12, color: "var(--muted)", marginTop: 4 }}>
                uptime {formatUptime(dash.uptimeSeconds)} · {dash.overview.eventsReceived.toLocaleString()} events received
              </div>
            )}
          </div>
          <div style={{ display: "flex", alignItems: "center", gap: 10 }}>
            {error && (
              <span style={{ fontSize: 12, color: "var(--unhealthy)", fontFamily: "var(--mono)" }}>
                ⚠ {error}
              </span>
            )}
            <button
              onClick={() => void load()}
              style={{ fontSize: 12, padding: "6px 14px", borderRadius: 6, border: "1px solid var(--border)", background: "transparent", color: "var(--muted)", cursor: "pointer", fontFamily: "var(--mono)" }}
            >
              ↻ {countdown}s
            </button>
          </div>
        </div>

        {/* Stat cards */}
        {o && (
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(min(100%, 180px), 1fr))", gap: 12, marginBottom: 24 }}>
            <StatCard label="Healthy" value={o.servicesHealthy} sub={`of ${o.servicesTotal} services`} accent="var(--healthy)" />
            <StatCard label="Degraded" value={o.servicesDegraded} sub="needs attention" accent={o.servicesDegraded > 0 ? "var(--degraded)" : undefined} />
            <StatCard label="Unhealthy" value={o.servicesUnhealthy} sub="critical" accent={o.servicesUnhealthy > 0 ? "var(--unhealthy)" : undefined} />
            <StatCard label="Active Alerts" value={o.activeAlerts} sub={`${o.alertRulesDefined} rules defined`} accent={o.activeAlerts > 0 ? "var(--unhealthy)" : undefined} />
            <StatCard label="Metrics" value={o.metricsStored.toLocaleString()} sub="data points stored" />
            <StatCard label="Heartbeats" value={o.heartbeatsReceived.toLocaleString()} sub="total received" />
          </div>
        )}

        {/* Main layout: service grid + recent alerts */}
        <div style={{ display: "grid", gridTemplateColumns: "1fr min(320px, 100%)", gap: 16, marginBottom: 24 }}>

          {/* Tabs + content */}
          <div style={{ background: "var(--surface)", border: "1px solid var(--border)", borderRadius: 10, overflow: "hidden" }}>
            <div style={{ display: "flex", borderBottom: "1px solid var(--border)" }}>
              {(["services", "metrics", "rules"] as Tab[]).map((t) => (
                <button
                  key={t}
                  onClick={() => setTab(t)}
                  style={{
                    padding: "12px 18px",
                    border: "none",
                    background: "transparent",
                    color: tab === t ? "var(--fg)" : "var(--muted)",
                    fontFamily: "var(--mono)",
                    fontSize: 11,
                    letterSpacing: "0.08em",
                    textTransform: "uppercase",
                    cursor: "pointer",
                    borderBottom: tab === t ? "2px solid var(--accent)" : "2px solid transparent",
                    marginBottom: -1,
                    fontWeight: tab === t ? 600 : 400,
                  }}
                >
                  {t}
                </button>
              ))}
            </div>

            {tab === "services" && dash && (
              <div style={{ padding: 16, display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(min(100%, 220px), 1fr))", gap: 10 }}>
                {Object.entries(dash.serviceStatuses).map(([name, s]) => (
                  <ServiceCard key={name} name={name} status={s.status} lastHeartbeat={s.lastHeartbeat} />
                ))}
                {Object.keys(dash.serviceStatuses).length === 0 && (
                  <div style={{ gridColumn: "1/-1", padding: "24px 0", textAlign: "center", color: "var(--muted)", fontSize: 13 }}>
                    No services registered yet. Services send heartbeats via POST to <code style={{ fontFamily: "var(--mono)" }}>/api/v1/monitor/ingest</code>.
                  </div>
                )}
              </div>
            )}

            {tab === "metrics" && <MetricsTimeline points={metrics} />}
            {tab === "rules" && <AlertRulesPanel rules={rules} />}
          </div>

          {/* Recent alerts */}
          <div style={{ background: "var(--surface)", border: "1px solid var(--border)", borderRadius: 10, overflow: "hidden", display: "flex", flexDirection: "column" }}>
            <div style={{ padding: "12px 14px", borderBottom: "1px solid var(--border)", fontFamily: "var(--mono)", fontSize: 11, letterSpacing: "0.08em", textTransform: "uppercase", color: "var(--muted)", fontWeight: 600 }}>
              Active Alerts
            </div>
            <div style={{ flex: 1, overflowY: "auto" }}>
              {dash?.recentAlerts && dash.recentAlerts.length > 0
                ? dash.recentAlerts.map((a, i) => <AlertRow key={i} alert={a} onAck={(id) => void ack(id)} />)
                : <div style={{ padding: 24, textAlign: "center", color: "var(--muted)", fontSize: 13 }}>No active alerts</div>
              }
            </div>
          </div>
        </div>

        <div style={{ fontSize: 11, color: "var(--muted)", textAlign: "center", paddingTop: 8 }}>
          Nexus Monitor · auto-refreshes every 30s · <a href="https://tnhc.dev" style={{ color: "var(--muted)" }}>tnhc.dev</a>
        </div>
      </div>
    </>
  );
}
