import { useEffect, useState } from "react";

export default function DisasterRecoveryHub() {
  const [posture, setPosture] = useState<{ rtoMinutes?: number; rpoMinutes?: number }>({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/dr/posture");
        const data = await res.json();
        setPosture(data.entity || {});
      } catch {
        setPosture({});
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Disaster Recovery Hub</h2>
      <p>Recovery planning, exercises, failover controls, and posture tracking.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Posture</h3>
          <p>RTO {posture.rtoMinutes ?? "-"}m / RPO {posture.rpoMinutes ?? "-"}m</p>
        </article>
        <article className="repo-card">
          <h3>Plans</h3>
          <p>Recovery plan lifecycle APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Failover</h3>
          <p>Failover orchestration contracts are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
