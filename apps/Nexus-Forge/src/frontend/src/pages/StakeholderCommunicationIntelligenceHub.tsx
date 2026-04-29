import { useEffect, useState } from "react";

export default function StakeholderCommunicationIntelligenceHub() {
  const [signals, setSignals] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/stakeholder-comms/signals");
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
      <h2>Stakeholder Communication Intelligence Hub</h2>
      <p>Audience strategy, briefing flows, and signal-driven communication recommendations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Signals</h3>
          <p>{signals.length > 0 ? signals.join(", ") : "No communication signals loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Audiences</h3>
          <p>Audience definition APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Recommendations</h3>
          <p>Recommendation workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
