import { useEffect, useState } from "react";

export default function QualityHub() {
  const [plans, setPlans] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/quality/test-plans");
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
      <h2>Quality Hub</h2>
      <p>Test plans, executions, flaky test analytics, and coverage intelligence.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Test Plans</h3>
          <p>Defined plans: {plans}</p>
        </article>
        <article className="repo-card">
          <h3>Run History</h3>
          <p>Test run scheduling and run logs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Coverage Insights</h3>
          <p>Coverage and flaky test analytics are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
