import { useEffect, useState } from "react";

export default function PolicyConflictResolverHub() {
  const [strategies, setStrategies] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/policy-conflicts/strategies");
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
      <h2>Policy Conflict Resolver Hub</h2>
      <p>Conflict registration, rulebook governance, and strategy-driven resolution flows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Strategies</h3>
          <p>{strategies.length > 0 ? strategies.join(", ") : "No strategies loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Conflicts</h3>
          <p>Conflict queue APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Resolutions</h3>
          <p>Resolution workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
