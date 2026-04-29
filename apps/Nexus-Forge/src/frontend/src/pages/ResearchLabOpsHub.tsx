import { useEffect, useState } from "react";

export default function ResearchLabOpsHub() {
  const [workflows, setWorkflows] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/research/ethics");
        const data = await res.json();
        setWorkflows(data.entity?.workflows || []);
      } catch {
        setWorkflows([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Research Lab Ops Hub</h2>
      <p>Lab provisioning, proposal governance, and ethics workflow orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Ethics Workflows</h3>
          <p>{workflows.length > 0 ? workflows.join(", ") : "No workflows loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Labs</h3>
          <p>Lab inventory and provisioning APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Programs</h3>
          <p>Research program tracking contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
