import { entityResponse, listResponse } from "./contracts";

export function registerOrgDesignStudioRoutes(app: ForgeRouteApp) {
  app.get("/api/org-design/topologies", async () =>
    listResponse([], "Organization topology catalog is stubbed."),
  );
  app.post("/api/org-design/topologies", async ({ body }) =>
    entityResponse(
      { id: "org-topology-stub", ...(body || {}) },
      "Organization topology authoring is stubbed.",
    ),
  );

  app.get("/api/org-design/skills", async () => listResponse([], "Org skill matrix is stubbed."));
  app.post("/api/org-design/restructures", async ({ body }) =>
    entityResponse(
      { id: "org-restructure-stub", payload: body || {} },
      "Restructure simulation workflow is stubbed.",
    ),
  );

  app.get("/api/org-design/capacity", async () =>
    listResponse([], "Capacity planning view is stubbed."),
  );
}
