import { useEffect, useState } from "react";

export default function ProcurementHub() {
  const [vendors, setVendors] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/procurement/vendors");
        const data = await res.json();
        setVendors((data.items || []).length);
      } catch {
        setVendors(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Procurement Hub</h2>
      <p>Vendor onboarding, RFP lifecycle, and procurement contract orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Vendors</h3>
          <p>Vendors tracked: {vendors}</p>
        </article>
        <article className="repo-card">
          <h3>RFPs</h3>
          <p>RFP creation and response workflows are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Contracts</h3>
          <p>Procurement contract records are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
