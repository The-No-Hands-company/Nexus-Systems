import { useEffect, useState } from "react";

export default function PlatformTrustAutomationHub() {
  const [signals, setSignals] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/platform-trust/signals");
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
      <h2>Platform Trust Automation Hub</h2>
      <p>Trust rules, verification jobs, remediation actions, and trust signal operations.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Signals</h3>
          <p>{signals.length > 0 ? signals.join(", ") : "No trust signals loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Rules</h3>
          <p>Trust rule lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Remediations</h3>
          <p>Remediation workflow APIs are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
