import { entityResponse, listResponse } from "./contracts";

export function registerTenancyRoutes(app: ForgeRouteApp) {
  app.get("/api/tenancy/models", async () =>
    entityResponse(
      {
        models: ["single-tenant", "pooled", "hybrid"],
      },
      "Tenancy model catalog is stubbed.",
    ),
  );

  app.get("/api/tenancy/organizations", async () =>
    listResponse([], "Organization registry is stubbed."),
  );
  app.post("/api/tenancy/organizations", async ({ body }) =>
    entityResponse(
      { id: "tenant-org-stub", ...(body || {}) },
      "Organization onboarding is stubbed.",
    ),
  );

  app.get("/api/tenancy/environments", async () =>
    listResponse([], "Tenant environment topology is stubbed."),
  );
  app.post("/api/tenancy/provision", async ({ body }) =>
    entityResponse(
      { requestId: "tenant-provision-stub", payload: body || {} },
      "Tenant provisioning is stubbed.",
    ),
  );
}
