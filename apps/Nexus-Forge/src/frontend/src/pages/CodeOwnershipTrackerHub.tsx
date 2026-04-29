import { useEffect, useState } from "react";

export default function CodeOwnershipTrackerHub() {
  const [coverage, setCoverage] = useState<{ coverage: number; orphaned: number }>({ coverage: 0, orphaned: 0 });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/codeowners/coverage");
        const data = await res.json();
        setCoverage(data.entity || { coverage: 0, orphaned: 0 });
      } catch {
        setCoverage({ coverage: 0, orphaned: 0 });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Code Ownership Tracker Hub</h2>
      <p>CODEOWNERS file management, path-to-owner mapping, and responsibility tracking.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>CODEOWNERS</h3>
          <p>Ownership files are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Mappings</h3>
          <p>Path mappings are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Coverage</h3>
          <p>Coverage: {coverage.coverage}%, Orphaned: {coverage.orphaned}</p>
        </article>
      </div>
    </section>
  );
}
