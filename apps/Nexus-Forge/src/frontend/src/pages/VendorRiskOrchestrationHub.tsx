import { useEffect, useState } from "react";

export default function VendorRiskOrchestrationHub() {
  const [categories, setCategories] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/vendor-risk/posture");
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
      <h2>Vendor Risk Orchestration Hub</h2>
      <p>Vendor onboarding, risk assessments, and mitigation planning workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Risk Categories</h3>
          <p>{categories.length > 0 ? categories.join(", ") : "No risk categories loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Vendors</h3>
          <p>Vendor registry APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Mitigations</h3>
          <p>Mitigation workflow APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
