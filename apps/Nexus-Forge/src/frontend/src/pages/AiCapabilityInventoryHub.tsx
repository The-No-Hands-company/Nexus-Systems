import { useEffect, useState } from "react";

export default function AiCapabilityInventoryHub() {
  const [policies, setPolicies] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/ai-capabilities/governance");
        const data = await res.json();
        setPolicies(data.entity?.policies || []);
      } catch {
        setPolicies([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>AI Capability Inventory Hub</h2>
      <p>Catalog, governance, and lifecycle management for AI capabilities across platform.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Capabilities</h3>
          <p>AI models are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Governance Policies</h3>
          <p>{policies.join(", ") || "No policies configured"}</p>
        </article>
        <article className="repo-card">
          <h3>Evaluations</h3>
          <p>Capability readiness assessments are stubbed</p>
        </article>
      </div>
    </section>
  );
}
