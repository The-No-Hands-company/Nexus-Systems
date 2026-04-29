import { useEffect, useState } from "react";

export default function ApiHub() {
  const [keys, setKeys] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/api-management/keys");
        const data = await res.json();
        setKeys((data.items || []).length);
      } catch {
        setKeys(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>API Hub</h2>
      <p>API keys, OAuth clients, quotas, and access audit trail.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Access Keys</h3>
          <p>Issued keys: {keys}</p>
        </article>
        <article className="repo-card">
          <h3>Rate Limits</h3>
          <p>Rate limiting policy contracts are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>OAuth Clients</h3>
          <p>OAuth app lifecycle endpoints are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
