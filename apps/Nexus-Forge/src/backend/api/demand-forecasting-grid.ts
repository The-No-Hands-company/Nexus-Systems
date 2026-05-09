import { entityResponse, listResponse } from "./contracts";

export function registerDemandForecastingGridRoutes(app: ForgeRouteApp) {
  app.get("/api/demand-forecasting/models", async () =>
    listResponse([], "Demand forecasting models are stubbed."),
  );
  app.post("/api/demand-forecasting/models", async ({ body }) =>
    entityResponse(
      { id: "forecast-model-stub", ...(body || {}) },
      "Forecast model creation is stubbed.",
    ),
  );

  app.get("/api/demand-forecasting/scenarios", async () =>
    listResponse([], "Demand forecast scenarios are stubbed."),
  );
  app.post("/api/demand-forecasting/runs", async ({ body }) =>
    entityResponse(
      { id: "forecast-run-stub", payload: body || {} },
      "Forecast simulation runs are stubbed.",
    ),
  );

  app.get("/api/demand-forecasting/signals", async () =>
    entityResponse(
      {
        signals: ["pipeline", "seasonality", "capacity", "market-volatility"],
      },
      "Demand signal dimensions are stubbed.",
    ),
  );
}
