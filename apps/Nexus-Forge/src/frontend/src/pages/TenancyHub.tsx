import { useEffect, useState } from "react";

export default function TenancyHub() {
  const [models, setModels] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/tenancy/models");
        const data = await res.json();
        setModels(data.entity?.models || []);
      } catch {
        setModels([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Tenancy Hub</h2>
      <p>Organization isolation, environment topology, and tenant provisioning controls.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Models</h3>
          <p>{models.length > 0 ? models.join(", ") : "No tenancy models loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Organizations</h3>
          <p>Organization lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Provisioning</h3>
          <p>Tenant provisioning contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
