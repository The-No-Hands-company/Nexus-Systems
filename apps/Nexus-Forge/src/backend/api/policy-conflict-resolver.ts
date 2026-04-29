import { entityResponse, listResponse } from "./contracts";

export function registerPolicyConflictResolverRoutes(app: any) {
  app.get("/api/policy-conflicts/conflicts", async () => listResponse([], "Policy conflicts are stubbed."));
  app.post("/api/policy-conflicts/conflicts", async ({ body }: any) =>
    entityResponse({ id: "policy-conflict-stub", ...(body || {}) }, "Conflict registration is stubbed.")
  );

  app.get("/api/policy-conflicts/rules", async () => listResponse([], "Conflict resolution rulebook is stubbed."));
  app.post("/api/policy-conflicts/resolutions", async ({ body }: any) =>
    entityResponse({ id: "policy-resolution-stub", payload: body || {} }, "Conflict resolution workflow is stubbed.")
  );

  app.get("/api/policy-conflicts/strategies", async () =>
    entityResponse(
      {
        strategies: ["precedence", "risk-weighted", "human-review", "defer"],
      },
      "Policy conflict resolution strategies are stubbed."
    )
  );
}
