import { useEffect, useState } from "react";

export default function BrandConsistencyGuardHub() {
  const [deviations, setDeviations] = useState<{ violations: number; compliant: boolean }>({
    violations: 0,
    compliant: false,
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/brand-consistency/deviations");
        const data = await res.json();
        setDeviations(data.entity || { violations: 0, compliant: false });
      } catch {
        setDeviations({ violations: 0, compliant: false });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Brand Consistency Guard Hub</h2>
      <p>Brand asset library, guideline compliance, and visual identity enforcement.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Assets</h3>
          <p>Brand assets are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Guidelines</h3>
          <p>Brand standards are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Status</h3>
          <p>
            Compliant: {deviations.compliant ? "Yes" : "No"}, Violations: {deviations.violations}
          </p>
        </article>
      </div>
    </section>
  );
}
