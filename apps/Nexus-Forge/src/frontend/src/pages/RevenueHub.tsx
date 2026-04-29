import { useEffect, useState } from "react";

export default function RevenueHub() {
  const [plans, setPlans] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/billing/plans");
        const data = await res.json();
        setPlans((data.plans || []).length);
      } catch {
        setPlans(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Revenue Hub</h2>
      <p>Pricing, subscriptions, invoices, and usage metering stubs.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Plan Catalog</h3>
          <p>Plans available: {plans}</p>
        </article>
        <article className="repo-card">
          <h3>Subscriptions</h3>
          <p>Subscription lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Usage and Quotas</h3>
          <p>Metering and limits are represented as contracts.</p>
        </article>
      </div>
    </section>
  );
}
