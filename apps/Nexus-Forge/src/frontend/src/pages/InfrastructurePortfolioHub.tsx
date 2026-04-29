import { useEffect, useState } from "react";

export default function InfrastructurePortfolioHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/infra-portfolio/risk");
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
      <h2>Infrastructure Portfolio Hub</h2>
      <p>Asset portfolio planning, budget alignment, and risk posture intelligence.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Risk Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No risk dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Assets</h3>
          <p>Infrastructure asset inventory APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Roadmaps</h3>
          <p>Roadmap and budget planning APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
