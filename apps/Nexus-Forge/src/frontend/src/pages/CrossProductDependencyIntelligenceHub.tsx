import { useEffect, useState } from "react";

export default function CrossProductDependencyIntelligenceHub() {
  const [factors, setFactors] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/dependency-intel/risk");
        const data = await res.json();
        setFactors(data.entity?.factors || []);
      } catch {
        setFactors([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Cross-Product Dependency Intelligence Hub</h2>
      <p>Dependency graphing, hotspot analysis, and risk factor diagnostics.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Risk Factors</h3>
          <p>{factors.length > 0 ? factors.join(", ") : "No factors loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Graph</h3>
          <p>Dependency graph APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Hotspots</h3>
          <p>Hotspot analytics contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
