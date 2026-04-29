import { useEffect, useState } from "react";

export default function IncidentLearningSystemHub() {
  const [reports, setReports] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/incident-learning/reports");
        const data = await res.json();
        setReports((data.items || []).length);
      } catch {
        setReports(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Incident Learning System Hub</h2>
      <p>Learning reports, action management, and incident pattern discovery workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Reports</h3>
          <p>Reports tracked: {reports}</p>
        </article>
        <article className="repo-card">
          <h3>Actions</h3>
          <p>Corrective action tracking APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Patterns</h3>
          <p>Pattern mining contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
