import { entityResponse, listResponse } from "./contracts";

export function registerWorkflowMarketplaceRoutes(app: ForgeRouteApp) {
  app.get("/api/workflow-marketplace/listings", async () =>
    listResponse([], "Workflow marketplace listings are stubbed."),
  );
  app.post("/api/workflow-marketplace/listings", async ({ body }) =>
    entityResponse(
      { id: "workflow-listing-stub", ...(body || {}) },
      "Workflow listing publishing is stubbed.",
    ),
  );

  app.get("/api/workflow-marketplace/subscriptions", async () =>
    listResponse([], "Workflow subscription ledger is stubbed."),
  );
  app.post("/api/workflow-marketplace/purchases", async ({ body }) =>
    entityResponse(
      { id: "workflow-purchase-stub", ...(body || {}) },
      "Workflow marketplace purchase flow is stubbed.",
    ),
  );

  app.get("/api/workflow-marketplace/ratings", async () =>
    listResponse([], "Workflow ratings and reviews are stubbed."),
  );
}
