import { useEffect, useState } from "react";

export default function AdminConsoleHub() {
  const [panels, setPanels] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/admin-console/panels");
        const data = await res.json();
        setPanels(data.entity?.panels || []);
      } catch {
        setPanels([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Admin Console Hub</h2>
      <p>Global system administration, jobs, and maintenance orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Panels</h3>
          <p>{panels.length > 0 ? panels.join(", ") : "No admin panels loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Admin Jobs</h3>
          <p>Background operation scheduling is stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Maintenance Mode</h3>
          <p>Maintenance windows and toggles are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
