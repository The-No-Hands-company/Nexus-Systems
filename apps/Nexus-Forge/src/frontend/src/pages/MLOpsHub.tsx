import { useEffect, useState } from "react";

export default function MLOpsHub() {
  const [models, setModels] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/ml/models");
        const data = await res.json();
        setModels((data.items || []).length);
      } catch {
        setModels(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>MLOps Hub</h2>
      <p>Model registry, experiments, datasets, serving, and drift monitoring.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Models</h3>
          <p>Registered models: {models}</p>
        </article>
        <article className="repo-card">
          <h3>Experiments</h3>
          <p>Experiment tracking API is stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Serving and Drift</h3>
          <p>Online serving and monitoring contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
