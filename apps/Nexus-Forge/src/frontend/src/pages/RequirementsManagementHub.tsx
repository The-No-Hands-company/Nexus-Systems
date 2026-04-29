import { useEffect, useState } from "react";

export default function RequirementsManagementHub() {
  const [report, setReport] = useState<{ complianceMatrix?: string; csvImport?: string }>({});

  useEffect(() => {
    async function load() {
      try {
        const res = await fetch("/api/requirements-management/reports");
        const data = await res.json();
        setReport(data.entity || {});
      } catch {
        setReport({});
      }
    }
    load();
  }, []);

  return (
    <section>
      <h2>Requirements Management Hub</h2>
      <p>Requirements lifecycle and compliance traceability parity.</p>
      <div className="repo-grid">
        <article className="repo-card"><h3>Compliance Matrix</h3><p>{report.complianceMatrix || "stubbed"}</p></article>
        <article className="repo-card"><h3>CSV Import</h3><p>{report.csvImport || "stubbed"}</p></article>
        <article className="repo-card"><h3>Status</h3><p>Requirements API stubs active</p></article>
      </div>
    </section>
  );
}