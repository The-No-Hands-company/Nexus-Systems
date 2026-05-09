export function registerInteropRoutes(app: ForgeRouteApp) {
  app.get("/api/interop/providers", async () => ({
    providers: ["github", "gitlab", "gitea", "bitbucket", "azure-devops"],
    note: "Interop provider catalog is stubbed.",
  }));

  app.post("/api/interop/migrations", async ({ body }) => ({
    ok: true,
    migrationId: "migration-stub",
    source: body || {},
  }));
  app.get("/api/interop/migrations", async () => ({
    jobs: [],
    note: "Migration jobs are stubbed.",
  }));

  app.post("/api/interop/mirrors", async ({ body }) => ({
    ok: true,
    mirrorId: "mirror-stub",
    config: body || {},
  }));
  app.get("/api/interop/mirrors", async () => ({
    mirrors: [],
    note: "Mirror sync topology is stubbed.",
  }));

  app.get("/api/interop/tokens", async () => ({
    tokens: [],
    note: "Interop token management is stubbed.",
  }));
  app.post("/api/interop/tokens", async ({ body }) => ({
    ok: true,
    token: { id: "token-stub", ...(body || {}) },
  }));
}
