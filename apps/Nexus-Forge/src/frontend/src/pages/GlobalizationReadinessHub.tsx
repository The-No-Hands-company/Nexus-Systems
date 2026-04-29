import { useEffect, useState } from "react";

export default function GlobalizationReadinessHub() {
  const [locales, setLocales] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/globalization/locales");
        const data = await res.json();
        setLocales((data.items || []).length);
      } catch {
        setLocales(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Globalization Readiness Hub</h2>
      <p>Locale rollout readiness, compliance checks, and global launch checkpoints.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Locales</h3>
          <p>Locales tracked: {locales}</p>
        </article>
        <article className="repo-card">
          <h3>Compliance</h3>
          <p>Regional compliance APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Checkpoints</h3>
          <p>Global rollout checkpoint workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
