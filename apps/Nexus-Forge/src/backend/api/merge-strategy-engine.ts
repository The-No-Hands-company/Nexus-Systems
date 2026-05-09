import { entityResponse, listResponse } from "./contracts";

export function registerMergeStrategyEngineRoutes(app: ForgeRouteApp) {
  app.get("/api/merge/strategies", async () => listResponse([], "Available merge strategies"));
  app.post("/api/merge/analyze", async ({ body }) =>
    entityResponse({ id: "merge-stub", ...(body || {}) }, "Analyze merge compatibility"),
  );
  app.get("/api/merge/conflicts", async () => listResponse([], "Active merge conflicts"));
  app.post("/api/merge/resolve", async ({ body }) =>
    entityResponse({ id: "resolve-stub", payload: body || {} }, "Resolve merge conflict"),
  );
  app.get("/api/merge/history", async () =>
    entityResponse(
      { strategies: ["fast-forward", "three-way", "squash", "rebase"] },
      "Merge history",
    ),
  );
}
