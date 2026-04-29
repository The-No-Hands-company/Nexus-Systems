import { useEffect, useState } from "react";

export default function MultiCloudFailoverMeshHub() {
  const [states, setStates] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/failover-mesh/posture");
        const data = await res.json();
        setStates(data.entity?.states || []);
      } catch {
        setStates([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Multi-Cloud Failover Mesh Hub</h2>
      <p>Failover topology management, readiness exercises, and posture monitoring.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Posture States</h3>
          <p>{states.length > 0 ? states.join(", ") : "No posture states loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Topologies</h3>
          <p>Topology authoring APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Activations</h3>
          <p>Activation workflow APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
