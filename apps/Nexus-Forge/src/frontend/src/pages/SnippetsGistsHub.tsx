import { useEffect, useState } from "react";

export default function SnippetsGistsHub() {
  const [capabilities, setCapabilities] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/snippets-gists/capabilities");
        const data = await res.json();
        setCapabilities(Array.isArray(data.items) ? data.items : []);
      } catch {
        setCapabilities([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Snippets & Gists Hub</h2>
      <p>Public/internal/secret snippet workflows with versioning and embed parity.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Capabilities</h3>
          <p>{capabilities.length}</p>
        </article>
        <article className="repo-card">
          <h3>Status</h3>
          <p>Snippets and gists routes stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Next</h3>
          <p>UI editing and gist API compatibility</p>
        </article>
      </div>
    </section>
  );
}
