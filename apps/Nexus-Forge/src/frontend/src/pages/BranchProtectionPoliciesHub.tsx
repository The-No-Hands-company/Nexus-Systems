import { useEffect, useState } from "react";

export default function BranchProtectionPoliciesHub() {
  const [status, setStatus] = useState<{ violations: number; enforced: boolean }>({ violations: 0, enforced: false });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/branch-protection/violations");
        const data = await res.json();
        setStatus(data.entity || { violations: 0, enforced: false });
      } catch {
        setStatus({ violations: 0, enforced: false });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Branch Protection Policies Hub</h2>
      <p>Protection rules, approval gates, and branch enforcement policies.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Policies</h3>
          <p>Protection policies are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Rules</h3>
          <p>Enforcement rules are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Status</h3>
          <p>Enforced: {status.enforced ? "Yes" : "No"}, Violations: {status.violations}</p>
        </article>
      </div>
    </section>
  );
}
