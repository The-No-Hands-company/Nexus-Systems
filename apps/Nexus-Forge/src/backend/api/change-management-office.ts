import { entityResponse, listResponse } from "./contracts";

export function registerChangeManagementOfficeRoutes(app: ForgeRouteApp) {
  app.get("/api/change-office/initiatives", async () =>
    listResponse([], "Change initiative portfolio is stubbed."),
  );
  app.post("/api/change-office/initiatives", async ({ body }) =>
    entityResponse(
      { id: "change-initiative-stub", ...(body || {}) },
      "Change initiative creation is stubbed.",
    ),
  );

  app.get("/api/change-office/comms", async () =>
    listResponse([], "Change communication plan registry is stubbed."),
  );
  app.get("/api/change-office/readiness", async () =>
    listResponse([], "Readiness assessment workflows are stubbed."),
  );

  app.post("/api/change-office/checkpoints", async ({ body }) =>
    entityResponse(
      { id: "change-checkpoint-stub", ...(body || {}) },
      "Adoption checkpoint management is stubbed.",
    ),
  );
}
