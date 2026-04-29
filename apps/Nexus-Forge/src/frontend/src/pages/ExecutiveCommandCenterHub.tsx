import { useEffect, useState } from "react";

export default function ExecutiveCommandCenterHub() {
  const [channels, setChannels] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/executive/signals");
        const data = await res.json();
        setChannels(data.entity?.channels || []);
      } catch {
        setChannels([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Executive Command Center Hub</h2>
      <p>Executive briefings, decision logs, and signal channels for portfolio steering.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Signal Channels</h3>
          <p>{channels.length > 0 ? channels.join(", ") : "No channels loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Briefings</h3>
          <p>Briefing feed and narrative APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Decisions</h3>
          <p>Decision tracking and approvals are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
