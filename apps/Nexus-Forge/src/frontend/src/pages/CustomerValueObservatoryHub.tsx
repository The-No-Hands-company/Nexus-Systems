import { useEffect, useState } from "react";

export default function CustomerValueObservatoryHub() {
  const [dimensions, setDimensions] = useState<string[]>([]);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/customer-value/dimensions");
        const data = await res.json();
        setDimensions(data.entity?.dimensions || []);
      } catch {
        setDimensions([]);
      }
    }

    load();
  }, []);

  return (
    <section>
      <h2>Customer Value Observatory Hub</h2>
      <p>Outcome intelligence, value hypotheses, and account value dimension tracking.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Dimensions</h3>
          <p>{dimensions.length > 0 ? dimensions.join(", ") : "No value dimensions loaded"}</p>
        </article>
        <article className="repo-card">
          <h3>Accounts</h3>
          <p>Account value APIs are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Hypotheses</h3>
          <p>Value hypothesis workflows are stubbed.</p>
        </article>
      </div>
    </section>
  );
}
