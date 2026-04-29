import { useEffect, useState } from "react";

export default function CustomerSuccessHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/customer-success/health");
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
      <h2>Customer Success Hub</h2>
      <p>Account health, playbooks, adoption interventions, and renewal planning.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Health Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No health dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Playbooks</h3>
          <p>Playbook automation lifecycle is stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Renewals</h3>
          <p>Renewal planning contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
