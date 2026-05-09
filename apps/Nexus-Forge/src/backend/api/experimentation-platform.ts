import { entityResponse, listResponse } from "./contracts";

export function registerExperimentationPlatformRoutes(app: ForgeRouteApp) {
  app.get("/api/experiments/catalog", async () =>
    listResponse([], "Experiment catalog is stubbed."),
  );
  app.post("/api/experiments/catalog", async ({ body }) =>
    entityResponse(
      { id: "experiment-stub", ...(body || {}) },
      "Experiment registration is stubbed.",
    ),
  );

  app.get("/api/experiments/variants", async () =>
    listResponse([], "Variant configuration is stubbed."),
  );
  app.post("/api/experiments/launch", async ({ body }) =>
    entityResponse(
      { id: "experiment-launch-stub", payload: body || {} },
      "Experiment launch orchestration is stubbed.",
    ),
  );

  app.get("/api/experiments/insights", async () =>
    listResponse([], "Experiment analytics insights are stubbed."),
  );
}
