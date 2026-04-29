import { useEffect, useState } from "react";

export default function ChangeManagementOfficeHub() {
  const [initiatives, setInitiatives] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/change-office/initiatives");
        const data = await res.json();
        setInitiatives((data.items || []).length);
      } catch {
        setInitiatives(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Change Management Office Hub</h2>
      <p>Initiative governance, communications planning, and readiness checkpoints.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Initiatives</h3>
          <p>Initiatives tracked: {initiatives}</p>
        </article>
        <article className="repo-card">
          <h3>Comms Plans</h3>
          <p>Communication planning APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Readiness</h3>
          <p>Readiness and checkpoint workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
