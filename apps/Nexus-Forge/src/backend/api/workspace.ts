export function registerWorkspaceRoutes(app: any) {
  app.get("/api/workspace/dashboard", async () => ({ widgets: [], note: "Custom dashboard is stubbed." }));
  app.post("/api/workspace/dashboard", async ({ body }: any) => ({ ok: true, layout: body || {} }));

  app.get("/api/workspace/preferences", async () => ({
    preferences: {
      theme: "nexus-night",
      defaultHome: "/platform",
      compactMode: false,
    },
    note: "Workspace preferences are stubbed.",
  }));

  app.post("/api/workspace/preferences", async ({ body }: any) => ({ ok: true, preferences: body || {} }));

  app.get("/api/workspace/saved-searches", async () => ({ searches: [], note: "Saved searches are stubbed." }));
  app.post("/api/workspace/saved-searches", async ({ body }: any) => ({ ok: true, search: { id: "search-stub", ...(body || {}) } }));

  app.get("/api/workspace/quick-actions", async () => ({
    actions: ["create_repo", "open_pr", "review_queue", "new_issue"],
    note: "Quick actions are stubbed.",
  }));
}
