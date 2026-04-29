import { useEffect, useState } from "react";

export default function LegalOpsHub() {
  const [contracts, setContracts] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/legal/contracts");
        const data = await res.json();
        setContracts((data.items || []).length);
      } catch {
        setContracts(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Legal Operations Hub</h2>
      <p>Contract lifecycle, approvals, legal holds, and e-discovery operations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Contracts</h3>
          <p>Contracts tracked: {contracts}</p>
        </article>
        <article className="repo-card">
          <h3>Approvals</h3>
          <p>Legal approval routing is stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Discovery</h3>
          <p>E-discovery and legal hold workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
