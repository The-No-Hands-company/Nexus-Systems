import { entityResponse, listResponse } from "./contracts";

export function registerPlatformEconomicsRoutes(app: ForgeRouteApp) {
  app.get("/api/economics/models", async () =>
    listResponse([], "Economic model library is stubbed."),
  );
  app.post("/api/economics/models", async ({ body }) =>
    entityResponse(
      { id: "economic-model-stub", ...(body || {}) },
      "Economic model authoring is stubbed.",
    ),
  );

  app.get("/api/economics/scenarios", async () =>
    listResponse([], "Economic scenario simulation is stubbed."),
  );
  app.post("/api/economics/forecast", async ({ body }) =>
    entityResponse(
      { id: "economic-forecast-stub", payload: body || {} },
      "Forecast generation is stubbed.",
    ),
  );

  app.get("/api/economics/drivers", async () =>
    entityResponse(
      {
        drivers: ["demand", "cost", "capacity", "risk"],
      },
      "Economic driver taxonomy is stubbed.",
    ),
  );
}
