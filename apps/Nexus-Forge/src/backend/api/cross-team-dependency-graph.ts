import { entityResponse, listResponse } from "./contracts";

export function registerCrossTeamDependencyGraphRoutes(app: ForgeRouteApp) {
  app.get("/api/team-dependencies/graph", async () =>
    listResponse([], "Cross-team dependency graph"),
  );
  app.post("/api/team-dependencies/map", async ({ body }) =>
    entityResponse({ id: "map-stub", ...(body || {}) }, "Map team dependencies"),
  );
  app.get("/api/team-dependencies/blockers", async () =>
    listResponse([], "Blocking dependencies and risks"),
  );
  app.post("/api/team-dependencies/resolve", async ({ body }) =>
    entityResponse({ id: "resolve-stub", payload: body || {} }, "Resolve dependency"),
  );
  app.get("/api/team-dependencies/health", async () =>
    entityResponse({ status: "scanning", criticalDeps: 0 }, "Dependency health status"),
  );
}
