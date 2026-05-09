export function registerKnowledgeRoutes(app: ForgeRouteApp) {
  app.get("/api/wiki/pages", async () => ({ pages: [], note: "Wiki engine is stubbed." }));
  app.post("/api/wiki/pages", async ({ body }) => ({
    ok: true,
    page: { id: "wiki-stub", ...(body || {}) },
  }));

  app.get("/api/docs/sites", async () => ({ sites: [], note: "Docs site hosting is stubbed." }));
  app.post("/api/docs/sites/build", async ({ body }) => ({
    ok: true,
    buildId: "docs-build-stub",
    input: body || {},
  }));

  app.get("/api/snippets", async () => ({
    snippets: [],
    note: "Code snippets library is stubbed.",
  }));
  app.post("/api/snippets", async ({ body }) => ({
    ok: true,
    snippet: { id: "snippet-stub", ...(body || {}) },
  }));

  app.get("/api/adr", async () => ({
    records: [],
    note: "Architecture decision record system is stubbed.",
  }));
  app.post("/api/adr", async ({ body }) => ({
    ok: true,
    record: { id: "adr-stub", ...(body || {}) },
  }));
}
