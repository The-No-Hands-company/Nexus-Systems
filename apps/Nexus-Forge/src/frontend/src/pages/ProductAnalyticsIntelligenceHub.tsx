import { useEffect, useState } from "react";

export default function ProductAnalyticsIntelligenceHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/product-analytics/segments");
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
      <h2>Product Analytics Intelligence Hub</h2>
      <p>Product usage events, cohort analysis, and predictive user behavior analytics.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Events</h3>
          <p>Usage events are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Cohorts</h3>
          <p>User cohort analysis is stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Segments</h3>
          <p>{dimensions.join(", ") || "No segments available"}</p>
        </article>
      </div>
    </section>
  );
}
