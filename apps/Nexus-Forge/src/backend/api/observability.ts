import { entityResponse, listResponse } from "./contracts";

export function registerObservabilityRoutes(app: any) {
  app.get("/api/observability/overview", async () =>
    entityResponse(
      {
        metrics: ["latency", "throughput", "error-rate", "saturation"],
      },
      "Observability overview is stubbed."
    )
  );

  app.get("/api/observability/traces", async () => listResponse([], "Distributed trace search is stubbed."));
  app.get("/api/observability/logs", async () => listResponse([], "Log query index is stubbed."));

  app.post("/api/observability/alerts", async ({ body }: any) =>
    entityResponse({ id: "alert-policy-stub", ...(body || {}) }, "Alert policy management is stubbed.")
  );

  app.get("/api/observability/slo", async () => listResponse([], "SLO management is stubbed."));
}
