import { useEffect, useState } from "react";

export default function DataContractRegistryHub() {
  const [contracts, setContracts] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/data-contracts/contracts");
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
      <h2>Data Contract Registry Hub</h2>
      <p>Data contract cataloging, versioning, verification, and compliance views.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Contracts</h3>
          <p>Contracts tracked: {contracts}</p>
        </article>
        <article className="repo-card">
          <h3>Versions</h3>
          <p>Version graph and lineage APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Compliance</h3>
          <p>Contract compliance and verification flows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
