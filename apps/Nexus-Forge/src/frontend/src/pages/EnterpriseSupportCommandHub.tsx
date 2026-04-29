import { useEffect, useState } from "react";

export default function EnterpriseSupportCommandHub() {
  const [tiers, setTiers] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/support/slas");
        const data = await res.json();
        setTiers(data.entity?.tiers || []);
      } catch {
        setTiers([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Enterprise Support Command Hub</h2>
      <p>Support queues, incident streams, SLA governance, and escalations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>SLA Tiers</h3>
          <p>{tiers.length > 0 ? tiers.join(", ") : "No SLA tiers loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Queues</h3>
          <p>Support queue and routing APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Escalations</h3>
          <p>Escalation workflow APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
