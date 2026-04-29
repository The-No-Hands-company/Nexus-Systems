import { useEffect, useState } from "react";

export default function CiCdFullDepthHub() {
  const [status, setStatus] = useState<{ environments?: string[]; approvals?: string }>({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/ci-cd-full-depth/deploy-tracking");
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
      <h2>CI/CD Full Depth Hub</h2>
      <p>Hosted runners, matrix pipelines, review apps, and deployment controls parity.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Coverage</h3>
          <p>Runner and pipeline depth stubs are registered</p>
        </article>
        <article className="repo-card">
          <h3>Environments</h3>
          <p>{(status.environments || []).join(", ") || "none"}</p>
        </article>
        <article className="repo-card">
          <h3>Approvals</h3>
          <p>{status.approvals || "stubbed"}</p>
        </article>
      </div>
    </section>
  );
}