import { entityResponse, listResponse } from "./contracts";

export function registerQualityRoutes(app: ForgeRouteApp) {
  app.get("/api/quality/test-plans", async () =>
    listResponse([], "Test plan management is stubbed."),
  );
  app.post("/api/quality/test-plans", async ({ body }) =>
    entityResponse({ id: "test-plan-stub", ...(body || {}) }, "Test plan creation is stubbed."),
  );

  app.get("/api/quality/test-runs", async () => listResponse([], "Test run history is stubbed."));
  app.post("/api/quality/test-runs", async ({ body }) =>
    entityResponse({ id: "test-run-stub", ...(body || {}) }, "Test run scheduling is stubbed."),
  );

  app.get("/api/quality/flaky-tests", async () =>
    listResponse([], "Flaky test analytics is stubbed."),
  );
  app.get("/api/quality/coverage", async () =>
    entityResponse({ overall: 0, byPackage: [] }, "Coverage analytics are stubbed."),
  );
}
