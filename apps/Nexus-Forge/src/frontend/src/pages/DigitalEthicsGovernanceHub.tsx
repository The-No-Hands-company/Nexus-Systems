import { useEffect, useState } from "react";

export default function DigitalEthicsGovernanceHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/digital-ethics/dimensions");
        const data = await res.json();
        setDimensions(data.entity?.dimensions || []);
      } catch {
        setDimensions([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Digital Ethics Governance Hub</h2>
      <p>Ethics principle lifecycle, review operations, and accountability dimensions.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No ethics dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Principles</h3>
          <p>Principle authoring APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Assessments</h3>
          <p>Ethics assessment workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
