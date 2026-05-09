import { entityResponse, listResponse } from "./contracts";

export function registerPolicyAsCodeStudioRoutes(app: ForgeRouteApp) {
  app.get("/api/policy-code/policies", async () =>
    listResponse([], "Policy-as-code catalog is stubbed."),
  );
  app.post("/api/policy-code/policies", async ({ body }) =>
    entityResponse({ id: "policy-code-stub", ...(body || {}) }, "Policy authoring is stubbed."),
  );

  app.get("/api/policy-code/tests", async () =>
    listResponse([], "Policy test suite registry is stubbed."),
  );
  app.post("/api/policy-code/evaluations", async ({ body }) =>
    entityResponse(
      { id: "policy-eval-stub", payload: body || {} },
      "Policy evaluation workflow is stubbed.",
    ),
  );

  app.get("/api/policy-code/bundles", async () =>
    listResponse([], "Policy bundle publishing is stubbed."),
  );
}
