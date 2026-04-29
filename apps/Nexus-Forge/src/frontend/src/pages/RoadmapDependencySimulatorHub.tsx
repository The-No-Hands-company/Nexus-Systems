import { useEffect, useState } from "react";

export default function RoadmapDependencySimulatorHub() {
  const [roadmaps, setRoadmaps] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/roadmap-simulator/roadmaps");
        const data = await res.json();
        setRoadmaps((data.items || []).length);
      } catch {
        setRoadmaps(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Roadmap Dependency Simulator Hub</h2>
      <p>Roadmap dependency graphing, simulation runs, and delivery outcome analysis.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Roadmaps</h3>
          <p>Roadmaps tracked: {roadmaps}</p>
        </article>
        <article className="repo-card">
          <h3>Dependencies</h3>
          <p>Dependency graph APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Outcomes</h3>
          <p>Simulation outcome feeds are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
