import { useEffect, useState } from "react";

export default function DataGovernanceFabricHub() {
  const [families, setFamilies] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/data-governance/policies");
        const data = await res.json();
        setFamilies(data.entity?.policyFamilies || []);
      } catch {
        setFamilies([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Data Governance Fabric Hub</h2>
      <p>Dataset inventory, lineage visibility, classifications, and policy governance.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Policy Families</h3>
          <p>{families.length > 0 ? families.join(", ") : "No policy families loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Datasets</h3>
          <p>Dataset registration and ownership APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Lineage</h3>
          <p>Lineage graph and dependency tracking are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
