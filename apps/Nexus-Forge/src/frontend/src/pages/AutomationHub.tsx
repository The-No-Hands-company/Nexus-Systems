import { useEffect, useState } from "react";

export default function AutomationHub() {
  const [rules, setRules] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/automation/rules");
        const payload = await res.json();
        setRules((payload.rules || []).length);
      } catch {
        setRules(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Automation Hub</h2>
      <p>Rule engine, schedules, templates, and event dispatch orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Rules</h3>
          <p>Configured rules: {rules}</p>
        </article>
        <article className="repo-card">
          <h3>Schedules</h3>
          <p>Cron-like scheduled runs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Template Registry</h3>
          <p>Reusable workflow templates are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
