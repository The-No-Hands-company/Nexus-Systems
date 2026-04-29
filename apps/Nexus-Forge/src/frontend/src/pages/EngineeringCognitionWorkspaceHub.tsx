import { useEffect, useState } from "react";

export default function EngineeringCognitionWorkspaceHub() {
  const [notebooks, setNotebooks] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/engineering-cognition/notebooks");
        const data = await res.json();
        setNotebooks((data.items || []).length);
      } catch {
        setNotebooks(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Engineering Cognition Workspace Hub</h2>
      <p>Hypothesis-driven engineering notebooks, experiments, and insight workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Notebooks</h3>
          <p>Notebooks tracked: {notebooks}</p>
        </article>
        <article className="repo-card">
          <h3>Hypotheses</h3>
          <p>Hypothesis backlog APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Insights</h3>
          <p>Insight feed workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
