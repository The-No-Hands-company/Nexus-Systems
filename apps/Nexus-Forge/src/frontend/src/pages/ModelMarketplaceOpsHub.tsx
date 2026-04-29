import { useEffect, useState } from "react";

export default function ModelMarketplaceOpsHub() {
  const [listings, setListings] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/model-marketplace/listings");
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
      <h2>Model Marketplace Ops Hub</h2>
      <p>Model listing lifecycle, licensing operations, and trust evaluation orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Listings</h3>
          <p>Model listings: {listings}</p>
        </article>
        <article className="repo-card">
          <h3>Licenses</h3>
          <p>Model licensing records are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Evaluations</h3>
          <p>Evaluation and provenance workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
