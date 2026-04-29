import { useEffect, useState } from "react";

export default function PartnerEcosystemHub() {
  const [tiers, setTiers] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/partners/tiering");
        const data = await res.json();
        setTiers(data.entity?.tiers || []);
      } catch {
        setTiers([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Partner Ecosystem Hub</h2>
      <p>Partner directory, program operations, deal registration, and tier strategy.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Tiers</h3>
          <p>{tiers.length > 0 ? tiers.join(", ") : "No partner tiers loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Programs</h3>
          <p>Program lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Deals</h3>
          <p>Deal registration and tracking are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
