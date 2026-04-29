import { useEffect, useState } from "react";

export default function IncidentPreventionGraphHub() {
  const [nodes, setNodes] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/incident-prevention/nodes");
        const data = await res.json();
        setNodes((data.items || []).length);
      } catch {
        setNodes(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Incident Prevention Graph Hub</h2>
      <p>Prevention graph modeling, dependency simulation, and hotspot analysis.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Nodes</h3>
          <p>Nodes tracked: {nodes}</p>
        </article>
        <article className="repo-card">
          <h3>Dependencies</h3>
          <p>Graph edge APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Hotspots</h3>
          <p>Hotspot analytics workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
