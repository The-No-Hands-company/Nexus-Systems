import { useEffect, useState } from "react";

export default function CustomerLifetimeVaultHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/customer-ltv/metrics");
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
      <h2>Customer Lifetime Vault Hub</h2>
      <p>Lifecycle value profiles, segmentation, and predictive analytics for customer relationships.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Profiles</h3>
          <p>Customer LTV profiles are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Journeys</h3>
          <p>Customer journey analytics are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Metrics</h3>
          <p>{dimensions.join(", ") || "No metrics available"}</p>
        </article>
      </div>
    </section>
  );
}
