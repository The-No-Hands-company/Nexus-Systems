import { useEffect, useState } from "react";

export default function ServiceLevelEconomicsHub() {
  const [drivers, setDrivers] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/service-economics/drivers");
        const data = await res.json();
        setDrivers(data.entity?.drivers || []);
      } catch {
        setDrivers([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Service Level Economics Hub</h2>
      <p>SLO target cost modeling, optimization runs, and driver-based planning.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Drivers</h3>
          <p>{drivers.length > 0 ? drivers.join(", ") : "No economics drivers loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Services</h3>
          <p>Service economics setup APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Optimizations</h3>
          <p>Optimization run workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
