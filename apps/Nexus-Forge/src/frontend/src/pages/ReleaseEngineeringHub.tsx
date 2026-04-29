import { useEffect, useState } from "react";

export default function ReleaseEngineeringHub() {
  const [channels, setChannels] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/release/channels");
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
      <h2>Release Engineering Hub</h2>
      <p>Release trains, promotion controls, candidate flow, and notes automation.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Channels</h3>
          <p>{channels.length > 0 ? channels.join(", ") : "No channels loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Release Trains</h3>
          <p>Train scheduling contracts are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Promotion</h3>
          <p>Promotion and release notes workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
