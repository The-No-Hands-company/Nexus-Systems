import { useEffect, useState } from "react";

export default function InfrastructureOptimizationLabHub() {
  const [recommendations, setRecommendations] = useState<{ focus: string[]; savings: string }>({ focus: [], savings: "" });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/infra-optimization/recommendations");
        const data = await res.json();
        setRecommendations(data.entity || { focus: [], savings: "" });
      } catch {
        setRecommendations({ focus: [], savings: "" });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Infrastructure Optimization Lab Hub</h2>
      <p>Infrastructure baseline analysis, optimization experiments, and cost-performance recommendations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Baseline Metrics</h3>
          <p>Infrastructure baselines are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Experiments</h3>
          <p>Optimization experiments are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Recommendations</h3>
          <p>Focus: {recommendations.focus.join(", ") || "Analyzing..."}</p>
        </article>
      </div>
    </section>
  );
}
