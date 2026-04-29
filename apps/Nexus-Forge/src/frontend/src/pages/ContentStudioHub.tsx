import { useEffect, useState } from "react";

export default function ContentStudioHub() {
  const [templates, setTemplates] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/content/templates");
        const data = await res.json();
        setTemplates((data.items || []).length);
      } catch {
        setTemplates(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Content Studio Hub</h2>
      <p>Template authoring, asset management, and collaborative content workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Templates</h3>
          <p>Templates available: {templates}</p>
        </article>
        <article className="repo-card">
          <h3>Assets</h3>
          <p>Asset library APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Workflows</h3>
          <p>Content approval workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
