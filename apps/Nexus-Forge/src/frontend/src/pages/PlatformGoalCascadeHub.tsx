import { useEffect, useState } from "react";

export default function PlatformGoalCascadeHub() {
  const [objectives, setObjectives] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/goal-cascade/objectives");
        const data = await res.json();
        setObjectives((data.items || []).length);
      } catch {
        setObjectives(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Platform Goal Cascade Hub</h2>
      <p>Objective hierarchy mapping, dependency analysis, and alignment checks.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Objectives</h3>
          <p>Objectives tracked: {objectives}</p>
        </article>
        <article className="repo-card">
          <h3>Dependencies</h3>
          <p>Dependency mapping APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Alignment</h3>
          <p>Alignment check workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
