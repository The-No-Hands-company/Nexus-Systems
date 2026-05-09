export function registerWorkflowRoutes(app: ForgeRouteApp) {
  app.get("/api/issues", async () => ({
    issues: [],
    nextCursor: null,
    note: "Issue engine is stubbed.",
  }));
  app.post("/api/issues", async ({ body }) => {
    const payload =
      typeof body === "object" && body !== null ? (body as Record<string, unknown>) : {};

    return {
      ok: true,
      draft: true,
      issue: payload,
    };
  });

  app.get("/api/pull-requests", async () => ({
    pullRequests: [],
    nextCursor: null,
    note: "PR workflow is stubbed.",
  }));
  app.post("/api/pull-requests", async ({ body }) => {
    const payload =
      typeof body === "object" && body !== null ? (body as Record<string, unknown>) : {};

    return {
      ok: true,
      draft: true,
      pullRequest: payload,
    };
  });

  app.get("/api/ci/pipelines", async () => ({
    pipelines: [],
    note: "CI pipeline orchestration is stubbed.",
  }));
  app.post("/api/ci/pipelines/run", async ({ body }) => {
    const payload =
      typeof body === "object" && body !== null ? (body as Record<string, unknown>) : {};

    return {
      ok: true,
      runId: "stub-run-001",
      input: payload,
    };
  });

  app.get("/api/releases", async () => ({ releases: [], note: "Release management is stubbed." }));
  app.post("/api/releases", async ({ body }) => {
    const payload =
      typeof body === "object" && body !== null ? (body as Record<string, unknown>) : {};

    return {
      ok: true,
      release: { id: "stub-release", ...payload },
    };
  });

  app.get("/api/packages", async () => ({
    registries: ["npm", "container", "maven", "pypi"],
    packages: [],
  }));
  app.post("/api/packages/publish", async ({ body }) => ({
    ok: true,
    queued: true,
    payload: body || {},
  }));

  app.get("/api/webhooks", async () => ({
    webhooks: [],
    note: "Webhook subscriptions are stubbed.",
  }));
  app.post("/api/webhooks/test", async ({ body }) => {
    const payload =
      typeof body === "object" && body !== null ? (body as Record<string, unknown>) : {};

    return {
      ok: true,
      delivery: "stub-delivery",
      target: typeof payload.url === "string" ? payload.url : null,
    };
  });

  app.get("/api/search", async ({ query }) => ({
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
