import { useEffect, useState } from "react";

export default function SupplyContinuityPlannerHub() {
  const [chains, setChains] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/supply-continuity/chains");
        const data = await res.json();
        setChains((data.items || []).length);
      } catch {
        setChains(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Supply Continuity Planner Hub</h2>
      <p>Supply chain continuity planning, disruption tracking, and mitigation control.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Chains</h3>
          <p>Chains tracked: {chains}</p>
        </article>
        <article className="repo-card">
          <h3>Disruptions</h3>
          <p>Disruption feed APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Reserves</h3>
          <p>Reserve model workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
