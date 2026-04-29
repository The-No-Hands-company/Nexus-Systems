import { useEffect, useState } from "react";

export default function ArchitectureDecisionHub() {
  const [templates, setTemplates] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/adr/templates");
        const data = await res.json();
        setTemplates(data.entity?.templates || []);
      } catch {
        setTemplates([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Architecture Decision Hub</h2>
      <p>ADR catalog, review workflows, and traceability through platform evolution.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Templates</h3>
          <p>{templates.length > 0 ? templates.join(", ") : "No templates loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Records</h3>
          <p>ADR creation and indexing APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Traceability</h3>
          <p>Decision dependency graph is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
