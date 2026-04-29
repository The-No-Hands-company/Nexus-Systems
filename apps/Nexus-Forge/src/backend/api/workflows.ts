export function registerWorkflowRoutes(app: any) {
  app.get("/api/issues", async () => ({ issues: [], nextCursor: null, note: "Issue engine is stubbed." }));
  app.post("/api/issues", async ({ body }: any) => ({ ok: true, draft: true, issue: body || {} }));

  app.get("/api/pull-requests", async () => ({ pullRequests: [], nextCursor: null, note: "PR workflow is stubbed." }));
  app.post("/api/pull-requests", async ({ body }: any) => ({ ok: true, draft: true, pullRequest: body || {} }));

  app.get("/api/ci/pipelines", async () => ({ pipelines: [], note: "CI pipeline orchestration is stubbed." }));
  app.post("/api/ci/pipelines/run", async ({ body }: any) => ({ ok: true, runId: "stub-run-001", input: body || {} }));

  app.get("/api/releases", async () => ({ releases: [], note: "Release management is stubbed." }));
  app.post("/api/releases", async ({ body }: any) => ({ ok: true, release: { id: "stub-release", ...(body || {}) } }));

  app.get("/api/packages", async () => ({ registries: ["npm", "container", "maven", "pypi"], packages: [] }));
  app.post("/api/packages/publish", async ({ body }: any) => ({ ok: true, queued: true, payload: body || {} }));

  app.get("/api/webhooks", async () => ({ webhooks: [], note: "Webhook subscriptions are stubbed." }));
  app.post("/api/webhooks/test", async ({ body }: any) => ({ ok: true, delivery: "stub-delivery", target: body?.url || null }));

  app.get("/api/search", async ({ query }: any) => ({
    q: query?.q || "",
    results: { repos: [], code: [], issues: [], pullRequests: [] },
    note: "Unified search is stubbed.",
  }));

  app.get("/api/audit/events", async () => ({ events: [], note: "Audit trail is stubbed." }));
  app.get("/api/admin/health/modules", async () => ({
    modules: [
      { name: "repositories", healthy: true, mode: "stubbed" },
      { name: "issues", healthy: true, mode: "stubbed" },
      { name: "ci", healthy: true, mode: "stubbed" },
      { name: "federation", healthy: true, mode: "stubbed" },
    ],
  }));
}
