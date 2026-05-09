import { entityResponse, listResponse } from "./contracts";

export function registerPolicySimulationRoutes(app: ForgeRouteApp) {
  app.get("/api/policy/simulations", async () =>
    listResponse([], "Policy simulation queue is stubbed."),
  );

  app.post("/api/policy/simulations", async ({ body }) =>
    entityResponse(
      {
        id: "simulation-stub",
        status: "queued",
        ...(body || {}),
      },
      "Policy simulation dispatch is stubbed.",
    ),
  );

  app.get("/api/policy/simulations/:id", async ({ params }) =>
    entityResponse(
      {
        id: params.id,
        status: "completed",
        impacts: [],
      },
      "Policy simulation result retrieval is stubbed.",
    ),
  );

  app.get("/api/policy/what-if", async () =>
    entityResponse(
      { scenarios: [], note: "What-if policy engine is stubbed." },
      "Policy what-if analysis is stubbed.",
    ),
  );
}
