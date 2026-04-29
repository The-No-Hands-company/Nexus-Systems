import { useEffect, useState } from "react";

export default function AutonomousBacklogTriageHub() {
  const [classes, setClasses] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/backlog/priorities");
        const data = await res.json();
        setClasses(data.entity?.classes || []);
      } catch {
        setClasses([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Autonomous Backlog Triage Hub</h2>
      <p>Backlog intake, autonomous triage, and priority class intelligence.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Priority Classes</h3>
          <p>{classes.length > 0 ? classes.join(", ") : "No classes loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Intake Queue</h3>
          <p>Backlog intake APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Classification</h3>
          <p>Autonomous classification contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
