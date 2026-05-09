import { entityResponse, listResponse } from "./contracts";

export function registerVendorRiskOrchestrationRoutes(app: ForgeRouteApp) {
  app.get("/api/vendor-risk/vendors", async () =>
    listResponse([], "Vendor risk registry is stubbed."),
  );
  app.post("/api/vendor-risk/vendors", async ({ body }) =>
    entityResponse(
      { id: "vendor-risk-vendor-stub", ...(body || {}) },
      "Vendor onboarding is stubbed.",
    ),
  );

  app.get("/api/vendor-risk/assessments", async () =>
    listResponse([], "Vendor risk assessments are stubbed."),
  );
  app.post("/api/vendor-risk/mitigations", async ({ body }) =>
    entityResponse(
      { id: "vendor-risk-mitigation-stub", payload: body || {} },
      "Mitigation workflow is stubbed.",
    ),
  );

  app.get("/api/vendor-risk/posture", async () =>
    entityResponse(
      {
        categories: ["security", "privacy", "resilience", "financial"],
      },
      "Vendor risk posture categories are stubbed.",
    ),
  );
}
