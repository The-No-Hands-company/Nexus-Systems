import { useEffect, useState } from "react";

export default function PagesHostingHub() {
  const [status, setStatus] = useState<{ source?: string; deploymentHistory?: string }>({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/pages-hosting/deployments");
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
      <h2>Pages Hosting Hub</h2>
      <p>Static hosting parity for project and org pages with custom domains and deployment history.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Source</h3>
          <p>{status.source || "branch-or-ci"}</p>
        </article>
        <article className="repo-card">
          <h3>History</h3>
          <p>{status.deploymentHistory || "stubbed"}</p>
        </article>
        <article className="repo-card">
          <h3>Status</h3>
          <p>Pages hosting API stubs active</p>
        </article>
      </div>
    </section>
  );
}