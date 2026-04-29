import { useEffect, useState } from "react";

export default function MergeStrategyEngineHub() {
  const [strategies, setStrategies] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/merge/history");
        const data = await res.json();
        setStrategies(data.entity?.strategies || []);
      } catch {
        setStrategies([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Merge Strategy Engine Hub</h2>
      <p>Merge conflict resolution, strategy analysis, and automated merge orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Strategies</h3>
          <p>{strategies.join(", ") || "No strategies available"}</p>
        </article>
        <article className="repo-card">
          <h3>Conflict Analysis</h3>
          <p>Merge conflicts are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Resolution</h3>
          <p>Automated resolution is stubbed</p>
        </article>
      </div>
    </section>
  );
}
