import { useEffect, useState } from "react";

interface Summary {
  total: number;
  implemented: number;
  stubbed: number;
  notYet: number;
  sections: number;
}

interface FeatureItem {
  id: string;
  slug: string;
  name: string;
  status: "✅" | "🟡" | "❌";
  section: string;
  notes: string;
}

export default function InventoryStubEngineHub() {
  const [summary, setSummary] = useState<Summary>({
    total: 0,
    implemented: 0,
    stubbed: 0,
    notYet: 0,
    sections: 0,
  });
  const [features, setFeatures] = useState<FeatureItem[]>([]);
  const [statusFilter, setStatusFilter] = useState<"all" | "✅" | "🟡" | "❌">("❌");
  const [loading, setLoading] = useState(false);

  useEffect(() => {
    async function loadSummary() {
      try {
        const res = await fetch("/api/inventory-stub-engine/summary");
        const data = await res.json();
        if (data?.entity) {
          setSummary(data.entity);
        }
      } catch {
        setSummary({ total: 0, implemented: 0, stubbed: 0, notYet: 0, sections: 0 });
      }
    }

    loadSummary();
  }, []);

  useEffect(() => {
    async function loadFeatures() {
      setLoading(true);
      try {
        const query = statusFilter === "all" ? "" : `?status=${encodeURIComponent(statusFilter)}`;
        const res = await fetch(`/api/inventory-stub-engine/features${query}`);
        const data = await res.json();
        setFeatures(Array.isArray(data?.items) ? data.items : []);
      } catch {
        setFeatures([]);
      } finally {
        setLoading(false);
      }
    }

    loadFeatures();
  }, [statusFilter]);

  return (
    <section>
      <h2>Inventory Stub Engine Hub</h2>
      <p>Autonomous stubbing foundation for all tracked feature rows from FEATURE_INVENTORY.md.</p>

      <div className="repo-grid">
        <article className="repo-card">
          <h3>Total Features</h3>
          <p>{summary.total}</p>
        </article>
        <article className="repo-card">
          <h3>Implemented</h3>
          <p>{summary.implemented}</p>
        </article>
        <article className="repo-card">
          <h3>Stubbed</h3>
          <p>{summary.stubbed}</p>
        </article>
        <article className="repo-card">
          <h3>Not Yet</h3>
          <p>{summary.notYet}</p>
        </article>
      </div>

      <div className="repo-card" style={{ marginTop: 12 }}>
        <h3>Filter</h3>
        <label htmlFor="status-filter">Status </label>
        <select
          id="status-filter"
          value={statusFilter}
          onChange={(e) => setStatusFilter(e.target.value as "all" | "✅" | "🟡" | "❌")}
        >
          <option value="all">All</option>
          <option value="❌">Not Yet</option>
          <option value="🟡">Stub</option>
          <option value="✅">Implemented</option>
        </select>
        <p>Sections tracked: {summary.sections}</p>
      </div>

      <div className="repo-grid" style={{ marginTop: 12 }}>
        {loading ? (
          <article className="repo-card">
            <h3>Loading</h3>
            <p>Fetching features from inventory stub engine...</p>
          </article>
        ) : (
          features.slice(0, 60).map((feature) => (
            <article className="repo-card" key={feature.id}>
              <h3>
                {feature.status} {feature.name}
              </h3>
              <p>{feature.section}</p>
              <p>{feature.notes || "No notes"}</p>
            </article>
          ))
        )}
      </div>
    </section>
  );
}
