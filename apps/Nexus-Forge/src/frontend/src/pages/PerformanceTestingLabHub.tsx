import { useEffect, useState } from "react";

export default function PerformanceTestingLabHub() {
  const [regressions, setRegressions] = useState<{ regressions: number; baseline: string }>({
    regressions: 0,
    baseline: "",
  });

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/perf-testing/regressions");
        const data = await res.json();
        setRegressions(data.entity || { regressions: 0, baseline: "" });
      } catch {
        setRegressions({ regressions: 0, baseline: "" });
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Performance Testing Lab Hub</h2>
      <p>Performance benchmarks, test execution, and regression analysis for releases.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Benchmarks</h3>
          <p>Performance benchmarks are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Results</h3>
          <p>Test results are stubbed</p>
        </article>
        <article className="repo-card">
          <h3>Analysis</h3>
          <p>
            Baseline: {regressions.baseline}, Regressions: {regressions.regressions}
          </p>
        </article>
      </div>
    </section>
  );
}
