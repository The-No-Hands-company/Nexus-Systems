import { entityResponse, listResponse } from "./contracts";

export function registerIssueManagementFullDepthRoutes(app: ForgeRouteApp) {
  app.get("/api/issue-management-full-depth/metadata", async () =>
    listResponse([], "Issue metadata parity: types, weights, severity, priority, health"),
  );

  app.get("/api/issue-management-full-depth/hierarchy", async () =>
    listResponse([], "Epics, child issues, and relationship graph parity"),
  );

  app.get("/api/issue-management-full-depth/iterations", async () =>
    listResponse([], "Iteration and sprint planning parity controls"),
  );

  app.post("/api/issue-management-full-depth/stub", async ({ body }) =>
    entityResponse(
      { id: "issue-depth-stub", ...(body || {}) },
      "Issue management depth parity stub request accepted",
    ),
  );
}
