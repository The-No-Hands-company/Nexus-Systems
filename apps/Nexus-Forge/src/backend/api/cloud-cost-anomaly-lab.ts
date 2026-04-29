import { entityResponse, listResponse } from "./contracts";

export function registerCloudCostAnomalyLabRoutes(app: any) {
  app.get("/api/cloud-cost/anomalies", async () => listResponse([], "Cloud cost anomaly feed is stubbed."));
  app.get("/api/cloud-cost/baselines", async () => listResponse([], "Cloud cost baseline models are stubbed."));

  app.post("/api/cloud-cost/alerts", async ({ body }: any) =>
    entityResponse({ id: "cloud-cost-alert-stub", ...(body || {}) }, "Cloud cost alert workflow is stubbed.")
  );

  app.get("/api/cloud-cost/dimensions", async () =>
    entityResponse(
      {
        dimensions: ["service", "region", "team", "environment"],
      },
      "Cloud cost dimensions are stubbed."
    )
  );
}
