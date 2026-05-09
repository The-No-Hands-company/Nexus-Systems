import { useEffect, useState } from "react";
import { Link } from "react-router-dom";

interface DomainItem {
  name: string;
  description: string;
  state: string;
  capabilities: string[];
}

export default function PlatformHub() {
  const [domains, setDomains] = useState<DomainItem[]>([]);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/domains");
        const data = await res.json();
        setDomains(data.domains || []);
      } catch {
        setDomains([]);
      } finally {
        setLoading(false);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Platform Hub</h2>
      <p>Explore all major Nexus Forge platform domains and their implementation stubs.</p>
      {loading ? <p>Loading platform domains…</p> : null}
      {!loading && domains.length === 0 ? <p>No domain metadata is available yet.</p> : null}

      <div className="repo-grid">
        {domains.map((domain) => (
          <article key={domain.name} className="repo-card">
            <h3>{domain.name}</h3>
            <p>{domain.description}</p>
            <p>Capabilities: {domain.capabilities.join(", ")}</p>
            <span className="badge">{domain.state}</span>
            <p>
              <Link to={`/platform/${domain.name}`}>Open workbench</Link>
            </p>
          </article>
        ))}
      </div>
    </section>
  );
}
