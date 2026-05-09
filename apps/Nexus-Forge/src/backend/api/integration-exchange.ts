import { entityResponse, listResponse } from "./contracts";

export function registerIntegrationExchangeRoutes(app: ForgeRouteApp) {
  app.get("/api/integrations/catalog", async () =>
    listResponse([], "Integration catalog is stubbed."),
  );
  app.post("/api/integrations/catalog", async ({ body }) =>
    entityResponse(
      { id: "integration-entry-stub", ...(body || {}) },
      "Integration publishing is stubbed.",
    ),
  );

  app.get("/api/integrations/connectors", async () =>
    listResponse([], "Connector registry is stubbed."),
  );
  app.post("/api/integrations/connectors", async ({ body }) =>
    entityResponse({ id: "connector-stub", ...(body || {}) }, "Connector lifecycle is stubbed."),
  );

  app.get("/api/integrations/health", async () =>
    entityResponse(
      {
        states: ["healthy", "degraded", "offline"],
      },
      "Integration health model is stubbed.",
    ),
  );
}
