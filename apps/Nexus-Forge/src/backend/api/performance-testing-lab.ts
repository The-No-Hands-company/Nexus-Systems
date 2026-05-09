import { entityResponse, listResponse } from "./contracts";

export function registerPerformanceTestingLabRoutes(app: ForgeRouteApp) {
  app.get("/api/perf-testing/benchmarks", async () => listResponse([], "Performance benchmarks"));
  app.post("/api/perf-testing/create", async ({ body }) =>
    entityResponse({ id: "bench-stub", ...(body || {}) }, "Create performance test"),
  );
  app.get("/api/perf-testing/results", async () => listResponse([], "Test execution results"));
  app.post("/api/perf-testing/execute", async ({ body }) =>
    entityResponse({ id: "exec-stub", payload: body || {} }, "Execute performance test"),
  );
  app.get("/api/perf-testing/regressions", async () =>
    entityResponse({ regressions: 0, baseline: "current" }, "Performance regression analysis"),
  );
}
