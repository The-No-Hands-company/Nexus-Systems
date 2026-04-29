import { useEffect, useState } from "react";

export default function StrategyOKRHub() {
  const [views, setViews] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/strategy/scorecards");
        const data = await res.json();
        setViews(data.entity?.views || []);
      } catch {
        setViews([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Strategy & OKR Hub</h2>
      <p>Objective planning, key result tracking, and strategic scorecard visibility.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Scorecard Views</h3>
          <p>{views.length > 0 ? views.join(", ") : "No scorecard views loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Objectives</h3>
          <p>Objective portfolio APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Progress Updates</h3>
          <p>OKR update workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
