import { useEffect, useState } from "react";

export default function ChatOpsHub() {
  const [channels, setChannels] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/chatops/channels");
        const data = await res.json();
        setChannels((data.items || []).length);
      } catch {
        setChannels(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>ChatOps Hub</h2>
      <p>Team chat integration, slash commands, and command dispatch history.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Connected Channels</h3>
          <p>Channels bound: {channels}</p>
        </article>
        <article className="repo-card">
          <h3>Command Registry</h3>
          <p>Chat command contracts are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Dispatch History</h3>
          <p>Execution history and delivery status are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
