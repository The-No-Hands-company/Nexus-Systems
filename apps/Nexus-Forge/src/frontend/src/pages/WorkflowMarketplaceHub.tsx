import { useEffect, useState } from "react";

export default function WorkflowMarketplaceHub() {
  const [listings, setListings] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/workflow-marketplace/listings");
        const data = await res.json();
        setListings((data.items || []).length);
      } catch {
        setListings(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Workflow Marketplace Hub</h2>
      <p>Workflow listing, purchase, subscription, and rating operations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Listings</h3>
          <p>Workflow listings: {listings}</p>
        </article>
        <article className="repo-card">
          <h3>Purchases</h3>
          <p>Purchase and subscription flows are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Ratings</h3>
          <p>Ratings and reviews are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
