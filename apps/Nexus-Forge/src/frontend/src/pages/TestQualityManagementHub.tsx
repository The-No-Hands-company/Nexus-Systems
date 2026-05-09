import { useEffect, useState } from "react";

export default function TestQualityManagementHub() {
  const [coverage, setCoverage] = useState<{
    thresholdPolicy?: string;
    flakiness?: string;
    traceability?: string;
  }>({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/test-quality-management/coverage");
        const data = await res.json();
        setCoverage(data.entity || {});
      } catch {
        setCoverage({});
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Test & Quality Management Hub</h2>
      <p>Managed test plans, coverage gates, flakiness controls, and traceability parity.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Thresholds</h3>
          <p>{coverage.thresholdPolicy || "stubbed"}</p>
        </article>
        <article className="repo-card">
          <h3>Flakiness</h3>
          <p>{coverage.flakiness || "stubbed"}</p>
        </article>
        <article className="repo-card">
          <h3>Traceability</h3>
          <p>{coverage.traceability || "stubbed"}</p>
        </article>
      </div>
    </section>
  );
}
