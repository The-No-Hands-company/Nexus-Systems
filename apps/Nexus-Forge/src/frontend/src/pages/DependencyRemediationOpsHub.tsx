import { useEffect, useState } from "react";

export default function DependencyRemediationOpsHub() {
  const [bands, setBands] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/dependency-remediation/risk-bands");
        const data = await res.json();
        setBands(data.entity?.bands || []);
      } catch {
        setBands([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Dependency Remediation Ops Hub</h2>
      <p>Dependency findings intake, remediation planning, and risk-banded execution.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Risk Bands</h3>
          <p>{bands.length > 0 ? bands.join(", ") : "No risk bands loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Findings</h3>
          <p>Finding intake APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Actions</h3>
          <p>Remediation action workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
