import { useEffect, useState } from "react";

export default function GovernanceRiskCouncilHub() {
  const [charterTypes, setCharterTypes] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/governance/charters");
        const data = await res.json();
        setCharterTypes(data.entity?.charterTypes || []);
      } catch {
        setCharterTypes([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Governance Risk Council Hub</h2>
      <p>Council charters, risk registers, and escalation governance workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Charter Types</h3>
          <p>{charterTypes.length > 0 ? charterTypes.join(", ") : "No charter types loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Councils</h3>
          <p>Council lifecycle and ownership APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Risk Escalations</h3>
          <p>Escalation workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
