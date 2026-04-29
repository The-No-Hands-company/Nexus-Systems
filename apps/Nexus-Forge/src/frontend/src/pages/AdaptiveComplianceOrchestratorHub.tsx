import { useEffect, useState } from "react";

export default function AdaptiveComplianceOrchestratorHub() {
  const [modes, setModes] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/adaptive-compliance/modes");
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
      <h2>Adaptive Compliance Orchestrator Hub</h2>
      <p>Dynamic compliance rules, signal ingestion, and automated remediation orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Modes</h3>
          <p>{modes.length > 0 ? modes.join(", ") : "No compliance modes loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Rules</h3>
          <p>Rule lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Actions</h3>
          <p>Action workflow APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
