import { useEffect, useState } from "react";

export default function SovereignCloudOpsHub() {
  const [controls, setControls] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/sovereign-cloud/posture");
        const data = await res.json();
        setControls(data.entity?.controls || []);
      } catch {
        setControls([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Sovereign Cloud Ops Hub</h2>
      <p>Residency controls, attestation workflows, and sovereign posture intelligence.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Controls</h3>
          <p>{controls.length > 0 ? controls.join(", ") : "No controls loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Regions</h3>
          <p>Sovereign region registry APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Attestations</h3>
          <p>Attestation workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
