import { entityResponse, listResponse } from "./contracts";

export function registerIncidentPreventionGraphRoutes(app: any) {
  app.get("/api/incident-prevention/nodes", async () => listResponse([], "Incident prevention graph nodes are stubbed."));
  app.post("/api/incident-prevention/nodes", async ({ body }: any) =>
    entityResponse({ id: "incident-prevention-node-stub", ...(body || {}) }, "Prevention node creation is stubbed.")
  );

  app.get("/api/incident-prevention/edges", async () => listResponse([], "Incident prevention dependencies are stubbed."));
  app.post("/api/incident-prevention/simulations", async ({ body }: any) =>
    entityResponse({ id: "incident-prevention-sim-stub", payload: body || {} }, "Prevention simulation runs are stubbed.")
  );

  app.get("/api/incident-prevention/hotspots", async () => listResponse([], "Prevention hotspot analysis is stubbed."));
}
