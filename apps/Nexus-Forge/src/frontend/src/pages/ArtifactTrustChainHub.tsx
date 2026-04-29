import { useEffect, useState } from "react";

export default function ArtifactTrustChainHub() {
  const [artifacts, setArtifacts] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/artifact-trust/artifacts");
        const data = await res.json();
        setArtifacts((data.items || []).length);
      } catch {
        setArtifacts(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Artifact Trust Chain Hub</h2>
      <p>Artifact attestations, verification jobs, and provenance graph orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Artifacts</h3>
          <p>Artifacts tracked: {artifacts}</p>
        </article>
        <article className="repo-card">
          <h3>Attestations</h3>
          <p>Attestation ledger APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Provenance</h3>
          <p>Provenance graph and trust workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
