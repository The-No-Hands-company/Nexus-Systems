import { featureCatalog, getFeatureSummary } from "../features/catalog";

export function registerPlatformRoutes(app: any) {
  app.get("/api/features", async ({ query }: any) => {
    const category = query?.category as string | undefined;
    const state = query?.state as string | undefined;

    const filtered = featureCatalog.filter((item) => {
      const categoryOk = category ? item.category.toLowerCase() === category.toLowerCase() : true;
      const stateOk = state ? item.state === state : true;
      return categoryOk && stateOk;
    });

    return {
      features: filtered,
      summary: getFeatureSummary(),
      filters: { category: category || null, state: state || null },
    };
  });

  app.get("/api/features/:id", async ({ params }: any) => {
    const found = featureCatalog.find((item) => item.id === params.id);
    if (!found) return { error: "Feature not found", status: 404 };
    return { feature: found };
  });

  app.get("/api/roadmap/highlights", async () => {
    return {
      now: [
        "Core repository management",
        "Federation contract endpoints",
        "AI integration contract",
      ],
      next: [
        "Issue and project workflows",
        "CI/CD pipelines",
        "Security scans",
      ],
      later: [
        "Federated replication",
        "Observability and policy-as-code",
        "Enterprise governance",
      ],
    };
  });

  app.get("/api/status/modules", async () => {
    return {
      modules: [
        { name: "Repository", state: "stubbed" },
        { name: "Teams and Permissions", state: "stubbed" },
        { name: "Code Review", state: "stubbed" },
        { name: "Issues and Projects", state: "planned" },
        { name: "CI/CD", state: "planned" },
        { name: "Federation", state: "stubbed" },
        { name: "AI", state: "stubbed" },
      ],
    };
  });
}
