import { useEffect, useState } from "react";

export default function LicenseComplianceTrackerHub() {
  const [actions, setActions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/license-compliance/recommendations");
        const data = await res.json();
        setActions(data.entity?.actions || []);
      } catch {
        setActions([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>License Compliance Tracker Hub</h2>
      <p>Software license inventory, compliance violation detection, and remediation recommendations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>License Inventory</h3>
          <p>Software licenses are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Violations</h3>
          <p>Compliance violations are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Actions</h3>
          <p>{actions.join(", ") || "No violations detected"}</p>
        </article>
      </div>
    </section>
  );
}
