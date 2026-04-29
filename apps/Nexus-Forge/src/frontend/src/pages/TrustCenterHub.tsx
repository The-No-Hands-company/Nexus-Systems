import { useEffect, useState } from "react";

export default function TrustCenterHub() {
  const [certs, setCerts] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/trust/certifications");
        const data = await res.json();
        setCerts(data.entity?.certifications || []);
      } catch {
        setCerts([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Trust Center Hub</h2>
      <p>Certifications, controls, disclosures, and trust reporting workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Certifications</h3>
          <p>{certs.length > 0 ? certs.join(", ") : "No certifications loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Controls</h3>
          <p>Control mapping endpoints are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Disclosures</h3>
          <p>Disclosure feed and trust reports are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
