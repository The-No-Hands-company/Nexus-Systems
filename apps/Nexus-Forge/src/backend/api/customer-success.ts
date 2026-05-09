import { entityResponse, listResponse } from "./contracts";

export function registerCustomerSuccessRoutes(app: ForgeRouteApp) {
  app.get("/api/customer-success/accounts", async () =>
    listResponse([], "Customer account portfolio is stubbed."),
  );

  app.get("/api/customer-success/health", async () =>
    entityResponse(
      {
        dimensions: ["adoption", "reliability", "support", "engagement"],
      },
      "Customer health model is stubbed.",
    ),
  );

  app.post("/api/customer-success/playbooks", async ({ body }) =>
    entityResponse(
      { id: "playbook-stub", ...(body || {}) },
      "Success playbook lifecycle is stubbed.",
    ),
  );

  app.get("/api/customer-success/renewals", async () =>
    listResponse([], "Renewal planning is stubbed."),
  );
}
