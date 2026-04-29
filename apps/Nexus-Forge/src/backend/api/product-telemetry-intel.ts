import { entityResponse, listResponse } from "./contracts";

export function registerProductTelemetryIntelRoutes(app: any) {
  app.get("/api/product-telemetry/events", async () => listResponse([], "Product telemetry event stream is stubbed."));
  app.get("/api/product-telemetry/funnels", async () => listResponse([], "Funnel analytics are stubbed."));

  app.post("/api/product-telemetry/alerts", async ({ body }: any) =>
    entityResponse({ id: "telemetry-alert-stub", ...(body || {}) }, "Telemetry alerting is stubbed.")
  );

  app.get("/api/product-telemetry/signals", async () =>
    entityResponse(
      {
        signals: ["activation", "dropoff", "retention", "feature-adoption"],
      },
      "Product telemetry signal taxonomy is stubbed."
    )
  );
}
