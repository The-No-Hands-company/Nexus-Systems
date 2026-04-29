import { useEffect, useState } from "react";

export default function ProductExperimentLedgerHub() {
  const [entries, setEntries] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/experiment-ledger/entries");
        const data = await res.json();
        setEntries((data.items || []).length);
      } catch {
        setEntries(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Product Experiment Ledger Hub</h2>
      <p>Experiment history, approvals, outcome tracking, and taxonomy indexing.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Entries</h3>
          <p>Entries tracked: {entries}</p>
        </article>
        <article className="repo-card">
          <h3>Outcomes</h3>
          <p>Outcome feed APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Approvals</h3>
          <p>Approval workflow APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
