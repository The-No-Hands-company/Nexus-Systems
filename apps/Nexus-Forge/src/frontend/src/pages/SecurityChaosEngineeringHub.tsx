import { useEffect, useState } from "react";

export default function SecurityChaosEngineeringHub() {
  const [scopes, setScopes] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/security-chaos/scopes");
        const data = await res.json();
        setScopes(data.entity?.scopes || []);
      } catch {
        setScopes([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Security Chaos Engineering Hub</h2>
      <p>Adversarial experiments, findings analysis, and remediation orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Scopes</h3>
          <p>{scopes.length > 0 ? scopes.join(", ") : "No chaos scopes loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Experiments</h3>
          <p>Experiment lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Remediations</h3>
          <p>Remediation workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
