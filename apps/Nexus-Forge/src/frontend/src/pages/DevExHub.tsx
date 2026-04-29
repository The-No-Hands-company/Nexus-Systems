import { useEffect, useState } from "react";

export default function DevExHub() {
  const [journeys, setJourneys] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/devex/onboarding");
        const data = await res.json();
        setJourneys(data.entity?.journeys || []);
      } catch {
        setJourneys([]);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Developer Experience Hub</h2>
      <p>Onboarding journeys, starters, scorecards, and productivity guidance.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Journeys</h3>
          <p>{journeys.length > 0 ? journeys.join(", ") : "No onboarding journeys loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Starters</h3>
          <p>Starter template lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Recommendations</h3>
          <p>Scorecards and recommendations are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
