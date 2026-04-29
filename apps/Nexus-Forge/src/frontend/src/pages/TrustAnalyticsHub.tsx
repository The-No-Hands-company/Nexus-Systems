import { useEffect, useState } from "react";

export default function TrustAnalyticsHub() {
  const [categories, setCategories] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/trust-analytics/controls");
        const data = await res.json();
        setCategories(data.entity?.categories || []);
      } catch {
        setCategories([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Trust Analytics Hub</h2>
      <p>Trust scorecards, signal telemetry, benchmarks, and control analytics.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Control Categories</h3>
          <p>{categories.length > 0 ? categories.join(", ") : "No categories loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Scorecards</h3>
          <p>Trust scorecard APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Benchmarks</h3>
          <p>Benchmark and signal workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
