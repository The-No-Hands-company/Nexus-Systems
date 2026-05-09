import { useEffect, useState } from "react";

export default function BudgetAllocationEngineHub() {
  const [health, setHealth] = useState<{ utilization: number; trend: string }>({
    utilization: 0,
    trend: "",
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/budget-allocation/health");
        const data = await res.json();
        setHealth(data.entity || { utilization: 0, trend: "" });
      } catch {
        setHealth({ utilization: 0, trend: "" });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Budget Allocation Engine Hub</h2>
      <p>Budget planning, allocation tracking, and spend forecasting across organizations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Budgets</h3>
          <p>Budget allocations are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Tracking</h3>
          <p>Spend tracking is stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Health</h3>
          <p>
            Utilization: {health.utilization}%, Trend: {health.trend}
          </p>
        </article>
      </div>
    </section>
  );
}
