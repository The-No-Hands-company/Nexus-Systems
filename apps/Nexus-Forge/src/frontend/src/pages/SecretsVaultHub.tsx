import { useEffect, useState } from "react";

export default function SecretsVaultHub() {
  const [providers, setProviders] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/secrets/providers");
        const data = await res.json();
        setProviders(data.entity?.providers || []);
      } catch {
        setProviders([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Secrets Vault Hub</h2>
      <p>Secret provider integration, namespace governance, and rotation automation.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Providers</h3>
          <p>{providers.length > 0 ? providers.join(", ") : "No providers loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Namespaces</h3>
          <p>Namespace lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Rotation</h3>
          <p>Rotation workflows and schedules are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
