export function registerMarketplaceRoutes(app: ForgeRouteApp) {
  app.get("/api/marketplace/apps", async () => ({
    apps: [],
    note: "Marketplace app index is stubbed.",
  }));
  app.get("/api/marketplace/themes", async () => ({
    themes: [],
    note: "Theme marketplace is stubbed.",
  }));
  app.get("/api/marketplace/extensions", async () => ({
    extensions: [],
    note: "Extension marketplace is stubbed.",
  }));

  app.post("/api/marketplace/install", async ({ body }) => ({
    ok: true,
    installId: "install-stub",
    package: body || {},
  }));
  app.post("/api/marketplace/uninstall", async ({ body }) => ({
    ok: true,
    removed: true,
    package: body || {},
  }));

  app.get("/api/marketplace/submissions", async () => ({
    submissions: [],
    note: "Publisher submissions are stubbed.",
  }));
  app.post("/api/marketplace/submissions", async ({ body }) => ({
    ok: true,
    submission: { id: "submission-stub", ...(body || {}) },
  }));
}
