import { useEffect, useState } from "react";

export default function QuantumResilienceLabHub() {
  const [phases, setPhases] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/quantum-resilience/readiness");
        const data = await res.json();
        setPhases(data.entity?.phases || []);
      } catch {
        setPhases([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Quantum Resilience Lab Hub</h2>
      <p>Post-quantum migration readiness, control cataloging, and resilience exercises.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Readiness Phases</h3>
          <p>{phases.length > 0 ? phases.join(", ") : "No readiness phases loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Controls</h3>
          <p>Post-quantum control APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Exercises</h3>
          <p>Resilience exercise workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
