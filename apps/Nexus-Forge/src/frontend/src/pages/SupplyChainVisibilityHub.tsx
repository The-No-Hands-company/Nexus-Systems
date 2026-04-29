import { useEffect, useState } from "react";

export default function SupplyChainVisibilityHub() {
  const [metrics, setMetrics] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/supply-chain/analytics");
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
      <h2>Supply Chain Visibility Hub</h2>
      <p>Real-time tracking, disruption detection, and analytics for supply chain operations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Node Visibility</h3>
          <p>Supply chain nodes are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Disruptions</h3>
          <p>Active disruptions are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Analytics</h3>
          <p>{metrics.join(", ") || "No metrics available"}</p>
        </article>
      </div>
    </section>
  );
}
