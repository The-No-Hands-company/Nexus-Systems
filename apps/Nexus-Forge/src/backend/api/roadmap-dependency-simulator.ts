import { entityResponse, listResponse } from "./contracts";

export function registerRoadmapDependencySimulatorRoutes(app: ForgeRouteApp) {
  app.get("/api/roadmap-simulator/roadmaps", async () =>
    listResponse([], "Roadmap inventory is stubbed."),
  );
  app.get("/api/roadmap-simulator/dependencies", async () =>
    listResponse([], "Roadmap dependency graph is stubbed."),
  );

  app.post("/api/roadmap-simulator/runs", async ({ body }) =>
    entityResponse(
      { id: "roadmap-sim-run-stub", payload: body || {} },
      "Roadmap simulation run is stubbed.",
    ),
  );

  app.get("/api/roadmap-simulator/outcomes", async () =>
    listResponse([], "Roadmap simulation outcomes are stubbed."),
  );
}
