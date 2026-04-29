import { useEffect, useState } from "react";

export default function ContractTestingExchangeHub() {
  const [contracts, setContracts] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/contract-testing/contracts");
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
      <h2>Contract Testing Exchange Hub</h2>
      <p>Shared contract suites, execution lifecycle, and compatibility intelligence.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Contracts</h3>
          <p>Contracts tracked: {contracts}</p>
        </article>
        <article className="repo-card">
          <h3>Suites</h3>
          <p>Suite catalog APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Compatibility</h3>
          <p>Compatibility matrix workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
