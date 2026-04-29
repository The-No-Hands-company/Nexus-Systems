import { useEffect, useState } from "react";

export default function PolicyLabHub() {
  const [simulations, setSimulations] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/policy/simulations");
        const data = await res.json();
        setSimulations((data.items || []).length);
      } catch {
        setSimulations(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Policy Lab Hub</h2>
      <p>Policy simulation, impact forecasting, and what-if governance scenarios.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Simulations</h3>
          <p>Simulation jobs: {simulations}</p>
        </article>
        <article className="repo-card">
          <h3>Impact Forecast</h3>
          <p>Policy impact analysis stubs are active.</p>
        </article>
        <article className="repo-card">
          <h3>What-if Engine</h3>
          <p>Scenario exploration APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
