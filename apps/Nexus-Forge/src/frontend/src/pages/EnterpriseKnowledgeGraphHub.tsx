import { useEffect, useState } from "react";

export default function EnterpriseKnowledgeGraphHub() {
  const [entities, setEntities] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/knowledge-graph/entities");
        const data = await res.json();
        setEntities((data.items || []).length);
      } catch {
        setEntities(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Enterprise Knowledge Graph Hub</h2>
      <p>Entity graphing, relationship modeling, and inference-driven knowledge views.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Entities</h3>
          <p>Entities tracked: {entities}</p>
        </article>
        <article className="repo-card">
          <h3>Relationships</h3>
          <p>Relationship graph APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Inferences</h3>
          <p>Inference and view workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
