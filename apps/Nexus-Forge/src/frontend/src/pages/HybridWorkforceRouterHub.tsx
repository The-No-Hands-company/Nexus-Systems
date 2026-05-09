import { useEffect, useState } from "react";

export default function HybridWorkforceRouterHub() {
  const [modes, setModes] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/workforce/modes");
        const data = await res.json();
        setModes(data.entity?.modes || []);
      } catch {
        setModes([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Hybrid Workforce Router Hub</h2>
      <p>
        Workforce assignment, scheduling, and optimization across on-site, remote, and hybrid modes.
      </p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Assignments</h3>
          <p>Task assignments are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Capacity</h3>
          <p>Workforce capacity is stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Modes</h3>
          <p>{modes.join(", ") || "No modes configured"}</p>
        </article>
      </div>
    </section>
  );
}
