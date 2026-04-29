import { useEffect, useState } from "react";

export default function ServiceCatalogHub() {
  const [services, setServices] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/services/catalog");
        const data = await res.json();
        setServices((data.items || []).length);
      } catch {
        setServices(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Service Catalog Hub</h2>
      <p>Service registry, dependency graph, ownership map, and SLO controls.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Catalog Entries</h3>
          <p>Services: {services}</p>
        </article>
        <article className="repo-card">
          <h3>Dependencies</h3>
          <p>Service dependency topology APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>SLO and Ownership</h3>
          <p>Ownership and SLO management endpoints are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
