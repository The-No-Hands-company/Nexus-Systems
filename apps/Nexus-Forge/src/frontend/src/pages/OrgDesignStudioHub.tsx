import { useEffect, useState } from "react";

export default function OrgDesignStudioHub() {
  const [topologies, setTopologies] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/org-design/topologies");
        const data = await res.json();
        setTopologies((data.items || []).length);
      } catch {
        setTopologies(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Org Design Studio Hub</h2>
      <p>Organization topology planning, skill mapping, and restructure simulation.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Topologies</h3>
          <p>Topologies tracked: {topologies}</p>
        </article>
        <article className="repo-card">
          <h3>Skills</h3>
          <p>Skill matrix APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Restructures</h3>
          <p>Restructure simulation workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
