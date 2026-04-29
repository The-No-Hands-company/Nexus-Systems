import { entityResponse, listResponse } from "./contracts";

export function registerCapabilityMaturityRadarRoutes(app: any) {
  app.get("/api/maturity/domains", async () => listResponse([], "Capability maturity domains are stubbed."));
  app.post("/api/maturity/assessments", async ({ body }: any) =>
    entityResponse({ id: "maturity-assessment-stub", ...(body || {}) }, "Maturity assessment capture is stubbed.")
  );

  app.get("/api/maturity/roadmaps", async () => listResponse([], "Maturity roadmap planning is stubbed."));
  app.get("/api/maturity/radar", async () =>
    entityResponse(
      {
        bands: ["ad-hoc", "managed", "defined", "optimized"],
      },
      "Capability maturity radar bands are stubbed."
    )
  );
}
