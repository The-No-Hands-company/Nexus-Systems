import { useEffect, useState } from "react";

export default function ReleaseReliabilityScorecardsHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/release-reliability/dimensions");
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
      <h2>Release Reliability Scorecards Hub</h2>
      <p>Release quality dimensions, reliability budgets, and scorecard tracking.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Scorecards</h3>
          <p>Release scorecard APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Budgets</h3>
          <p>Reliability budget workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
