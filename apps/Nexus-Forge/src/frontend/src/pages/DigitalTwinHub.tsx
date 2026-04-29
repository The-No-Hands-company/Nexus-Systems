import { useEffect, useState } from "react";

export default function DigitalTwinHub() {
  const [envCount, setEnvCount] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/digital-twin/environments");
        const data = await res.json();
        setEnvCount((data.items || []).length);
      } catch {
        setEnvCount(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Digital Twin Hub</h2>
      <p>Synthetic environment provisioning and scenario simulation orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Environments</h3>
          <p>Digital twin environments: {envCount}</p>
        </article>
        <article className="repo-card">
          <h3>Scenarios</h3>
          <p>Scenario queue contracts are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Insights</h3>
          <p>Simulation insight feed is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
