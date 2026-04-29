import { useEffect, useState } from "react";

export default function PlatformEconomicsHub() {
  const [drivers, setDrivers] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/economics/drivers");
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
      <h2>Platform Economics Hub</h2>
      <p>Economic model design, scenario simulation, and forecast generation.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Drivers</h3>
          <p>{drivers.length > 0 ? drivers.join(", ") : "No economics drivers loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Models</h3>
          <p>Economic model lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Forecasts</h3>
          <p>Forecast generation and scenario APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
