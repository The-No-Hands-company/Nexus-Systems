import { useEffect, useState } from "react";

export default function RuntimeGovernanceHub() {
  const [posture, setPosture] = useState("unknown");

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/runtime/posture");
        const data = await res.json();
        setPosture(data.entity?.posture || "unknown");
      } catch {
        setPosture("unknown");
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Runtime Governance Hub</h2>
      <p>Policy guardrails, runtime evaluations, and execution posture intelligence.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Posture</h3>
          <p>Current posture: {posture}</p>
        </article>
        <article className="repo-card">
          <h3>Policies</h3>
          <p>Runtime policy lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Evaluations</h3>
          <p>Evaluation and guardrail contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
