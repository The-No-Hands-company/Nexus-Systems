import { useEffect, useState } from "react";

export default function RepositoryMirrorFederationHub() {
  const [status, setStatus] = useState<{ providers?: string[]; syncState?: string }>({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/repository-mirror-federation/status");
        const data = await res.json();
        setStatus(data.entity || {});
      } catch {
        setStatus({});
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Repository Mirror Federation Hub</h2>
      <p>Push/pull mirrors, migration parity, and sync health across forge providers.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Providers</h3>
          <p>{(status.providers || []).join(", ") || "none"}</p>
        </article>
        <article className="repo-card">
          <h3>Sync State</h3>
          <p>{status.syncState || "stubbed"}</p>
        </article>
        <article className="repo-card">
          <h3>Scope</h3>
          <p>Mirrors + migrations are stubbed</p>
        </article>
      </div>
    </section>
  );
}
