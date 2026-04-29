import { useEffect, useState } from "react";

export default function PluginSandboxHub() {
  const [policies, setPolicies] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/plugins/policies");
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
      <h2>Plugin Sandbox Hub</h2>
      <p>Plugin registry, sandbox isolation, and execution safety policies.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Policies</h3>
          <p>{policies.length > 0 ? policies.join(", ") : "No policies loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Registry</h3>
          <p>Plugin registration APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Sandboxes</h3>
          <p>Sandbox allocation and lifecycle are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
