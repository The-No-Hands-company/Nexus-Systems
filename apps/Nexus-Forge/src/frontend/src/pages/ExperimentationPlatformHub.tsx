import { useEffect, useState } from "react";

export default function ExperimentationPlatformHub() {
  const [experiments, setExperiments] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/experiments/catalog");
        const data = await res.json();
        setExperiments((data.items || []).length);
      } catch {
        setExperiments(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Experimentation Platform Hub</h2>
      <p>Experiment registration, variant management, launch control, and insights.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Experiments</h3>
          <p>Registered experiments: {experiments}</p>
        </article>
        <article className="repo-card">
          <h3>Variants</h3>
          <p>Variant orchestration APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Insights</h3>
          <p>Experiment insights feed is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
