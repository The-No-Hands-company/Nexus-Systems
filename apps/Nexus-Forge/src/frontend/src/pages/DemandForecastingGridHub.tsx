import { useEffect, useState } from "react";

export default function DemandForecastingGridHub() {
  const [signals, setSignals] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/demand-forecasting/signals");
        const data = await res.json();
        setSignals(data.entity?.signals || []);
      } catch {
        setSignals([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Demand Forecasting Grid Hub</h2>
      <p>Demand signal modeling, scenario runs, and forecast operations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Signals</h3>
          <p>{signals.length > 0 ? signals.join(", ") : "No signals loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Models</h3>
          <p>Forecast model lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Runs</h3>
          <p>Forecast simulation run workflow is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
