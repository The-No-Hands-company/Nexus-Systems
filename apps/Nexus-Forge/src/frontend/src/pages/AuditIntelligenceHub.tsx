import { useEffect, useState } from "react";

export default function AuditIntelligenceHub() {
  const [signals, setSignals] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/audit/risk-signals");
        const data = await res.json();
        setSignals(data.entity?.signals || []);
      } catch {
        setSignals([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Audit Intelligence Hub</h2>
      <p>Audit evidence streams, findings, timelines, and risk signal analytics.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Risk Signals</h3>
          <p>{signals.length > 0 ? signals.join(", ") : "No signals loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Findings</h3>
          <p>Finding lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Timelines</h3>
          <p>Investigation timeline reconstruction is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
