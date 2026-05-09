import { entityResponse, listResponse } from "./contracts";

export function registerMultiCloudFailoverMeshRoutes(app: ForgeRouteApp) {
  app.get("/api/failover-mesh/topologies", async () =>
    listResponse([], "Failover mesh topologies are stubbed."),
  );
  app.post("/api/failover-mesh/topologies", async ({ body }) =>
    entityResponse(
      { id: "failover-mesh-topology-stub", ...(body || {}) },
      "Topology authoring is stubbed.",
    ),
  );

  app.get("/api/failover-mesh/exercises", async () =>
    listResponse([], "Failover exercises are stubbed."),
  );
  app.post("/api/failover-mesh/activations", async ({ body }) =>
    entityResponse(
      { id: "failover-mesh-activation-stub", payload: body || {} },
      "Failover activation is stubbed.",
    ),
  );

  app.get("/api/failover-mesh/posture", async () =>
    entityResponse(
      {
        states: ["ready", "degraded", "recovering", "stable"],
      },
      "Failover mesh posture states are stubbed.",
    ),
  );
}
