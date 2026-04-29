import { entityResponse, listResponse } from "./contracts";

export function registerDigitalTwinRoutes(app: any) {
  app.get("/api/digital-twin/environments", async () => listResponse([], "Digital twin environment catalog is stubbed."));
  app.post("/api/digital-twin/environments", async ({ body }: any) =>
    entityResponse({ id: "digital-twin-env-stub", ...(body || {}) }, "Digital twin environment provisioning is stubbed.")
  );

  app.get("/api/digital-twin/scenarios", async () => listResponse([], "Scenario simulation queue is stubbed."));
  app.post("/api/digital-twin/simulate", async ({ body }: any) =>
    entityResponse({ id: "digital-twin-run-stub", payload: body || {} }, "Simulation execution is stubbed.")
  );

  app.get("/api/digital-twin/insights", async () => listResponse([], "Simulation insight feed is stubbed."));
}
