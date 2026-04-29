import { useEffect, useState } from "react";

interface Experiment {
  id: string;
  title: string;
  status: string;
}

export default function FutureLabHub() {
  const [experiments, setExperiments] = useState<Experiment[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/labs/experiments");
        const data = await res.json();
        setExperiments(data.experiments || []);
      } catch {
        setExperiments([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Future Labs</h2>
      <p>Experimental capabilities and next-gen workflows for Nexus Forge.</p>
      <div className="repo-grid">
        {experiments.length === 0 ? (
          <article className="repo-card">
            <h3>No experiments loaded</h3>
            <p>Labs API is currently unavailable or still in stub mode.</p>
          </article>
        ) : (
          experiments.map((exp) => (
            <article key={exp.id} className="repo-card">
              <h3>{exp.title}</h3>
              <p>Experiment ID: {exp.id}</p>
              <span className="badge">{exp.status}</span>
            </article>
          ))
        )}
      </div>
    </section>
  );
}
