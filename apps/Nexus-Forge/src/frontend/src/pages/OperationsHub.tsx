import { useEffect, useState } from "react";

export default function OperationsHub() {
  const [incidents, setIncidents] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/incidents");
        const data = await res.json();
        setIncidents((data.incidents || []).length);
      } catch {
        setIncidents(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Operations Hub</h2>
      <p>Incident response, status page, postmortems, and on-call orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Incidents</h3>
          <p>Open incidents: {incidents}</p>
        </article>
        <article className="repo-card">
          <h3>Status Page</h3>
          <p>Public status and component health endpoints are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Postmortems</h3>
          <p>Learning loop for incident recovery is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
