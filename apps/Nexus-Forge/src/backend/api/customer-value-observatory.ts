import { entityResponse, listResponse } from "./contracts";

export function registerCustomerValueObservatoryRoutes(app: any) {
  app.get("/api/customer-value/accounts", async () => listResponse([], "Customer value account registry is stubbed."));
  app.get("/api/customer-value/outcomes", async () => listResponse([], "Customer outcome tracking is stubbed."));

  app.post("/api/customer-value/hypotheses", async ({ body }: any) =>
    entityResponse({ id: "customer-value-hypothesis-stub", ...(body || {}) }, "Value hypothesis workflow is stubbed.")
  );

  app.get("/api/customer-value/dimensions", async () =>
    entityResponse(
      {
        dimensions: ["adoption", "time-to-value", "retention", "expansion"],
      },
      "Customer value dimensions are stubbed."
    )
  );
}
