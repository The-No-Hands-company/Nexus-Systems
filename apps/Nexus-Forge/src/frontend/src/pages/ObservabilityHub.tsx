import { useEffect, useState } from "react";

export default function ObservabilityHub() {
  const [metrics, setMetrics] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/observability/overview");
        const data = await res.json();
        setMetrics(data.entity?.metrics || []);
      } catch {
        setMetrics([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Observability Hub</h2>
      <p>Unified metrics, traces, logs, alert policy, and SLO operation contracts.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Golden Signals</h3>
          <p>{metrics.length > 0 ? metrics.join(", ") : "No metrics loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Traces & Logs</h3>
          <p>Trace search and log index APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Alerts & SLO</h3>
          <p>Alerting and SLO management are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
