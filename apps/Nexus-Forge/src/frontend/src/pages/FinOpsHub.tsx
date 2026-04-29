import { useEffect, useState } from "react";

export default function FinOpsHub() {
  const [budgets, setBudgets] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/finops/budgets");
        const data = await res.json();
        setBudgets((data.items || []).length);
      } catch {
        setBudgets(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>FinOps Hub</h2>
      <p>Budget controls, chargeback visibility, and cost anomaly workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Budgets</h3>
          <p>Budget objects: {budgets}</p>
        </article>
        <article className="repo-card">
          <h3>Anomalies</h3>
          <p>Cost anomaly analysis contracts are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Chargeback</h3>
          <p>Allocation and chargeback reporting is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
