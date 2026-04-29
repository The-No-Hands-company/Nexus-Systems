import { useEffect, useState } from "react";

export default function CommitSigningVaultHub() {
  const [policy, setPolicy] = useState<{ required: boolean; algorithms: string[] }>({ required: false, algorithms: [] });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/commit-signing/policy");
        const data = await res.json();
        setPolicy(data.entity || { required: false, algorithms: [] });
      } catch {
        setPolicy({ required: false, algorithms: [] });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Commit Signing Vault Hub</h2>
      <p>GPG key management, commit signature verification, and signing policy enforcement.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Keys</h3>
          <p>GPG keys are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Verified Commits</h3>
          <p>Verified commits are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Policy</h3>
          <p>Required: {policy.required ? "Yes" : "No"}, Algorithms: {policy.algorithms.join(", ") || "None"}</p>
        </article>
      </div>
    </section>
  );
}
