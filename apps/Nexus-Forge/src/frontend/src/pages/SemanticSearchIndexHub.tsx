import { useEffect, useState } from "react";

export default function SemanticSearchIndexHub() {
  const [stats, setStats] = useState<{ indexed: number; ready: boolean }>({
    indexed: 0,
    ready: false,
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/semantic-search/stats");
        const data = await res.json();
        setStats(data.entity || { indexed: 0, ready: false });
      } catch {
        setStats({ indexed: 0, ready: false });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Semantic Search Index Hub</h2>
      <p>Semantic content indexing, natural language queries, and knowledge discovery.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Index</h3>
          <p>Search index is stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Queries</h3>
          <p>Query history is stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Stats</h3>
          <p>
            Indexed: {stats.indexed}, Ready: {stats.ready ? "Yes" : "No"}
          </p>
        </article>
      </div>
    </section>
  );
}
