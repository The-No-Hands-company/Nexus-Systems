import { entityResponse, listResponse } from "./contracts";

export function registerStrategicCapacityCommandRoutes(app: ForgeRouteApp) {
  app.get("/api/strategic-capacity/plans", async () =>
    listResponse([], "Strategic capacity plans are stubbed."),
  );
  app.post("/api/strategic-capacity/plans", async ({ body }) =>
    entityResponse(
      { id: "capacity-plan-stub", ...(body || {}) },
      "Capacity plan creation is stubbed.",
    ),
  );

  app.get("/api/strategic-capacity/allocations", async () =>
    listResponse([], "Capacity allocation ledger is stubbed."),
  );
  app.post("/api/strategic-capacity/rebalances", async ({ body }) =>
    entityResponse(
      { id: "capacity-rebalance-stub", payload: body || {} },
      "Capacity rebalance orchestration is stubbed.",
    ),
  );

  app.get("/api/strategic-capacity/drivers", async () =>
    entityResponse(
      {
        drivers: ["demand", "talent", "budget", "technical-debt"],
      },
      "Strategic capacity drivers are stubbed.",
    ),
  );
}
