import { useEffect, useState } from "react";

export default function CrossTeamDependencyGraphHub() {
  const [health, setHealth] = useState<{ status: string; criticalDeps: number }>({ status: "", criticalDeps: 0 });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/team-dependencies/health");
        const data = await res.json();
        setHealth(data.entity || { status: "", criticalDeps: 0 });
      } catch {
        setHealth({ status: "", criticalDeps: 0 });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Cross-Team Dependency Graph Hub</h2>
      <p>Team dependency mapping, blocking dependency detection, and resolution orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Dependency Graph</h3>
          <p>Team dependencies are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Blockers</h3>
          <p>Blocking dependencies are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Health</h3>
          <p>Status: {health.status}, Critical: {health.criticalDeps}</p>
        </article>
      </div>
    </section>
  );
}
