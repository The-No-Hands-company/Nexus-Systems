import { useEffect, useState } from "react";

export default function PolicyAsCodeStudioHub() {
  const [policies, setPolicies] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/policy-code/policies");
        const data = await res.json();
        setPolicies((data.items || []).length);
      } catch {
        setPolicies(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Policy as Code Studio Hub</h2>
      <p>Policy authoring, tests, evaluation, and bundle release orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Policies</h3>
          <p>Policies tracked: {policies}</p>
        </article>
        <article className="repo-card">
          <h3>Tests</h3>
          <p>Policy test catalog APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Evaluations</h3>
          <p>Policy evaluation and decisioning are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
