import { useEffect, useState } from "react";

export default function NotebookHub() {
  const [notebooks, setNotebooks] = useState(0);

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/notebooks");
        const data = await res.json();
        setNotebooks((data.items || []).length);
      } catch {
        setNotebooks(0);
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Notebook Hub</h2>
      <p>Interactive notebook workspaces, kernels, and execution orchestration.</p>
      <div className="repo-grid">
        <article className="repo-card">
          <h3>Notebooks</h3>
          <p>Saved notebooks: {notebooks}</p>
        </article>
        <article className="repo-card">
          <h3>Kernels</h3>
          <p>Kernel and runtime registry contracts are stubbed.</p>
        </article>
        <article className="repo-card">
          <h3>Execution History</h3>
          <p>Execution run tracking is stubbed.</p>
        </article>
      </div>
    </section>
  );
}
