import { useEffect, useState } from "react";

export default function IssueManagementFullDepthHub() {
  const [counts, setCounts] = useState({ metadata: 0, hierarchy: 0, iterations: 0 });

  useEffect(() => {
    async function load() {
      try {
        const [metaRes, hierRes, iterRes] = await Promise.all([
          fetch("/api/issue-management-full-depth/metadata"),
          fetch("/api/issue-management-full-depth/hierarchy"),
          fetch("/api/issue-management-full-depth/iterations"),
        ]);
        const [meta, hier, iter] = await Promise.all([metaRes.json(), hierRes.json(), iterRes.json()]);
        setCounts({
          metadata: Array.isArray(meta.items) ? meta.items.length : 0,
          hierarchy: Array.isArray(hier.items) ? hier.items.length : 0,
          iterations: Array.isArray(iter.items) ? iter.items.length : 0,
        });
      } catch {
        setCounts({ metadata: 0, hierarchy: 0, iterations: 0 });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Issue Management Full Depth Hub</h2>
      <p>Deep issue parity covering metadata, hierarchy, and iteration controls.</p>
      <div className="repo-grid">
        <article className="repo-card"><h3>Metadata</h3><p>{counts.metadata}</p></article>
        <article className="repo-card"><h3>Hierarchy</h3><p>{counts.hierarchy}</p></article>
        <article className="repo-card"><h3>Iterations</h3><p>{counts.iterations}</p></article>
      </div>
    </section>
  );
}