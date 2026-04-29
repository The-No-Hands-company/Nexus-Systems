import { useEffect, useState } from "react";

export default function ApiEvolutionControlTowerHub() {
  const [contracts, setContracts] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/api-evolution/contracts");
        const data = await res.json();
        setContracts((data.items || []).length);
      } catch {
        setContracts(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>API Evolution Control Tower Hub</h2>
      <p>API contract evolution, deprecation windows, and compatibility guidance.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Contracts</h3>
          <p>Contracts tracked: {contracts}</p>
        </article>
        <article className="repo-card">
          <h3>Deprecations</h3>
          <p>Deprecation planning APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Compatibility</h3>
          <p>Compatibility matrix workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
