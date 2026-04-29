import { useEffect, useState } from "react";

export default function CloudCostAnomalyLabHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/cloud-cost/dimensions");
        const data = await res.json();
        setDimensions(data.entity?.dimensions || []);
      } catch {
        setDimensions([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Cloud Cost Anomaly Lab Hub</h2>
      <p>Cost anomaly detection, baseline models, and alerting orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No cost dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Anomalies</h3>
          <p>Anomaly feed APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Alerts</h3>
          <p>Alert workflow APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
