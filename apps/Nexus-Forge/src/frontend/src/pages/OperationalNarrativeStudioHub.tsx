import { useEffect, useState } from "react";

export default function OperationalNarrativeStudioHub() {
  const [stories, setStories] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/operational-narrative/stories");
        const data = await res.json();
        setStories((data.items || []).length);
      } catch {
        setStories(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Operational Narrative Studio Hub</h2>
      <p>Operational story crafting, briefing streams, and audience-aware summaries.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Stories</h3>
          <p>Stories tracked: {stories}</p>
        </article>
        <article className="repo-card">
          <h3>Briefs</h3>
          <p>Operational brief APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Audiences</h3>
          <p>Audience profile workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
