import { useEffect, useState } from "react";

export default function EcosystemComplianceExchangeHub() {
  const [domains, setDomains] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/ecosystem-compliance/posture");
        const data = await res.json();
        setDomains(data.entity?.domains || []);
      } catch {
        setDomains([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Ecosystem Compliance Exchange Hub</h2>
      <p>Partner attestations, shared controls, and ecosystem posture coordination.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Domains</h3>
          <p>{domains.length > 0 ? domains.join(", ") : "No compliance domains loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Partners</h3>
          <p>Partner registry APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Attestations</h3>
          <p>Attestation exchange workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
