import { useEffect, useState } from "react";

export default function FeatureGatingOrchestratorHub() {
  const [health, setHealth] = useState<{ status: string; activeFlags: number }>({
    status: "",
    activeFlags: 0,
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/feature-gating/health");
        const data = await res.json();
        setHealth(data.entity || { status: "", activeFlags: 0 });
      } catch {
        setHealth({ status: "", activeFlags: 0 });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Feature Gating Orchestrator Hub</h2>
      <p>Feature flags, rollout campaigns, and progressive deployment orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Feature Flags</h3>
          <p>Flags are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Rollouts</h3>
          <p>Deployment campaigns are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Health</h3>
          <p>
            Status: {health.status}, Active: {health.activeFlags}
          </p>
        </article>
      </div>
    </section>
  );
}
