import { useEffect, useState } from "react";

export default function SustainabilityCommandCenterHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/sustainability/dimensions");
        const data = await res.json();
        setDimensions(data.entity?.dimensions || []);
      } catch {
        setDimensions([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Sustainability Command Center Hub</h2>
      <p>Sustainability initiatives, footprint telemetry, and offset orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No sustainability dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Initiatives</h3>
          <p>Initiative planning APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Offsets</h3>
          <p>Offset workflow contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
