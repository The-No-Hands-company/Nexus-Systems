import { useEffect, useState } from "react";

export default function MultiTenantDataIsolationHub() {
  const [strategies, setStrategies] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/multi-tenant/strategies");
        const data = await res.json();
        setStrategies(data.entity?.strategies || []);
      } catch {
        setStrategies([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Multi-Tenant Data Isolation Hub</h2>
      <p>Data isolation policies, boundary management, and tenant compliance auditing.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Isolation Policies</h3>
          <p>Tenant boundaries are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Audit Logs</h3>
          <p>Access audit trails are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Strategies</h3>
          <p>{strategies.join(", ") || "No strategies configured"}</p>
        </article>
      </div>
    </section>
  );
}
