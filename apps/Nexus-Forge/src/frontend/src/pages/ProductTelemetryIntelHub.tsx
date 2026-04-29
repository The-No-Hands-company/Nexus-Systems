import { useEffect, useState } from "react";

export default function ProductTelemetryIntelHub() {
  const [signals, setSignals] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/product-telemetry/signals");
        const data = await res.json();
        setSignals(data.entity?.signals || []);
      } catch {
        setSignals([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Product Telemetry Intelligence Hub</h2>
      <p>Event intelligence, funnel behavior, and product signal-based alerting.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Signals</h3>
          <p>{signals.length > 0 ? signals.join(", ") : "No signals loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Funnels</h3>
          <p>Funnel analytics and cohort APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Alerts</h3>
          <p>Telemetry alert workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
