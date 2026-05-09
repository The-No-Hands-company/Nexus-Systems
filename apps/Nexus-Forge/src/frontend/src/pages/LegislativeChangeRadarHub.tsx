import { useEffect, useState } from "react";

export default function LegislativeChangeRadarHub() {
  const [categories, setCategories] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/legislative-radar/categories");
        const data = await res.json();
        setCategories(data.entity?.categories || []);
      } catch {
        setCategories([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Legislative Change Radar Hub</h2>
      <p>Legislative feed monitoring, impact assessment, and regulatory category tracking.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Categories</h3>
          <p>
            {categories.length > 0 ? categories.join(", ") : "No legislative categories loaded"}
          </p>
        </article>
        <article className="repo-card">
          <h3>Jurisdictions</h3>
          <p>Jurisdiction registry APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Impacts</h3>
          <p>Impact assessment workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
