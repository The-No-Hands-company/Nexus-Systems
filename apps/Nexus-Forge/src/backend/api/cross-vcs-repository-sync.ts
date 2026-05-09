import { entityResponse, listResponse } from "./contracts";

export function registerCrossVcsRepositorySyncRoutes(app: ForgeRouteApp) {
  app.get("/api/vcs-sync/mirrors", async () =>
    listResponse([], "Repository mirrors across VCS backends"),
  );
  app.post("/api/vcs-sync/create-mirror", async ({ body }) =>
    entityResponse({ id: "mirror-stub", ...(body || {}) }, "Create cross-VCS mirror"),
  );
  app.get("/api/vcs-sync/status", async () => listResponse([], "Mirror synchronization status"));
  app.post("/api/vcs-sync/sync", async ({ body }) =>
    entityResponse({ id: "sync-stub", payload: body || {} }, "Trigger VCS sync"),
  );
  app.get("/api/vcs-sync/health", async () =>
    entityResponse({ backends: 0, synced: false }, "Cross-VCS health"),
  );
}
