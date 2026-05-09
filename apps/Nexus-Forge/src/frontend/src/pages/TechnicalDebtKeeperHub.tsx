import { useEffect, useState } from "react";

export default function TechnicalDebtKeeperHub() {
  const [trends, setTrends] = useState<{ trends: string[]; focus: string }>({
    trends: [],
    focus: "",
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/tech-debt/trends");
        const data = await res.json();
        setTrends(data.entity || { trends: [], focus: "" });
      } catch {
        setTrends({ trends: [], focus: "" });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Technical Debt Keeper Hub</h2>
      <p>Inventory, tracking, and repayment management for technical debt across codebases.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Debt Inventory</h3>
          <p>Debt items are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Impact Analysis</h3>
          <p>Velocity and quality impacts are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Trends</h3>
          <p>
            Status: {trends.focus}, Trend: {trends.trends.join(" → ") || "No data"}
          </p>
        </article>
      </div>
    </section>
  );
}
