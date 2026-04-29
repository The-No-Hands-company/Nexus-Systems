import { useEffect, useState } from "react";

export default function ReleaseCadenceOptimizerHub() {
  const [plans, setPlans] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/release-cadence/plans");
        const data = await res.json();
        setPlans((data.items || []).length);
      } catch {
        setPlans(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Release Cadence Optimizer Hub</h2>
      <p>Release cadence planning, signal processing, and recommendation workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Plans</h3>
          <p>Plans tracked: {plans}</p>
        </article>
        <article className="repo-card">
          <h3>Signals</h3>
          <p>Signal ingestion APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Recommendations</h3>
          <p>Recommendation workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
