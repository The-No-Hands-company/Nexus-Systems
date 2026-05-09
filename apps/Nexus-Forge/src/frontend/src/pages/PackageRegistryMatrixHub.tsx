import { useEffect, useState } from "react";

export default function PackageRegistryMatrixHub() {
  const [formats, setFormats] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/package-registry-matrix/formats");
        const data = await res.json();
        setFormats(Array.isArray(data.items) ? data.items : []);
      } catch {
        setFormats([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Package Registry Matrix Hub</h2>
      <p>Unified package hosting parity across language ecosystems and container formats.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Formats</h3>
          <p>{formats.length}</p>
        </article>
        <article className="repo-card">
          <h3>Preview</h3>
          <p>{formats.slice(0, 5).join(", ") || "No formats loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Status</h3>
          <p>Depth parity stubs active</p>
        </article>
      </div>
    </section>
  );
}
