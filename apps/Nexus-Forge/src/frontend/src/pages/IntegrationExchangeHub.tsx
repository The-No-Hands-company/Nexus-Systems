import { useEffect, useState } from "react";

export default function IntegrationExchangeHub() {
  const [states, setStates] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/integrations/health");
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
      <h2>Integration Exchange Hub</h2>
      <p>Integration publishing, connector lifecycle, and ecosystem health management.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Connector States</h3>
          <p>{states.length > 0 ? states.join(", ") : "No states loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Catalog</h3>
          <p>Integration catalog APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Publishing</h3>
          <p>Publishing and review contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
