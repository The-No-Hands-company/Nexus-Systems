import { entityResponse, listResponse } from "./contracts";

export function registerPolicyAsCodeStudioRoutes(app: any) {
  app.get("/api/policy-code/policies", async () => listResponse([], "Policy-as-code catalog is stubbed."));
  app.post("/api/policy-code/policies", async ({ body }: any) =>
    entityResponse({ id: "policy-code-stub", ...(body || {}) }, "Policy authoring is stubbed.")
  );

  app.get("/api/policy-code/tests", async () => listResponse([], "Policy test suite registry is stubbed."));
  app.post("/api/policy-code/evaluations", async ({ body }: any) =>
    entityResponse({ id: "policy-eval-stub", payload: body || {} }, "Policy evaluation workflow is stubbed.")
  );

  app.get("/api/policy-code/bundles", async () => listResponse([], "Policy bundle publishing is stubbed."));
}
