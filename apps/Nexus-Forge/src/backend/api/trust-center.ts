import { entityResponse, listResponse } from "./contracts";

export function registerTrustCenterRoutes(app: ForgeRouteApp) {
  app.get("/api/trust/certifications", async () =>
    entityResponse(
      {
        certifications: ["SOC2", "ISO27001", "PCI-DSS"],
      },
      "Trust center certification listing is stubbed.",
    ),
  );

  app.get("/api/trust/controls", async () => listResponse([], "Control mapping is stubbed."));
  app.get("/api/trust/reports", async () =>
    listResponse([], "Trust report publishing is stubbed."),
  );

  app.post("/api/trust/reports", async ({ body }) =>
    entityResponse(
      { id: "trust-report-stub", ...(body || {}) },
      "Trust report creation is stubbed.",
    ),
  );

  app.get("/api/trust/disclosures", async () =>
    listResponse([], "Security disclosure feed is stubbed."),
  );
}
