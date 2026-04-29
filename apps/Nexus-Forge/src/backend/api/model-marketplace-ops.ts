import { entityResponse, listResponse } from "./contracts";

export function registerModelMarketplaceOpsRoutes(app: any) {
  app.get("/api/model-marketplace/listings", async () => listResponse([], "Model marketplace listings are stubbed."));
  app.post("/api/model-marketplace/listings", async ({ body }: any) =>
    entityResponse({ id: "model-listing-stub", ...(body || {}) }, "Model listing publication is stubbed.")
  );

  app.get("/api/model-marketplace/licenses", async () => listResponse([], "Model licensing registry is stubbed."));
  app.post("/api/model-marketplace/evaluations", async ({ body }: any) =>
    entityResponse({ id: "model-eval-stub", ...(body || {}) }, "Model evaluation workflow is stubbed.")
  );

  app.get("/api/model-marketplace/trust", async () => listResponse([], "Model trust and provenance records are stubbed."));
}
