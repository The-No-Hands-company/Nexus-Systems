import { entityResponse, listResponse } from "./contracts";

export function registerBillingCoreRoutes(app: any) {
  app.get("/api/billing/plans", async () => listResponse([], "Billing plan catalog is stubbed."));
  app.post("/api/billing/plans", async ({ body }: any) =>
    entityResponse({ id: "billing-plan-stub", ...(body || {}) }, "Billing plan management is stubbed.")
  );

  app.get("/api/billing/invoices", async () => listResponse([], "Invoice ledger is stubbed."));
  app.post("/api/billing/invoices", async ({ body }: any) =>
    entityResponse({ id: "invoice-stub", ...(body || {}) }, "Invoice generation is stubbed.")
  );

  app.get("/api/billing/meters", async () =>
    entityResponse(
      {
        dimensions: ["storage", "compute", "api-calls", "seats"],
      },
      "Usage metering dimensions are stubbed."
    )
  );
}
