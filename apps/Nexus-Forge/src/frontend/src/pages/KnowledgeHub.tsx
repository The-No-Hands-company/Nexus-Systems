import { useEffect, useState } from "react";

export default function KnowledgeHub() {
  const [wikiCount, setWikiCount] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/wiki/pages");
        const payload = await res.json();
        setWikiCount((payload.pages || []).length);
      } catch {
        setWikiCount(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Knowledge Hub</h2>
      <p>Wiki, docs hosting, snippets, and architecture decision records.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Wiki</h3>
          <p>Pages: {wikiCount}</p>
        </article>
        <article className="repo-card">
          <h3>Docs Hosting</h3>
          <p>Static docs deployment pipeline is stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Snippets and ADR</h3>
          <p>Knowledge primitives are wired through API stubs.</p>
        </article>
      </div>
    </section>
  );
}
