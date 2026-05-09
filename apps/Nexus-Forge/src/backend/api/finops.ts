import { entityResponse, listResponse } from "./contracts";

export function registerFinOpsRoutes(app: ForgeRouteApp) {
  app.get("/api/finops/cost-centers", async () =>
    listResponse([], "Cost center registry is stubbed."),
  );
  app.post("/api/finops/cost-centers", async ({ body }) =>
    entityResponse(
      { id: "cost-center-stub", ...(body || {}) },
      "Cost center provisioning is stubbed.",
    ),
  );

  app.get("/api/finops/budgets", async () => listResponse([], "Budget planning is stubbed."));
  app.post("/api/finops/budgets", async ({ body }) =>
    entityResponse({ id: "budget-stub", ...(body || {}) }, "Budget creation is stubbed."),
  );

  app.get("/api/finops/anomalies", async () =>
    listResponse([], "Cost anomaly detection is stubbed."),
  );
  app.get("/api/finops/chargeback", async () =>
    entityResponse({ reports: [], period: "monthly" }, "Chargeback reporting is stubbed."),
  );
}
