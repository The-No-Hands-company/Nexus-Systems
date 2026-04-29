import { useEffect, useState } from "react";

export default function EventHub() {
  const [channels, setChannels] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/events/channels");
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
      <h2>Event Hub</h2>
      <p>Real-time subscriptions, event streams, and replay controls.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Channels</h3>
          <p>{channels.length > 0 ? channels.join(", ") : "No channels loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Subscriptions</h3>
          <p>Subscription lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Replay and Publish</h3>
          <p>Event replay and publishing contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
