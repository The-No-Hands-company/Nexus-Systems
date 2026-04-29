import { useEffect, useState } from "react";

export default function ResilienceHub() {
  const [plans, setPlans] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/resilience/dr-plans");
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
      <h2>Resilience Hub</h2>
      <p>Disaster recovery plans, chaos experiments, failover drills, and continuity controls.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>DR Plans</h3>
          <p>Defined plans: {plans}</p>
        </article>
        <article className="repo-card">
          <h3>Chaos Experiments</h3>
          <p>Resilience experiment scheduling is stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Failover Readiness</h3>
          <p>Failover and drill reporting endpoints are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
