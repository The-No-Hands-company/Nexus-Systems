import { useEffect, useState } from "react";

export default function SupplyChainHub() {
  const [sbom, setSbom] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/supply-chain/sbom");
        const data = await res.json();
        setSbom((data.items || []).length);
      } catch {
        setSbom(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Supply Chain Hub</h2>
      <p>SBOM, attestations, provenance graph, and policy controls.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>SBOM Artifacts</h3>
          <p>SBOM records: {sbom}</p>
        </article>
        <article className="repo-card">
          <h3>Attestations</h3>
          <p>Signed artifact attestations are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Provenance</h3>
          <p>Build and dependency provenance contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
