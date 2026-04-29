import { useEffect, useState } from "react";

export default function PortfolioSynergyEngineHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/portfolio-synergy/metrics");
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
      <h2>Portfolio Synergy Engine Hub</h2>
      <p>Initiative overlap analysis, simulation modeling, and synergy metrics orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Initiatives</h3>
          <p>Initiative planning APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Simulations</h3>
          <p>Scenario simulation APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
