import { useEffect, useState } from "react";

export default function InteropHub() {
  const [providers, setProviders] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/interop/providers");
        const data = await res.json();
        setProviders(data.providers || []);
      } catch {
        setProviders([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Interoperability Hub</h2>
      <p>Migration from other forges, mirrors, and external token workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Providers</h3>
          <p>{providers.length > 0 ? providers.join(", ") : "No providers loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Migrations</h3>
          <p>Migration job orchestration is stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Mirrors and Tokens</h3>
          <p>Mirror topology and interop auth are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
