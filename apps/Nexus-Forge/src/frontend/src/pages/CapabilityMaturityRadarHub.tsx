import { useEffect, useState } from "react";

export default function CapabilityMaturityRadarHub() {
  const [bands, setBands] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/maturity/radar");
        const data = await res.json();
        setBands(data.entity?.bands || []);
      } catch {
        setBands([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Capability Maturity Radar Hub</h2>
      <p>Maturity assessments, roadmap planning, and radar-based capability posture.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Bands</h3>
          <p>{bands.length > 0 ? bands.join(", ") : "No maturity bands loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Domains</h3>
          <p>Capability domain inventory APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Roadmaps</h3>
          <p>Maturity roadmap planning APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
