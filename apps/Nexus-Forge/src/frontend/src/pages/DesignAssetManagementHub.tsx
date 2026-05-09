import { useEffect, useState } from "react";

export default function DesignAssetManagementHub() {
  const [versioning, setVersioning] = useState<{ diffOverlay?: string; versionHistory?: string }>(
    {},
  );

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/design-asset-management/versioning");
        const data = await res.json();
        setVersioning(data.entity || {});
      } catch {
        setVersioning({});
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Design & Asset Management Hub</h2>
      <p>Design uploads, visual reviews, and version diff parity for design workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Diff Overlay</h3>
          <p>{versioning.diffOverlay || "stubbed"}</p>
        </article>
        <article className="repo-card">
          <h3>Version History</h3>
          <p>{versioning.versionHistory || "stubbed"}</p>
        </article>
        <article className="repo-card">
          <h3>Status</h3>
          <p>Design parity stubs active</p>
        </article>
      </div>
    </section>
  );
}
