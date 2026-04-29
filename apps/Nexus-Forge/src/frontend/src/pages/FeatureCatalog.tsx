import { useEffect, useMemo, useState } from "react";

type FeatureState = "planned" | "stubbed" | "in-progress" | "ready";

interface FeatureRow {
  id: string;
  name: string;
  category: string;
  state: FeatureState;
  description: string;
}

interface FeaturePayload {
  features: FeatureRow[];
  summary: {
    total: number;
    planned: number;
    stubbed: number;
    inProgress: number;
    ready: number;
  };
}

const fallbackData: FeaturePayload = {
  features: [
    { id: "repo.create", name: "Repository Creation", category: "Core Repository", state: "stubbed", description: "Create repositories with VCS selection." },
    { id: "pr.review", name: "Review Workflows", category: "Code Review", state: "stubbed", description: "Approve/request changes workflows." },
    { id: "actions.pipelines", name: "CI Pipelines", category: "CI/CD", state: "stubbed", description: "Configure jobs on push and pull requests." },
  ],
  summary: {
    total: 3,
    planned: 0,
    stubbed: 3,
    inProgress: 0,
    ready: 0,
  },
};

export default function FeatureCatalog() {
  const [payload, setPayload] = useState<FeaturePayload>(fallbackData);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    async function loadFeatures() {
      try {
        const res = await fetch("/api/features");
        if (!res.ok) throw new Error("Failed to load feature catalog");
        const data = (await res.json()) as FeaturePayload;
        setPayload(data);
      } catch {
        setError("Using fallback catalog while backend is unavailable.");
      } finally {
        setLoading(false);
      }
    }

    loadFeatures();
  }, []);

  const grouped = useMemo(() => {
    const map = new Map<string, FeatureRow[]>();
    for (const item of payload.features) {
      const list = map.get(item.category) || [];
      list.push(item);
      map.set(item.category, list);
    }
    return [...map.entries()].sort(([a], [b]) => a.localeCompare(b));
  }, [payload.features]);

  return (
    <section>
      <h2>Nexus Forge Feature Universe</h2>
      <p>Roadmap-level snapshot of platform capabilities and implementation state.</p>
      {loading ? <p>Loading feature catalog…</p> : null}
      {error ? <p>{error}</p> : null}

      <div className="repo-grid" style={{ marginTop: "14px" }}>
        <article className="repo-card">
          <h3>Total Features</h3>
          <p>{payload.summary.total}</p>
        </article>
        <article className="repo-card">
          <h3>Stubbed</h3>
          <p>{payload.summary.stubbed}</p>
        </article>
        <article className="repo-card">
          <h3>Planned</h3>
          <p>{payload.summary.planned}</p>
        </article>
      </div>

      {grouped.map(([category, items]) => (
        <div key={category} style={{ marginTop: "20px" }}>
          <h3>{category}</h3>
          <div className="repo-grid">
            {items.map((item) => (
              <article key={item.id} className="repo-card">
                <h4>{item.name}</h4>
                <p>{item.description}</p>
                <span className="badge">{item.state}</span>
              </article>
            ))}
          </div>
        </div>
      ))}
    </section>
  );
}
