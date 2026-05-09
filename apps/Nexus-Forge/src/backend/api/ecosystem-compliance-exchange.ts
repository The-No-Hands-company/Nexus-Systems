import { entityResponse, listResponse } from "./contracts";

export function registerEcosystemComplianceExchangeRoutes(app: ForgeRouteApp) {
  app.get("/api/ecosystem-compliance/partners", async () =>
    listResponse([], "Compliance partner registry is stubbed."),
  );
  app.get("/api/ecosystem-compliance/controls", async () =>
    listResponse([], "Shared ecosystem controls are stubbed."),
  );

  app.post("/api/ecosystem-compliance/attestations", async ({ body }) =>
    entityResponse(
      { id: "ecosystem-attestation-stub", ...(body || {}) },
      "Ecosystem attestation exchange is stubbed.",
    ),
  );

  app.get("/api/ecosystem-compliance/posture", async () =>
    entityResponse(
      {
        domains: ["privacy", "security", "resilience", "regulatory"],
      },
      "Ecosystem compliance posture is stubbed.",
    ),
  );
}
