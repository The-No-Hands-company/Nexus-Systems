import { useEffect, useState } from "react";

export default function WorkforceSkillMarketplaceHub() {
  const [skills, setSkills] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/workforce-skills/skills");
        const data = await res.json();
        setSkills((data.items || []).length);
      } catch {
        setSkills(0);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Workforce Skill Marketplace Hub</h2>
      <p>Skill listings, demand signals, and pathway-based matching orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Skills</h3>
          <p>Skills tracked: {skills}</p>
        </article>
        <article className="repo-card">
          <h3>Demands</h3>
          <p>Demand signal APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Matches</h3>
          <p>Match orchestration workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
