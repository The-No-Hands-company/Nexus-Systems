import { entityResponse, listResponse } from "./contracts";

export function registerProcurementRoutes(app: ForgeRouteApp) {
  app.get("/api/procurement/vendors", async () => listResponse([], "Vendor registry is stubbed."));
  app.post("/api/procurement/vendors", async ({ body }) =>
    entityResponse({ id: "vendor-stub", ...(body || {}) }, "Vendor onboarding is stubbed."),
  );

  app.get("/api/procurement/rfps", async () => listResponse([], "RFP management is stubbed."));
  app.post("/api/procurement/rfps", async ({ body }) =>
    entityResponse({ id: "rfp-stub", ...(body || {}) }, "RFP creation is stubbed."),
  );

  app.get("/api/procurement/contracts", async () =>
    listResponse([], "Procurement contracts are stubbed."),
  );
}
