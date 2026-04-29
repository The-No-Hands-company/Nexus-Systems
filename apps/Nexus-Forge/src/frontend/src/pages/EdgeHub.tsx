import { useEffect, useState } from "react";

export default function EdgeHub() {
  const [region, setRegion] = useState("unknown");

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/edge/regions");
        const data = await res.json();
        setRegion(data.entity?.activeRegion || "unknown");
      } catch {
        setRegion("unknown");
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Edge Hub</h2>
      <p>Global regions, replica topology, traffic routing, and cache invalidation.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Active Region</h3>
          <p>{region}</p>
        </article>
        <article className="repo-card">
          <h3>Replica Mesh</h3>
          <p>Replica registration and placement are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Traffic Control</h3>
          <p>Routing policy contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
