import { useEffect, useState } from "react";

export default function AutonomousReleaseGovernorHub() {
  const [signals, setSignals] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/release-governor/signals");
        const data = await res.json();
        setSignals(data.entity?.signals || []);
      } catch {
        setSignals([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Autonomous Release Governor Hub</h2>
      <p>Policy-driven release decisions, evaluations, and signal-based governance.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Signals</h3>
          <p>{signals.length > 0 ? signals.join(", ") : "No release signals loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Policies</h3>
          <p>Release policy APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Decisions</h3>
          <p>Decision and evaluation workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
