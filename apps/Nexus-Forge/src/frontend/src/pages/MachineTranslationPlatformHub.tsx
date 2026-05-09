import { useEffect, useState } from "react";

export default function MachineTranslationPlatformHub() {
  const [quality, setQuality] = useState<{ score: number; languages: number }>({
    score: 0,
    languages: 0,
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/translation/quality");
        const data = await res.json();
        setQuality(data.entity || { score: 0, languages: 0 });
      } catch {
        setQuality({ score: 0, languages: 0 });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Machine Translation Platform Hub</h2>
      <p>Multi-language content translation, quality assessment, and batch processing.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Languages</h3>
          <p>Supported languages are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Projects</h3>
          <p>Translation projects are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Quality</h3>
          <p>
            Score: {quality.score}, Languages: {quality.languages}
          </p>
        </article>
      </div>
    </section>
  );
}
