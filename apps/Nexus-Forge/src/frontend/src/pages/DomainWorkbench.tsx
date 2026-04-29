import { Link, useParams } from "react-router-dom";
import { useEffect, useState } from "react";

interface DomainResponse {
  domain?: {
    name: string;
    description: string;
    capabilities: string[];
    endpoints: string[];
    state: string;
  };
  workbench?: {
    status: string;
    nextMilestones: string[];
  };
  error?: string;
}

export default function DomainWorkbench() {
  const { domain } = useParams();
  const [payload, setPayload] = useState<DomainResponse | null>(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    async function load() {
      if (!domain) return;
      try {
        const res = await fetch(`/api/domains/${domain}`);
        const data = (await res.json()) as DomainResponse;
        setPayload(data);
      } catch {
        setPayload({ error: "Failed to load domain workbench." });
      } finally {
        setLoading(false);
      }
    }

    load();
  }, [domain]);

  return (
    <section>
      <p>
        <Link to="/platform">Back to Platform Hub</Link>
      </p>
      <h2>Domain Workbench: {domain}</h2>
      {loading ? <p>Loading domain details…</p> : null}
      {payload?.error ? <p>{payload.error}</p> : null}
      {payload?.domain ? (
        <>
          <p>{payload.domain.description}</p>
          <div className="repo-grid">
            <article className="repo-card">
              <h3>State</h3>
              <span className="badge">{payload.domain.state}</span>
            </article>
            <article className="repo-card">
              <h3>Capabilities</h3>
              <p>{payload.domain.capabilities.join(", ")}</p>
            </article>
            <article className="repo-card">
              <h3>Endpoints</h3>
              <p>{payload.domain.endpoints.join(" | ")}</p>
            </article>
          </div>

          <div className="activity-panel">
            <h3>Next Milestones</h3>
            <ul>
              {(payload.workbench?.nextMilestones || []).map((item) => (
                <li key={item}>{item}</li>
              ))}
            </ul>
          </div>
        </>
      ) : null}
    </section>
  );
}
