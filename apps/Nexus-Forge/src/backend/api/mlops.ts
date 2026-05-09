import { entityResponse, listResponse } from "./contracts";

export function registerMLOpsRoutes(app: ForgeRouteApp) {
  app.get("/api/ml/models", async () => listResponse([], "Model registry is stubbed."));
  app.post("/api/ml/models", async ({ body }) =>
    entityResponse({ id: "model-stub", ...(body || {}) }, "Model registration is stubbed."),
  );

  app.get("/api/ml/experiments", async () =>
    listResponse([], "ML experiment tracking is stubbed."),
  );
  app.post("/api/ml/experiments", async ({ body }) =>
    entityResponse({ id: "experiment-stub", ...(body || {}) }, "Experiment creation is stubbed."),
  );

  app.get("/api/ml/datasets", async () => listResponse([], "Dataset catalog is stubbed."));
  app.get("/api/ml/serving", async () =>
    entityResponse(
      { endpoints: [], autoscaling: false },
      "Model serving orchestration is stubbed.",
    ),
  );
  app.get("/api/ml/monitoring", async () => listResponse([], "Model drift monitoring is stubbed."));
}
