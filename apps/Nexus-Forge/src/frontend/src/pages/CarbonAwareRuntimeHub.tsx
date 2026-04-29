import { useEffect, useState } from "react";

export default function CarbonAwareRuntimeHub() {
  const [modes, setModes] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/carbon-runtime/modes");
        const data = await res.json();
        setModes(data.entity?.modes || []);
      } catch {
        setModes([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Carbon-Aware Runtime Hub</h2>
      <p>Runtime carbon policies, scheduling controls, and carbon intensity signal routing.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Modes</h3>
          <p>{modes.length > 0 ? modes.join(", ") : "No runtime modes loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Policies</h3>
          <p>Carbon policy APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Schedules</h3>
          <p>Carbon scheduling workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
