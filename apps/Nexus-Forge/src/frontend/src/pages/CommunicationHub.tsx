import { useEffect, useState } from "react";

export default function CommunicationHub() {
  const [campaigns, setCampaigns] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/comms/campaigns");
        const data = await res.json();
        setCampaigns((data.items || []).length);
      } catch {
        setCampaigns(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Communication Hub</h2>
      <p>Campaign orchestration, templates, outbound dispatch, and delivery analytics.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Campaigns</h3>
          <p>Campaigns tracked: {campaigns}</p>
        </article>
        <article className="repo-card">
          <h3>Templates</h3>
          <p>Template registry and reuse contracts are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Deliveries</h3>
          <p>Dispatch and delivery tracking are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
