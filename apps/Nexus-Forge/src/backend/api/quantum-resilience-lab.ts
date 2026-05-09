import { entityResponse, listResponse } from "./contracts";

export function registerQuantumResilienceLabRoutes(app: ForgeRouteApp) {
  app.get("/api/quantum-resilience/scenarios", async () =>
    listResponse([], "Quantum resilience scenarios are stubbed."),
  );
  app.post("/api/quantum-resilience/scenarios", async ({ body }) =>
    entityResponse(
      { id: "quantum-resilience-scenario-stub", ...(body || {}) },
      "Scenario creation is stubbed.",
    ),
  );

  app.get("/api/quantum-resilience/controls", async () =>
    listResponse([], "Post-quantum control catalog is stubbed."),
  );
  app.post("/api/quantum-resilience/exercises", async ({ body }) =>
    entityResponse(
      { id: "quantum-resilience-exercise-stub", payload: body || {} },
      "Resilience exercises are stubbed.",
    ),
  );

  app.get("/api/quantum-resilience/readiness", async () =>
    entityResponse(
      {
        phases: ["assessment", "migration", "validation", "monitoring"],
      },
      "Quantum readiness lifecycle is stubbed.",
    ),
  );
}
