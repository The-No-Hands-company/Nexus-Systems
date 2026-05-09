import { entityResponse, listResponse } from "./contracts";

export function registerResilienceRoutes(app: ForgeRouteApp) {
  app.get("/api/resilience/dr-plans", async () =>
    listResponse([], "Disaster recovery plans are stubbed."),
  );
  app.post("/api/resilience/dr-plans", async ({ body }) =>
    entityResponse({ id: "dr-plan-stub", ...(body || {}) }, "DR plan creation is stubbed."),
  );

  app.get("/api/resilience/chaos-experiments", async () =>
    listResponse([], "Chaos engineering registry is stubbed."),
  );
  app.post("/api/resilience/chaos-experiments", async ({ body }) =>
    entityResponse(
      { id: "chaos-stub", ...(body || {}) },
      "Chaos experiment scheduling is stubbed.",
    ),
  );

  app.get("/api/resilience/failover", async () =>
    entityResponse(
      { mode: "manual", regions: [], lastDrillAt: null },
      "Failover orchestration is stubbed.",
    ),
  );

  app.get("/api/resilience/drills", async () =>
    listResponse([], "Recovery drill history is stubbed."),
  );
}
