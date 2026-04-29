import { useEffect, useState } from "react";

export default function CustomerJourneyOrchestrationHub() {
  const [maps, setMaps] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/journeys/maps");
        const data = await res.json();
        setMaps((data.items || []).length);
      } catch {
        setMaps(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Customer Journey Orchestration Hub</h2>
      <p>Journey mapping, touchpoint planning, and lifecycle automation design.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Journey Maps</h3>
          <p>Maps tracked: {maps}</p>
        </article>
        <article className="repo-card">
          <h3>Touchpoints</h3>
          <p>Touchpoint catalogs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Automations</h3>
          <p>Journey automation execution contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
