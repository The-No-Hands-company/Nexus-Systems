import { entityResponse, listResponse } from "./contracts";

export function registerTrustAnalyticsRoutes(app: ForgeRouteApp) {
  app.get("/api/trust-analytics/scorecards", async () =>
    listResponse([], "Trust scorecards are stubbed."),
  );
  app.get("/api/trust-analytics/signals", async () =>
    listResponse([], "Trust signal telemetry is stubbed."),
  );

  app.post("/api/trust-analytics/benchmarks", async ({ body }) =>
    entityResponse(
      { id: "trust-benchmark-stub", ...(body || {}) },
      "Trust benchmark creation is stubbed.",
    ),
  );

  app.get("/api/trust-analytics/controls", async () =>
    entityResponse(
      {
        categories: ["security", "privacy", "availability", "compliance"],
      },
      "Trust control categories are stubbed.",
    ),
  );
}
