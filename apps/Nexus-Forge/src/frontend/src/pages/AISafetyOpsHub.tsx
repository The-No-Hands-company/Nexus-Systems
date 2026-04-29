import { useEffect, useState } from "react";

export default function AISafetyOpsHub() {
  const [controls, setControls] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/ai-safety/controls");
        const data = await res.json();
        setControls(data.entity?.controls || []);
      } catch {
        setControls([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>AI Safety Ops Hub</h2>
      <p>Model safety controls, policy governance, and incident operations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Controls</h3>
          <p>{controls.length > 0 ? controls.join(", ") : "No controls loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Policies</h3>
          <p>Safety policy lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Evaluations</h3>
          <p>Safety evaluations and incidents are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
