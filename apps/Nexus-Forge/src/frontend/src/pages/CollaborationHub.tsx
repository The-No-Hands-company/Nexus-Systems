import { useEffect, useState } from "react";

export default function CollaborationHub() {
  const [data, setData] = useState<{ discussions: unknown[]; note?: string }>({ discussions: [] });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/discussions");
        const payload = await res.json();
        setData(payload);
      } catch {
        setData({ discussions: [], note: "Collaboration services are not reachable." });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Collaboration Hub</h2>
      <p>Discussions, announcements, reactions, and social coding tools.</p>
      <p>{data.note || "Collaboration stack is stubbed and ready for implementation."}</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Discussions</h3>
          <p>Active threads: {data.discussions.length}</p>
        </article>
        <article className="repo-card">
          <h3>Announcements</h3>
          <p>Broadcast channel and release messaging stubs are enabled.</p>
        </article>
        <article className="repo-card">
          <h3>Mentions</h3>
          <p>Unified mentions inbox is prepared as API stub.</p>
        </article>
      </div>
    </section>
  );
}
