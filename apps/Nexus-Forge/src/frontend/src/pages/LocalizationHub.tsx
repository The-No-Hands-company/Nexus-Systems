import { useEffect, useState } from "react";

export default function LocalizationHub() {
  const [locales, setLocales] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/localization/locales");
        const data = await res.json();
        setLocales(data.entity?.supported || []);
      } catch {
        setLocales([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Localization Hub</h2>
      <p>Internationalization, translation review, and locale release workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Supported Locales</h3>
          <p>{locales.length > 0 ? locales.join(", ") : "No locales loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Translation Keys</h3>
          <p>String catalog management APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Review Queue</h3>
          <p>Localization QA workflow contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
