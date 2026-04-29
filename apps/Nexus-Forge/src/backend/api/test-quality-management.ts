import { entityResponse, listResponse } from "./contracts";

export function registerTestQualityManagementRoutes(app: any) {
  app.get("/api/test-quality-management/test-cases", async () =>
    listResponse([], "Managed test cases and suites parity"));

  app.get("/api/test-quality-management/runs", async () =>
    listResponse([], "Test plans and manual/exploratory run parity"));

  app.get("/api/test-quality-management/coverage", async () =>
    entityResponse({ thresholdPolicy: "stubbed", flakiness: "stubbed", traceability: "stubbed" }, "Quality and coverage parity"));

  app.post("/api/test-quality-management/stub", async ({ body }: any) =>
    entityResponse({ id: "test-quality-stub", ...(body || {}) }, "Test and quality parity stub request accepted"));
}