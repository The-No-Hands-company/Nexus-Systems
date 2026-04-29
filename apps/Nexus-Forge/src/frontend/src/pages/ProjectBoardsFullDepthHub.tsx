import { useEffect, useState } from "react";

export default function ProjectBoardsFullDepthHub() {
  const [views, setViews] = useState<string[]>([]);
  const [metrics, setMetrics] = useState<{ burndown?: string; velocity?: string; deliveryPlans?: string }>({});

  useEffect(() => {
    async function load() {
      try {
        const [viewsRes, metricsRes] = await Promise.all([
          fetch("/api/project-boards-full-depth/views"),
          fetch("/api/project-boards-full-depth/metrics"),
        ]);
        const [viewsData, metricsData] = await Promise.all([viewsRes.json(), metricsRes.json()]);
        setViews(Array.isArray(viewsData.items) ? viewsData.items : []);
        setMetrics(metricsData.entity || {});
      } catch {
        setViews([]);
        setMetrics({});
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Project Boards Full Depth Hub</h2>
      <p>Kanban, table, roadmap, and portfolio parity with board automation and metrics.</p>
      <div className="repo-grid">
        <article className="repo-card"><h3>Views</h3><p>{views.join(", ") || "none"}</p></article>
        <article className="repo-card"><h3>Burndown</h3><p>{metrics.burndown || "stubbed"}</p></article>
        <article className="repo-card"><h3>Velocity</h3><p>{metrics.velocity || "stubbed"}</p></article>
      </div>
    </section>
  );
}