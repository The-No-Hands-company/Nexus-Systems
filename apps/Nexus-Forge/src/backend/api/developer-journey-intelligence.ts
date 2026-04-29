import { entityResponse, listResponse } from "./contracts";

export function registerDeveloperJourneyIntelligenceRoutes(app: any) {
  app.get("/api/developer-journey/maps", async () => listResponse([], "Developer journey maps are stubbed."));
  app.post("/api/developer-journey/maps", async ({ body }: any) =>
    entityResponse({ id: "developer-journey-map-stub", ...(body || {}) }, "Developer journey map authoring is stubbed.")
  );

  app.get("/api/developer-journey/frictions", async () => listResponse([], "Developer friction telemetry is stubbed."));
  app.post("/api/developer-journey/interventions", async ({ body }: any) =>
    entityResponse({ id: "developer-journey-intervention-stub", payload: body || {} }, "Journey intervention workflows are stubbed.")
  );

  app.get("/api/developer-journey/insights", async () => listResponse([], "Developer journey insights are stubbed."));
}
