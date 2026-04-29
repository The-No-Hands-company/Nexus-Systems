import { useEffect, useState } from "react";

export default function StrategicCapacityCommandHub() {
  const [drivers, setDrivers] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/strategic-capacity/drivers");
        const data = await res.json();
        setDrivers(data.entity?.drivers || []);
      } catch {
        setDrivers([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Strategic Capacity Command Hub</h2>
      <p>Capacity planning, allocation governance, and strategic rebalance workflows.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Drivers</h3>
          <p>{drivers.length > 0 ? drivers.join(", ") : "No capacity drivers loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Plans</h3>
          <p>Capacity plan APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Allocations</h3>
          <p>Allocation and rebalance workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
