import { entityResponse, listResponse } from "./contracts";

export function registerContractTestingExchangeRoutes(app: any) {
  app.get("/api/contract-testing/contracts", async () => listResponse([], "Contract testing contract registry is stubbed."));
  app.post("/api/contract-testing/contracts", async ({ body }: any) =>
    entityResponse({ id: "contract-testing-stub", ...(body || {}) }, "Contract test definition workflow is stubbed.")
  );

  app.get("/api/contract-testing/suites", async () => listResponse([], "Contract test suite index is stubbed."));
  app.post("/api/contract-testing/executions", async ({ body }: any) =>
    entityResponse({ id: "contract-execution-stub", payload: body || {} }, "Contract test execution is stubbed.")
  );

  app.get("/api/contract-testing/compatibility", async () => listResponse([], "Compatibility matrix feed is stubbed."));
}
