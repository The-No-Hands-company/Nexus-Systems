import { entityResponse, listResponse } from "./contracts";

export function registerBranchProtectionPoliciesRoutes(app: ForgeRouteApp) {
  app.get("/api/branch-protection/policies", async () =>
    listResponse([], "Branch protection policies"),
  );
  app.post("/api/branch-protection/create", async ({ body }) =>
    entityResponse({ id: "policy-stub", ...(body || {}) }, "Create protection policy"),
  );
  app.get("/api/branch-protection/rules", async () =>
    listResponse([], "Protection rules and enforcement"),
  );
  app.post("/api/branch-protection/enforce", async ({ body }) =>
    entityResponse({ id: "enforce-stub", payload: body || {} }, "Enforce branch protection"),
  );
  app.get("/api/branch-protection/violations", async () =>
    entityResponse({ violations: 0, enforced: true }, "Policy violations and status"),
  );
}
