import { entityResponse, listResponse } from "./contracts";

export function registerCustomerJourneyOrchestrationRoutes(app: any) {
  app.get("/api/journeys/maps", async () => listResponse([], "Customer journey maps are stubbed."));
  app.post("/api/journeys/maps", async ({ body }: any) =>
    entityResponse({ id: "journey-map-stub", ...(body || {}) }, "Journey map authoring is stubbed.")
  );

  app.get("/api/journeys/touches", async () => listResponse([], "Touchpoint catalog is stubbed."));
  app.post("/api/journeys/automations", async ({ body }: any) =>
    entityResponse({ id: "journey-automation-stub", ...(body || {}) }, "Journey automation is stubbed.")
  );

  app.get("/api/journeys/health", async () => listResponse([], "Journey health scoring is stubbed."));
}
