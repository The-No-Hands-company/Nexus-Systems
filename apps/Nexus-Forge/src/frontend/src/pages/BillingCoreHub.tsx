import { useEffect, useState } from "react";

export default function BillingCoreHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/billing/meters");
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
      <h2>Billing Core Hub</h2>
      <p>Plans, invoices, and metering primitives for platform monetization foundations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Metering Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No billing dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Plans</h3>
          <p>Plan lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Invoices</h3>
          <p>Invoice generation and tracking are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
